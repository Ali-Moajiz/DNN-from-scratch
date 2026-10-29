#include "controller.h"
#include "logging.h"
#include <stdio.h>
static const char *edge_name(EdgeKind k)
{
    static const char *names[]={"READ_REQ","READ_CAP","COMPUTE","POST","STORE"};
    return names[k];
}
/* Fetcher runs after Controller. Its registered response is consumed at the
 * next REQ edge (3/5/7), overlapping configuration capture with the next read.
 * Thus three REQ/CAP pairs add exactly six cycles, not nine. */
static bool controllerReadConfig(Controller *c,uint64_t cycle)
{
    FetchLoad response;
    if(!bus_read(c->ports.config_response,&response,sizeof response)) {
        if(c->edge==2 || c->edge==4 || c->edge==6) {
            log_error(cycle,"Controller","missing configuration response");return false;
        }
        return true;
    }
    if(response.kind!=LOAD_CONFIG || response.index>=LAYER_CONFIG_WORDS ||
       c->edge!=2*(response.index+1)) {
        log_error(cycle,"Controller","unexpected configuration response");return false;
    }
    uint32_t word=response.word;
    if(response.index==0) {
        if(word==0 || word>INT32_MAX) { log_error(cycle,"Controller","invalid multiplier %u",word);return false; }
        c->multiplier=(int32_t)word;
    } else if(response.index==1) {
        if(word>62) { log_error(cycle,"Controller","invalid shift %u",word);return false; }
        c->shift=(uint8_t)word;
    } else {
        if(word>1) { log_error(cycle,"Controller","invalid ReLU flag %u",word);return false; }
        c->relu=word!=0;
    }
    c->config_loaded|=(uint8_t)(1u<<response.index);
    log_flow(cycle,"Controller","CONFIG_CAPTURE","layer=%u index=%u word=%u M=%d S=%u relu=%u loaded=0x%X",
             c->layer+1,response.index,word,c->multiplier,c->shift,c->relu,c->config_loaded);
    return true;
}
static void controllerProcess(Controller *c,Control *out)
{
    LayerConfig *cfg=&c->layers[c->layer];
    *out=(Control){.layer=c->layer,.multiplier=c->multiplier,.shift=c->shift,.relu=c->relu};
    if(c->edge<LAYER_SETUP_EDGES) {
        unsigned read=c->edge/2;
        out->kind=(c->edge%2)?EDGE_READ_CAP:EDGE_READ_REQ;
        if(read<LAYER_CONFIG_WORDS) {
            out->load=LOAD_CONFIG;out->index=read;out->address=cfg->config_base+read;
        } else if(read<LAYER_CONFIG_WORDS+4) {
            out->load=LOAD_WEIGHT;out->index=read-LAYER_CONFIG_WORDS;
            out->address=cfg->weight_base+out->index;
        } else if(read<LAYER_CONFIG_WORDS+8) {
            out->load=LOAD_BIAS;out->index=read-LAYER_CONFIG_WORDS-4;
            out->address=cfg->bias_base+out->index;
        } else { out->load=LOAD_INPUT;out->address=cfg->input_base; }
    } else if(c->edge<LAYER_SETUP_EDGES+LAYER_COMPUTE_EDGES) {
        out->kind=EDGE_COMPUTE;out->wave=c->edge-LAYER_SETUP_EDGES+1;
    } else if(c->edge==LAYER_TOTAL_EDGES-2) out->kind=EDGE_POST;
    else { out->kind=EDGE_STORE;out->address=cfg->output_base; }
}
void controllerBusses(void *context,uint64_t cycle)
{
    Controller *c=context;
    log_block_begin("Controller");
    if(c->done) return;
    if(c->edge==0) { c->multiplier=0;c->shift=0;c->relu=false;c->config_loaded=0; }
    if(!controllerReadConfig(c,cycle)) { c->error=true;return; }
    Control out;controllerProcess(c,&out);
    if(out.kind==EDGE_COMPUTE && c->config_loaded!=7) {
        c->error=true;log_error(cycle,"Controller","compute before configuration complete");return;
    }
    uint8_t valid=0;
    if(out.kind==EDGE_STORE && (!bus_read(c->ports.fifo_valid,&valid,sizeof valid) || !valid)) {
        c->error=true;log_error(cycle,"Controller","STORE requires a valid FIFO entry");return;
    }
    bus_write(c->ports.control,&out,sizeof out);
    log_flow(cycle,"Controller",edge_name(out.kind),"layer=%u edge=%u address=0x%03X load=%u index=%u wave=%u M=%d S=%u relu=%u",
             out.layer+1,c->edge+1,out.address,(unsigned)out.load,out.index,out.wave,out.multiplier,out.shift,out.relu);
    log_port("CONTROL",c->ports.control,LOG_OUTPUT,LOG_BYTES);
    log_port("CONFIG_RESPONSE",c->ports.config_response,LOG_INPUT,LOG_BYTES);
    log_port("FIFO_VALID",c->ports.fifo_valid,LOG_INPUT,LOG_U8);
    if(logging_ports_enabled()) printf("  DECODE state=%s layer=%u edge=%u wave=%u RE=%u WE=%u POST=%u M=%d S=%u relu=%u\n",
        edge_name(out.kind),out.layer+1,c->edge+1,out.wave,out.kind==EDGE_READ_REQ,out.kind==EDGE_STORE,out.kind==EDGE_POST,
        out.multiplier,out.shift,out.relu);
    if(++c->edge==LAYER_TOTAL_EDGES) {
        c->edge=0;
        if(++c->layer==NETWORK_LAYERS) c->done=true;
    }
}
bool controller_init(Controller *c,BusNetwork *n,const LayerConfig cfg[NETWORK_LAYERS],const Bus *valid,const Bus *config)
{
    if(!c || !valid || !config) return false;
    *c=(Controller){0};c->ports.control=&c->control_bus;c->ports.fifo_valid=valid;c->ports.config_response=config;
    for(unsigned l=0;l<NETWORK_LAYERS;l++) c->layers[l]=cfg[l];
    return bus_init_wire(&c->control_bus,sizeof(Control)) && bus_register_link(n,&c->control_bus) && bus_register_module(n,controllerBusses,c);
}
