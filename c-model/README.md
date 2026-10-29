# Tiny NPU: 4 x 4 weight-stationary array

Implements pages **01 and 06** of `Tiny-NPU_drawio_merged.xml`.
Pages 02–04 describe the older two-lane baseline; they are not the active schedule.
The network is **4 inputs → 4 hidden neurons → 4 output neurons**: two 4x4
weight matrices, two length-4 INT32 bias vectors, and one shared 16-MAC array
reused across the two layers. All parameters are explicitly **placeholders**.

## Build and logging

```sh
make run LOG=flow    # transfers, each active MAC, arithmetic, FIFO and stores
make run LOG=ports   # all block ports and all 16 cell registers each cycle
make run LOG=both
make run LOG=none
make log LOG=flow    # writes flow.log
make log LOG=ports   # writes ports.log
```

No HLS installation is required to build the C simulation. GCC/Clang and make
are sufficient. The build keeps each logging variant in a separate directory.
Errors always go to stderr. `make clean` removes build products.

## Files and responsibilities

| File | Responsibility |
|---|---|
| `main.c` | Initialize the NPU; run cycles; return completion/error status |
| `npu.c` | Assemble shared port connections, choose layer configuration, check faults |
| `bus.c/.h` | Generic callback traversal and registered/wire transport |
| `datapath.h` | Typed signal bundles and explicit packed word encoding |
| `controller.c` | Decode REQ/CAP/C1–C7/POST/STORE; sequence both layers |
| `fetcher.c` | Capture registered SRAM response; route W, bias, or packed x |
| `sram.c` | Single-port memory, registered reads, packed writes, placeholder parameters |
| `mac.c` | 16 stationary weights, registered downward x and rightward partial sums |
| `activation.c` | Four combinational ReLU/bypass units |
| `scale_clamp.c` | Four arithmetic shifts followed by signed INT8 clamps |
| `fifo.c` | Depth-one packed result register, valid flag, push and pop |
| `logging.c` | Shared flow and port formatting |

Every block registers one callback with the generic bus scheduler. Traversal is
**controller → fetcher → SRAM → MAC array → activation → scale/clamp → FIFO**.
There is no controller-specific loop or wiring in `main.c`.

## SRAM map

Addresses are **32-bit word indices**, not byte addresses. SRAM has 4096 words.

| Word address | Contents |
|---|---|
| `0x000` | Activation A: initial input; replaced by final layer output |
| `0x100`–`0x103` | Layer 1: four packed weight rows |
| `0x104`–`0x107` | Layer 2: four packed weight rows |
| `0x200`–`0x203` | Layer 1: four signed INT32 biases |
| `0x204`–`0x207` | Layer 2: four signed INT32 biases |
| `0x300` | Activation B: hidden-layer output, then input to layer 2 |

For `W[output][input]`, a weight row occupies one word:
`bits[7:0]=W[r][0]`, `bits[15:8]=W[r][1]`,
`bits[23:16]=W[r][2]`, `bits[31:24]=W[r][3]`.
The input and output vectors use the same LSB-first byte packing.
Each bias occupies a whole INT32 word, stored in two's complement.
Packing uses shifts, so host byte order does not determine the SRAM layout.

### Replace trained parameters here

Edit `placeholder_weights[2][4][4]`, `placeholder_biases[2][4]`, and
`placeholder_input[4]` in **sram.c**. Edit per-layer `shift` and `relu` in
**npu.c**. The runtime reads the parameters from SRAM through the real modeled
REQ/CAP path; MAC initialization does not preload weights or biases directly.

The current example uses the page-06 weights/biases for layer 1 and a simple
second-layer placeholder:

```
x  = [10,20,30,40]
W1 = [[1,2,3,4], [5,6,7,8], [-1,-2,-3,-4], [2,0,2,0]]
b1 = [4,-20,10,8]
W2 = [[1,1,0,0], [0,1,1,0], [0,0,1,1], [1,0,0,1]]
b2 = [0,4,-4,8]
```

Layer 1 uses ReLU, shift 2. Layer 2 uses bypass, shift 2. Both activation
choices are placeholders, not assumptions about the trained model.
Floating-point training weights require a defined quantization scale before
replacement: weights are signed INT8, biases are INT32 in input×weight units,
and each output conversion uses a power-of-two shift with zero point zero.
This model does not silently cast floating-point weights to INT8.

## Exact edge schedule

Cycle 1 is the first setup edge **after host start/configuration acceptance**;
there is no extra simulated IDLE/start edge in the reported latency.

