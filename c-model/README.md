# Tiny NPU — 128-bit SRAM and direct operand capture

Implements the wide-local-SRAM architecture in the supplied draw.io drawing.
The 4→4→4 neural network and quantization arithmetic are retained. The array
still has 16 weight-stationary MAC cells with registered x/partial-sum links.

**Changes from the previous model:**

- SRAM is 1024 lines × 128 bits (16 KiB), with 16 byte-write enables.
- Addresses are now **BYTE addresses**, aligned to 16 bytes.
- `fetcher.c/.h` are removed; there is no FetchLoad bus or fetcher callback.
- Controller acts as a tile sequencer/AGU, issuing four consecutive reads.
- The SRAM response bus connects directly to both sequencer and MAC array.
- All 16 weights capture on one edge; all four biases capture on another.
- Input/bias capture is inside mac.c; configuration capture is in controller.c.
  These represent distributed destination registers, not additional pipeline stages.
- Inference takes **28 simulated cycles**, down from 66, with 8 wide reads,
  2 masked writes, and 32 useful MAC updates.

No new C/header block files were added. Existing block separation is retained;
only the now-unnecessary fetcher files were removed.

## Build and terminal TB

```sh
make run TB=1 LOG=flow   # cycle/transfer/arithmetic flow, then result and wait
make run TB=1 LOG=ports  # every block port and all cell registers each cycle
make run TB=1 LOG=both
make run TB=1 LOG=none   # terminal prompts and results only
make run TB=0 LOG=flow   # default: one inference without interactive TB
make log TB=0 LOG=flow   # save flow.log
```

Enter four raw signed INT8 values, e.g. `1 2 3 4`. Enter `q` or EOF to exit.
Each vector resets the model, restores its parameters, preloads the input,
runs inference, displays the result, then waits without advancing cycles.
Invalid input is rejected before execution. TB=0 does not compile/link tb.c
or tb_main.c. Builds are isolated under build/tb0 and build/tb1 by log mode.
No HLS tools are needed to compile this C99 simulation.

With the bundled header, `[1,2,3,4]` gives `[0,1,-4,-1]`, packed `0xFFFC0100`.
Compare to Python `inference_raw_int8([1,2,3,4], params)`. Real float features
need host preprocessing/quantization first, as in the updated notebook.

## Files / reading order

| File | Responsibility |
|---|---|
| main.c | Initialize and run model; no block-specific traversal |
| npu.c/.h | Own instances, wire shared ports, register blocks, check completion/errors |
| bus.c/.h | Generic registered/wire links and cycle traversal (unchanged) |
| datapath.h | Typed signals, Line128 and packing helpers |
| controller.c/.h | Pipeline SRAM requests/capture tags, capture config, enable compute/POST/STORE |
| sram.c/.h | 128-bit native memory behavior, byte masks and parameter preload |
| mac.c/.h | Direct capture into PE weights/local bias/x; unchanged wavefront arithmetic |
| activation.c/.h | Four ReLU/bypass lanes (unchanged) |
| scale_clamp.c/.h | Integer M/S requantization (unchanged) |
| fifo.c/.h | One 32-bit vector held for a masked 128-bit SRAM write |
| logging.c/.h | Flow/port formatting, including 128-bit Q/WDATA |
| tb.c/.h, tb_main.c | Optional host terminal interaction and SRAM input preload |
| trained_npu_params.h | Exported weights, accumulator-unit biases, M/S and ReLU |

The CPU box in the architecture is optional. This model implements the tile
FSM/AGU, **not** a CPU, instruction decoder or real vector/matrix ISA. Default
layer base-address commands are configured by npu_init. A future CPU can submit
those commands to the same local sequencer.

## 128-bit layout / BYTE address map

`Line128.lane[0]` is bits31:0; lane[3] is bits127:96. It is an explicit logical
value built from four uint32_t lanes, not a compiler-specific 128-bit integer.
Within a lane, INT8 values pack LSB first. Packing is independent of host endian.

| Byte base | Line contents |
|---|---|
| 0x0000 | Activation A: initial x in lane0; final q overwrites lane0 |
| 0x0100 | Layer 1 W: four packed weight rows, one row per lane |
| 0x0110 | Layer 2 W: four packed weight rows |
| 0x0200 | Layer 1 biases: four INT32 lanes |
| 0x0210 | Layer 2 biases: four INT32 lanes |
| 0x0300 | Hidden vector B in lane0 |
| 0x0400 | Layer 1 CFG: lane0=M, lane1=S, lane2=ReLU, lane3=reserved zero |
| 0x0410 | Layer 2 CFG: same format |

