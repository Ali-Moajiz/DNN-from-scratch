#ifndef FIFO_H
#define FIFO_H
#include "datapath.h"
typedef struct
{
    Bus data_bus, valid_bus;
    struct
    {
        const Bus *control, *quantized;
        Bus *write_data, *valid;
    } ports;
    uint32_t word;
    uint8_t valid;
    unsigned pushes, pops;
    bool error;
} ResultFifo;

bool fifo_init(ResultFifo *, BusNetwork *, const Bus *, const Bus *);
void fifoBusses(void *, uint64_t);
#endif
