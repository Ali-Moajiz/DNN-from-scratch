#ifndef SCALE_CLAMP_H
#define SCALE_CLAMP_H
#include "datapath.h"
typedef struct {
    Bus output_bus;
    struct { const Bus *control,*activated;Bus *quantized; } ports;
    bool error;
} ScaleClamp;
/* Requires multiplier in [1,INT32_MAX], shift in [0,62]; checked by controller/block. */
int8_t scale_clamp_value(int32_t value,int32_t multiplier,unsigned shift);
bool scale_clamp_init(ScaleClamp *,BusNetwork *,const Bus *,const Bus *);
void scaleClampBusses(void *,uint64_t);
#endif
