#include "scale_clamp.h"
#include "logging.h"
int8_t scale_clamp_value(int32_t value,unsigned shift)
{
    /* Portable arithmetic right shift: floor division, including negatives.
     * int64 intermediate handles INT32_MIN without signed-overflow UB. */
    int64_t divisor=INT64_C(1)<<shift;
    int64_t q=value>=0?(int64_t)value/divisor:-((-(int64_t)value+divisor-1)/divisor);
    return (int8_t)(q>127?127:q< -128?-128:q);
}
void scaleClampBusses(void *context,uint64_t cycle)
{
    ScaleClamp *s=context;Control c;RowSums sums;
    log_block_begin("Scale / clamp x4");
    if(bus_read(s->ports.control,&c,sizeof c) && c.kind==EDGE_POST) {
        if(c.shift>31 || !bus_read(s->ports.activated,&sums,sizeof sums)) { s->error=true;return; }
        Quantized out;
        for(unsigned r=0;r<4;r++) out.row[r]=scale_clamp_value(sums.row[r],c.shift);
        bus_write(s->ports.quantized,&out,sizeof out);
        log_flow(cycle,"Scale","QUANTIZE","shift=%u floor=true q=[%d,%d,%d,%d]",c.shift,out.row[0],out.row[1],out.row[2],out.row[3]);
    }
    log_port("CONTROL",s->ports.control,LOG_INPUT,LOG_BYTES);
    log_port("ACTIVATED",s->ports.activated,LOG_INPUT,LOG_I32X4);
    log_port("QUANTIZED",s->ports.quantized,LOG_OUTPUT,LOG_I8X4);
}
bool scale_clamp_init(ScaleClamp *s,BusNetwork *n,const Bus *ctrl,const Bus *act)
{
    *s=(ScaleClamp){0};s->ports.control=ctrl;s->ports.activated=act;s->ports.quantized=&s->output_bus;
    return bus_init_wire(&s->output_bus,sizeof(Quantized)) && bus_register_link(n,&s->output_bus) && bus_register_module(n,scaleClampBusses,s);
}
