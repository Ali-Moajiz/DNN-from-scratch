# Generic bus traversal and controller module

## Files

- `bus.h` / `bus.c`: generic shared links, packet read/write, module registration,
  cycle traversal and publication. No NPU-specific signal definitions.
- `controller.h` / `controller.c`: controller ports, persistent state and the
  three internal phases of its single registered handler.
- `main.c`: initialize network, initialize/start controller, advance cycles.
- `Makefile`: build/run/clean. No demos, mocks or test routines are included.

```sh
make run
make clean
```

## Architecture

`controller_init()` creates the controller's five typed port links, registers
those links, and registers ONE handler: `controllerBusses()`.

On each call to `bus_run_cycle(&network)`, generic traversal calls every
registered module's handler. The controller's handler performs:

```c
controllerbusRead(controller);
controllerProcess(controller);
controllerbusWrite(controller);
```

These functions are private to controller.c. Main and bus.c know nothing about
how the controller reads, processes or writes. All controller signal handling
lives inside that module. Future blocks follow exactly the same pattern.

A module stores its own state and pointers to its shared links. The controller
owns link storage in `controller.buses`; `controller.ports` exposes pointers to
it. A future fetcher can store `controller.ports.fetch_data` as its input link.
The fetcher's init creates/registers only its OWN new output links and registers
its own handler. Do not initialize or register an existing shared link again.
Keep initialized modules at stable addresses: do not copy a Controller struct,
because its ports point to its embedded link storage.

## Who moves the data?

`bus_read()` copies a published packet into a block's private input fields.
`bus_write()` stages that block's output packet for the next cycle. The traversal
owns publication: after ALL handlers finish, all links update together.

Thus a producer's write in cycle N is visible to a consumer in cycle N+1. The
callbacks may run in any registration order without same-cycle forwarding.
Each handler can read/process/write in order without seeing another block's
partially updated signals. Values persist until overwritten. Packet validity
is a one-cycle event; without valid, old payload bits are not a new request.
Reads do not consume events. Each link must have one producer.

This is a registered bus model, not an automatically routed address bus. Links
are connected once through shared pointers. No central switch needs to inspect
controller signal names or copy fields between different block structs.

## Controller signals

| Port | Direction | Payload |
|---|---|---|
| MEM_READ | input | uint32_t; sampled, not used yet |
| NEW_DATA | input | uint8_t, low two bits; FIFO completion notifications |
| MEM_WRITE | output | uint32_t; reserved, not driven yet |
| MAC_CTRL | output | uint8_t, low two bits; held MAC enable |
| FETCH_DATA | output | uint8_t, low two bits; request event |

Mask 1 = lane 0; 2 = lane 1; 3 = both lanes. MAC_CTRL is refreshed every cycle;
FETCH_DATA is issued once for each requested group. bus.c copies arbitrary
fixed-size payloads and does not know these meanings. Producer and consumer
must agree on each link's payload type, size and meaning.

Controller sequence: IDLE -> ENABLE_MAC -> REQUEST_FETCH -> WAIT_FIFO, then
request another group or enter DONE. `controller_start(&controller,3)` requests
three output results (two lanes, then one); it does not specify input products.
FETCH_DATA means process a COMPLETE output group. Per-product handshaking and
backpressure are future work. NEW_DATA means results reached FIFO, not SRAM
write completion; next-layer scheduling still needs a writeback contract.

## Running the current stage

Only the controller is registered. There is no fabricated FIFO response.
It correctly remains in WAIT_FIFO. The main loop has a temporary 16-cycle
bring-up limit so the program exits without another block. Once responders
exist, remove that bound and run until controller_done(). No cycle count here
represents complete NN inference latency.

The callback registry, bus copies and logs are a software simulation framework,
not a synthesizable HLS interconnect. Hardware mapping is a separate later step.
