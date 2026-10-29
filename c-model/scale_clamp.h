#ifndef SCALE_CLAMP_H
#define SCALE_CLAMP_H
#include "datapath.h"
typedef struct {
    Bus output_bus;
    struct { const Bus *control,*activated;Bus *quantized; } ports;
    bool error;
} ScaleClamp;
int8_t scale_clamp_value(int32_t value,unsigned shift);
bool scale_clamp_init(ScaleClamp *,BusNetwork *,const Bus *,const Bus *);
void scaleClampBusses(void *,uint64_t);
#endif
