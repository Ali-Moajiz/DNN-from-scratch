# Tiny NPU: 4x4 array with model-specific integer requantization

Two-layer **4 → 4 → 4** network, one 16-cell weight-stationary array, single-port
SRAM, ReLU/bypass, M/S scale/clamp, and depth-one packed result FIFO.
The MAC array, activation arithmetic, FIFO and generic bus framework are
unchanged from the previous model. Configuration loading and output scaling
are updated to match the revised Python notebook.

## Build / terminal testbench

```sh
make run TB=1 LOG=flow   # enter 4 raw INT8 values; see cycles, result, then wait
make run TB=1 LOG=ports  # every block port / all cell registers each cycle
make run TB=1 LOG=both
make run TB=1 LOG=none   # terminal prompts/results only
make run TB=0 LOG=flow   # one default inference; default build is TB=0
make log TB=0 LOG=flow  # save flow.log
```

Enter, for example, `1 2 3 4`; `q` or EOF exits. Each accepted vector starts a
fresh model, preloads only its input word, and runs **66 cycles**. No clock
cycles advance while waiting for input. Invalid input is rejected before any
inference. TB=0 excludes `tb.c` and `tb_main.c`; the original `main.c` remains
the baseline entry point. Separate build/tb0 and build/tb1 directories also
separate logging modes, so switching flags needs no clean.

Input values are **already quantized INT8**, not original feature measurements
or preprocessed floats. Compare with Python:

```python
inference_raw_int8([1, 2, 3, 4], params)
```

For model-input floats, Python must first apply its fixed input scale. For
original dataset measurements, it must also apply the notebook's fitted
StandardScaler and feature gain. The C TB does neither of these host-side
float operations. It prints the final raw INT8 vector; multiply by the
exported output scale on the host to interpret approximate real logits.

## Replace trained parameters

`trained_npu_params.h` is included directly by **sram.c**. It contains:

- `trained_weights[2][4][4]`: signed INT8, W[output][input].
- `trained_biases[2][4]`: signed INT32, already in accumulator units.
- `requant_multiplier[2]`: positive signed INT32 M.
- `requant_shift[2]`: unsigned S, range 0..62.
- `relu_enable[2]`: 0=bypass, 1=ReLU.

Replace this whole header with the file exported by your updated notebook,
then run make. Dependency tracking recompiles sram.c when the header changes.
Do not mix weights, biases or M/S from different training/export runs.

The bundled header comes from the validated notebook run: training-only
preprocessing, seed 42, 300 epochs, same four-feature dataset. It may differ
from your local training run; use YOUR complete exported header to match YOUR
Python predictions. `npu_preprocessing.json` and `npu_reference_vectors.json`
in this project correspond to the bundled header only. Float scale metadata
is for host interpretation; the NPU datapath does not use floating point.

With the bundled header, raw `[1,2,3,4]` produces `[0,1,-4,-1]` at SRAM A,
packed word `0xFFFC0100`. The earlier `[19,34,-127,-35]` used dynamic output
normalization and a different export; it is not the reference for this model.

## Runtime SRAM map (WORD addresses)

| Region | Word addresses | Stored values |
|---|---|---|
| Activation A | 0x000 | Initial packed input; overwritten by final output |
| Layer 1 weights | 0x100..0x103 | Four packed INT8 rows |
| Layer 2 weights | 0x104..0x107 | Four packed INT8 rows |
| Layer 1 biases | 0x200..0x203 | Four full INT32 values |
| Layer 2 biases | 0x204..0x207 | Four full INT32 values |
| Activation B | 0x300 | Packed hidden vector |
| Layer 1 configuration | 0x400, 0x401, 0x402 | M, S, ReLU |
| Layer 2 configuration | 0x403, 0x404, 0x405 | M, S, ReLU |

Each INT8 row/vector packs four bytes LSB first: element 0 in bits 7:0.
Biases use two's complement. SRAM has 4096 words and one access per edge.
Initialization only preloads SRAM; runtime parameters are not loaded directly
into controller or MAC registers by npu_init.

## What changed and where

