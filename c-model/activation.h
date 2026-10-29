#ifndef ACTIVATION_H
#define ACTIVATION_H
#include "datapath.h"
typedef struct {
    Bus output_bus;
    struct { const Bus *control,*row_sums;Bus *activated; } ports;
    bool error;
} Activation;
bool activation_init(Activation *,BusNetwork *,const Bus *,const Bus *);
void activationBusses(void *,uint64_t);
#endif