| Layer 1 edges | Layer 2 edges | Accepted operation |
|---|---|---|
| 1–8 | 28–35 | Four weight row reads, REQ then CAP |
| 9–16 | 36–43 | Four bias reads, REQ then CAP |
| 17–18 | 44–45 | Packed input vector read, REQ then CAP |
| 19–25 | 46–52 | C1–C7: wavefront compute |
| 26 | 53 | POST: combinational ReLU/shift/clamp; FIFO captures four bytes |
| 27 | 54 | STORE: SRAM writes one packed word; FIFO becomes empty |

A layer costs **27 edges**, including setup. This two-layer inference costs
**54 edges**, with 18 reads, 2 writes, and 32 useful MAC updates.
The next layer starts only after the previous packed STORE has completed.
Each new layer reloads its weights and biases into the same physical array.
The diagram's 11-edge repeated-vector optimization and overlapping vectors
are not exposed in this version; it executes one vector through two layers.

Using zero-based row/column indices, cell `(r,c)` fires when
`wave == r+c+1`. Its update is:

```
x_in    = (r == 0) ? input[c] : old_cell[r-1][c].x
psum_in = (c == 0) ? bias[r]  : old_cell[r][c-1].psum
cell[r][c].x    = x_in
cell[r][c].psum = psum_in + W[r][c] * x_in
```

The old registers of **all 16 cells are snapshotted first**. A cell cannot
read its neighbor's newly computed value on the same edge. The loop over
cells evaluates spatial hardware in software; it does not advance the clock.
Active cell counts are **1, 2, 3, 4, 3, 2, 1** over C1–C7.
Rows finish at C4, C5, C6, C7 and hold their completed sums until POST.

## Bus and timing contract

The bus stays generic. It knows payload sizes, producer/consumer traversal,
and whether a link is registered or combinational; it knows no NPU opcodes.

- `bus_init`: registered link. Writes become visible after traversal commits;
  a consumer captures them on the following edge. SRAM read data and the
  array row-sum bundle use this mode.
- `bus_init_wire`: combinational link. A producer's write is visible to later
  callbacks during the same traversal. Its valid strobe clears at the start
  of every cycle. The controller decoder, fetch load routing, activation,
  and scaling use this mode.
- FIFO data/valid are registered. POST pushes; STORE on the following edge
  sees the entry, writes SRAM, and pops it.

Immediate wires are necessary here: controller controls must be stable for
that modeled edge, and activation/scale must feed the FIFO POST capture
without adding extra pipeline registers absent from the diagram. They model
combinational logic, not an asynchronous clock or another clock edge.
**Callback order is therefore a dependency order** and cannot be arbitrarily
reversed. There are no combinational cycles. One producer may write a link
at most once per cycle. Registered data bits persist; a valid event does not.

Control bundles use typed C structs for simulator wiring. Their raw byte
port logs are diagnostics, not a packed RTL interface or SRAM word encoding.
Controller logs decode the control fields, and array logs show every cell's
weight, x, partial sum, and valid flag. `WIRE` denotes same-cycle values;
`CURRENT` and `NEXT` denote registered values around the edge. Per-block
port dumps are taken at that callback's position in traversal.

## Arithmetic and expected placeholder result

Products are signed INT8×INT8 → INT16, sign-extended into INT32 sums.
There is **no saturation inside the MAC**, following the new diagram.
The simulator reports an error if any partial sum exceeds INT32 range.
Biases must be chosen so every intermediate sum fits.

Requantization is floor division by `2^shift` followed by clamp to
`[-128,127]`; it is not the previous nearest-rounding behavior. Negative
values are handled portably without depending on C signed-right-shift rules.

| Stage | Values |
|---|---|
| Layer 1 registered sums | `[304,680,-290,88]` |
| Hidden vector in SRAM B | `[76,127,0,22]`, packed `0x16007F4C` |
| Layer 2 registered sums | `[203,131,18,106]` |
| Final vector in SRAM A | `[50,32,4,26]`, packed `0x1A042032` |

This is an edge-defined C architectural simulator. The bus scheduler,
callbacks, logs, and software overflow diagnostics are simulation scaffolding,
not a synthesizable HLS top. Datapath behavior can be translated into HLS/RTL,
but the 27-edge schedule has not been verified against synthesized hardware.
No DMA, AXI, stalls, instruction execution, automatic tiling, or actual trained
sine-network accuracy is claimed.