Memory line index is `byte_address / 16`. Misalignment and out-of-range
accesses are errors. These addresses/formats intentionally differ from the old
32-bit word-addressed model; do not reuse its numeric indices into the array.

M must be 1..INT32_MAX; S must be 0..62; ReLU must be 0 or 1; reserved=0.
All four configuration fields are validated before computation.

Replace the entire `trained_npu_params.h` with your notebook export, then run
make. sram.c packs it into the wide format automatically. Do not separately
edit numerical constants inside controller/mac/scale. The bundled header and
JSON metadata/reference vectors come from the validated notebook run and must
be kept together; your local training run may export slightly different values.

## Direct routing: no fetch/copy stage

`npu.c` passes **the same &sram.response_bus** to the sequencer and MAC array.
Both read current SRAM Q. The sequencer sends only control signals, not a copy
of the operand payload. Captures are:

```
CAP_W:   cell[r][c].weight = unpack_byte(Q.lane[r], c)  // all 16 on one edge
CAP_B:   bias[r] = signed_INT32(Q.lane[r])             // all 4 on one edge
CAP_CFG: controller captures M, S and ReLU from Q
CAP_X:   input_word = Q.lane[0]
```

The generic Bus is simulation transport; it is not AXI or a shared arbitrated
interconnect. Destination registers are ordinary persistent C fields. Copies
from the shared Q payload into these fields represent register captures; they
do not add another simulated clock or an intermediate fetcher pipeline.

## Exact cycle schedule

| Operation | Layer 1 | Layer 2 |
|---|---|---|
| Issue W read | 1 | 15 |
| Capture all W; issue B read | 2 | 16 |
| Capture all biases; issue CFG read | 3 | 17 |
| Capture M/S/ReLU; issue X read | 4 | 18 |
| Capture x | 5 | 19 |
| C1..C7 wavefront | 6..12 | 20..26 |
| POST: activation, scaling, FIFO capture | 13 | 27 |
| STORE: masked SRAM write, FIFO pop | 14 | 28 |

One callback traversal equals one simulated clock update:
**Controller → SRAM → MAC array → Activation → ScaleClamp → FIFO**.
SRAM writes its NEXT Q during a read; consumers still see CURRENT Q from the
previous cycle, even when SRAM's callback runs before the MAC callback. Thus
capturing W while issuing B cannot accidentally capture the new bias data.

Control is a same-cycle wire. SRAM Q, array row sums and FIFO data/valid are
registered links. Activation and quantized outputs are same-cycle wires at
POST, feeding the FIFO capture. Generic bus timing/framework is unchanged.
Destination tags are one-cycle delayed by the sequencer's edge decoder; the
fixed-latency memory accepts one request each clock without stalls.

All cells use a snapshot of OLD neighbour registers. Cell(r,c), one-based,
updates at C(r+c-1). Active cell counts remain 1,2,3,4,3,2,1. Final row sums
are registered before POST. Layer 2 reads hidden B only after STORE1.

STORE supplies 128-bit WDATA, with q in lane0, and byte_enable=0x000F. Only the
low four bytes change; upper96 of the destination line are preserved. Native
per-byte writes are assumed; no read-modify-write cycles are hidden.
Eight 16-byte reads fetch 128 bytes including padding/reserved lanes. The
improvement is fewer/wider access cycles, not less total transferred data.

## Numerical contract / limits

INT8 x INT8 -> INT16 product; signed INT32 partial sums and biases. No MAC
saturation; overflow is an error. ReLU/bypass precedes requantization:

`q = clamp_int8(round_nearest_ties_even((int64_t)activated * M / 2^S))`.

All arithmetic/rounding matches the updated Python integer reference. The
input/output scales in the header/JSON are host metadata; datapath uses no
floating-point calculations.

This is a cycle-defined architectural C simulation, **not synthesized RTL**.
28 cycles assumes a one-cycle SRAM response, one request/clock, no stalls,
and a one-stage POST path. A physical memory macro or the INT64 requantizer
may require more pipeline stages or a different clock period. There is no
claim of measured FPGA speedup. No CPU ISA, AXI, DMA, variable dimensions,
automatic tiling, persistent two-layer parameter cache, overlapping vectors,
or hidden-vector bypass is implemented. The optional CPU remains future work.

The separately supplied wide-SRAM draw.io architecture remains the hardware
reference. This C release now implements its 28-cycle baseline; RTL timing
still requires synthesis and validation.
