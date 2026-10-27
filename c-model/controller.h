#ifndef CONTROLLER_H
#define CONTROLLER_H
#include "bus.h"

/* This module owns signal meaning; the generic bus knows only payload bytes.
 * Use uint8_t for 2-bit masks (only low 2 bits are meaningful), uint32_t for RAM.
 * Each future block receives the SAME Bus pointer for a connected signal.
 */
typedef struct {
    Bus *mem_read;                       /* input: SRAM -> controller; uint32_t */
    Bus *new_data;                       /* input: FIFO -> controller; uint8_t */
    Bus *mem_write;                      /* output: controller -> SRAM; uint32_t */
    Bus *mac_ctrl;                       /* output: controller -> MACs; uint8_t */
    Bus *fetch_data;                     /* output: controller -> fetcher; uint8_t */
} ControllerPorts;

typedef struct {
    uint32_t mem_read;
    uint8_t new_data;
    bool mem_read_valid, new_data_valid;
} ControllerInputs;
typedef struct {
    uint32_t mem_write;
    uint8_t mac_ctrl, fetch_data;
    bool mem_write_valid, fetch_valid;
} ControllerOutputs;
typedef enum { CTRL_IDLE, CTRL_ENABLE_MAC, CTRL_FETCH, CTRL_WAIT_FIFO, CTRL_DONE } ControllerState;
/* Link storage belongs to this module; connected blocks use ports pointers.
 * Each Bus is initialized/registered ONCE by controller_init(). A future block
 * reuses the supplied pointer; it must not reset or re-register that link.
 */
typedef struct {
    Bus mem_read, new_data, mem_write, mac_ctrl, fetch_data;
} ControllerBusStorage;

typedef struct {
    ControllerBusStorage buses;
    ControllerPorts ports;
    ControllerInputs in;
    ControllerOutputs out;
    ControllerState state;
    uint32_t output_count, completed;
    uint8_t active_mask, received_mask;
    bool start_pending, logging;
} Controller;

bool controller_init(Controller *controller, BusNetwork *network, bool logging);
/* Host/testbench config for now, not an additional physical input wire. */
bool controller_start(Controller *controller, uint32_t output_count);
void controllerBusses(void *controller, uint64_t cycle);
bool controller_done(const Controller *controller);
#endif
