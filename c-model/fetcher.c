#include "fetcher.h"
#include "logging.h"
void fetcherBusses(void *context,uint64_t cycle)
{
    Fetcher *f=context;Control c;
    log_block_begin("Fetcher");
    if(bus_read(f->ports.control,&c,sizeof c) && c.kind==EDGE_READ_CAP) {
        FetchLoad load={.kind=c.load,.index=c.index};
        if(!bus_read(f->ports.read_response,&load.word,sizeof load.word)) {
            f->error=true;log_error(cycle,"Fetcher","CAP without registered SRAM read response");return;
        }
        if(c.load==LOAD_INPUT) f->x_word=load.word;
        Bus *destination=c.load==LOAD_CONFIG?f->ports.config_response:f->ports.load;
        bus_write(destination,&load,sizeof load);
        log_flow(cycle,"Fetcher","CAPTURE_ROUTE","kind=%u index=%u word=0x%08X destination=%s",(unsigned)load.kind,load.index,load.word,c.load==LOAD_CONFIG?"Controller(next_cycle)":"MAC_array");
    }
    log_port("CONTROL",f->ports.control,LOG_INPUT,LOG_BYTES);
    log_port("READ_RESPONSE",f->ports.read_response,LOG_INPUT,LOG_U32);
    log_port("CONFIG_RESPONSE",f->ports.config_response,LOG_OUTPUT,LOG_BYTES);
    log_port("ARRAY_LOAD",f->ports.load,LOG_OUTPUT,LOG_BYTES);
}
bool fetcher_init(Fetcher *f,BusNetwork *n,const Bus *ctrl,const Bus *response)
{
    *f=(Fetcher){0};f->ports.control=ctrl;f->ports.read_response=response;f->ports.load=&f->load_bus;f->ports.config_response=&f->config_bus;
    return bus_init_wire(&f->load_bus,sizeof(FetchLoad)) && bus_init(&f->config_bus,sizeof(FetchLoad)) &&
           bus_register_link(n,&f->load_bus) && bus_register_link(n,&f->config_bus) && bus_register_module(n,fetcherBusses,f);
}
