#include "controller.h"
#include "logging.h"
#include <stdio.h>
static const char *edge_name(EdgeKind k)
{
    static const char *names[]={"READ_REQ","READ_CAP","COMPUTE","POST","STORE"};
    return names[k];
}
/* Decode before the edge; advance only after publishing this edge's controls. */
static void controllerProcess(Controller *c,Control *out)
{
    LayerConfig *cfg=&c->layers[c->layer];
    *out=(Control){.layer=c->layer,.shift=cfg->shift,.relu=cfg->relu};
    if(c->edge<18) {
        unsigned read=c->edge/2;
        out->kind=(c->edge%2)?EDGE_READ_CAP:EDGE_READ_REQ;
        out->load=read<4?LOAD_WEIGHT:read<8?LOAD_BIAS:LOAD_INPUT;
        out->index=read<4?read:read<8?read-4:0;
        out->address=read<4?cfg->weight_base+read:read<8?cfg->bias_base+read-4:cfg->input_base;
    } else if(c->edge<25) { out->kind=EDGE_COMPUTE;out->wave=c->edge-17; }
    else if(c->edge==25) out->kind=EDGE_POST;
    else { out->kind=EDGE_STORE;out->address=cfg->output_base; }
}
void controllerBusses(void *context,uint64_t cycle)
{
    Controller *c=context;
    log_block_begin("Controller");
    if(c->done) return;
    Control out; controllerProcess(c,&out);
    uint8_t valid=0;
    if(out.kind==EDGE_STORE && (!bus_read(c->ports.fifo_valid,&valid,sizeof valid) || !valid)) {
        c->error=true;log_error(cycle,"Controller","STORE requires a valid FIFO entry");return;
    }
    bus_write(c->ports.control,&out,sizeof out);
    log_flow(cycle,"Controller",edge_name(out.kind),"layer=%u edge=%u address=0x%03X load=%u index=%u wave=%u relu=%u shift=%u",
             out.layer+1,c->edge+1,out.address,(unsigned)out.load,out.index,out.wave,(unsigned)out.relu,out.shift);
    log_port("CONTROL",c->ports.control,LOG_OUTPUT,LOG_BYTES);
    log_port("FIFO_VALID",c->ports.fifo_valid,LOG_INPUT,LOG_U8);
    if(logging_ports_enabled()) printf("  DECODE state=%s layer=%u edge=%u wave=%u RE=%u WE=%u POST=%u\n",
        edge_name(out.kind),out.layer+1,c->edge+1,out.wave,out.kind==EDGE_READ_REQ,out.kind==EDGE_STORE,out.kind==EDGE_POST);
    if(++c->edge==27) {
        c->edge=0;
        if(++c->layer==NETWORK_LAYERS) c->done=true;
    }
}
bool controller_init(Controller *c,BusNetwork *n,const LayerConfig cfg[NETWORK_LAYERS],const Bus *valid)
{
    if(!c || !valid) return false;
    *c=(Controller){0}; c->ports.control=&c->control_bus;c->ports.fifo_valid=valid;
    for(unsigned l=0;l<NETWORK_LAYERS;l++) { if(cfg[l].shift>31) return false;c->layers[l]=cfg[l]; }
    return bus_init_wire(&c->control_bus,sizeof(Control)) && bus_register_link(n,&c->control_bus) && bus_register_module(n,controllerBusses,c);
}
