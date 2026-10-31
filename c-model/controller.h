#ifndef CONTROLLER_H
#define CONTROLLER_H
#include "datapath.h"
typedef struct {
    Bus control_bus;
    struct { Bus *control; const Bus *fifo_valid,*sram_data; } ports;
    LayerConfig layers[NETWORK_LAYERS];
    unsigned layer, edge;
    int32_t multiplier;
    uint8_t shift, config_loaded;
    bool relu;
    bool done, error;
} Controller;
bool controller_init(Controller *,BusNetwork *,const LayerConfig [NETWORK_LAYERS],const Bus *fifo_valid,const Bus *sram_data);
void controllerBusses(void *,uint64_t);
#endif
