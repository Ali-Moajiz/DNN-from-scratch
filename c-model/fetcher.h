#ifndef FETCHER_H
#define FETCHER_H
#include "datapath.h"
typedef struct {
    Bus load_bus,config_bus;
    struct { const Bus *control,*read_response;Bus *load,*config_response; } ports;
    uint32_t x_word;
    bool error;
} Fetcher;
bool fetcher_init(Fetcher *,BusNetwork *,const Bus *,const Bus *);
void fetcherBusses(void *,uint64_t);
#endif