| File | Change |
|---|---|
| datapath.h | Config base address, LOAD_CONFIG, M in Control, schedule constants |
| sram.c/.h | Exported header preload; six configuration words |
| fetcher.c/.h | Separate registered CONFIG_RESPONSE bus to controller |
| controller.c/.h | Read M/S/ReLU, validate/capture config, drive active settings |
| scale_clamp.c/.h | INT64 multiply, nearest/ties-even shift, signed INT8 clamp |
| npu.c | Wire config feedback; configure layer memory bases |
| main.c / tb.c | Use inference cycle constant; updated parameter reporting |

There are no TB-specific branches in hardware blocks.

## Configuration path and cycle timing

Traversal is unchanged:
**Controller → Fetcher → SRAM → MacArray → Activation → ScaleClamp → FIFO**.
All blocks share Bus pointers, and each registers one callback.

Control, FetchLoad to the array, activation and quantized outputs are
combinational wires: later callbacks can read same-cycle writes. SRAM read
data, array row sums, FIFO data/valid, and the NEW config-response bus are
registered: writes become visible after traversal, for the following cycle.

The controller executes before Fetcher, so configuration feedback is registered:

1. Cycle 1: Controller requests M; SRAM stages response.
2. Cycle 2: Fetcher captures it and stages CONFIG_RESPONSE.
3. Cycle 3: Controller captures M while requesting S.
4. Cycle 4: Fetcher captures S.
5. Cycle 5: Controller captures S while requesting ReLU.
6. Cycle 6: Fetcher captures ReLU.
7. Cycle 7: Controller captures ReLU while requesting weight row 0.

Thus capturing config overlaps the next request and adds exactly six cycles
per layer. All configuration is validated before computation: M=1..INT32_MAX,
S=0..62, ReLU=0/1. Invalid/missing configuration stops the model with an error.

| Operation | Layer 1 cycles | Layer 2 cycles |
|---|---|---|
| M/S/ReLU: 3 REQ/CAP pairs | 1..6 | 34..39 |
| Four weight row REQ/CAP pairs | 7..14 | 40..47 |
| Four bias REQ/CAP pairs | 15..22 | 48..55 |
| Packed input REQ/CAP | 23..24 | 56..57 |
| Array C1..C7 | 25..31 | 58..64 |
| POST: activation, scaling, FIFO capture | 32 | 65 |
| STORE: packed SRAM write, FIFO pop | 33 | 66 |

Total: 33 edges per layer, **66 cycles per inference**, 24 SRAM reads,
2 writes, 32 useful MAC updates. Cycle 1 is the first setup edge after the
host configuration/start boundary; no separate start/IDLE edge is counted.

## Exact numerical contract

Zero points are zero. For each output row:

```
acc = bias_int32 + sum(weight_int8 * input_int8)
activated = relu_enable ? max(acc, 0) : acc
product_int64 = activated * M
rounded = round_nearest_ties_even(product_int64 / 2^S)
q = clamp(rounded, -128, 127)
```

Bias export is `round(float_bias / (input_scale * weight_scale))`.
M/2^S approximates `input_scale * weight_scale / output_scale`.
Python and C use the same integer M/S and the same signed rounding rule.
Examples: +2.5→2, +3.5→4, -2.5→-2, -3.5→-4.
There is no MAC saturation: partial sums must fit INT32 or the simulator errors.
INT32×positive-INT32 products safely fit INT64. Scale/clamp avoids shifting
negative signed integers and avoids overflow from adding a rounding bias.

The array still snapshots all old cells before wavefront updates. Cell (r,c),
using one-based indices, fires at C(r+c-1); x moves down and partial sums right.
Activation and scaling remain combinational at POST. The wider scaling path
may need pipelining or a longer clock period in actual HLS/RTL, changing the
schedule. **66 cycles is this architectural C schedule, not synthesized timing.**

No DMA, stalls, AXI, instruction fetch, variable shapes, automatic tiling,
streaming overlapping vectors, or repeated-vector shortcut is implemented.
The function-pointer scheduler/logging are software simulation scaffolding,
not a synthesizable HLS top.
