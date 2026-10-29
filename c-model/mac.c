#include "mac.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static void macArrayLoad(MacArray *a,const FetchLoad *load)
{
    if(load->index>=4) { a->error=true;return; }
    unsigned r=load->index;
    if(load->kind==LOAD_WEIGHT) {
        for(unsigned c=0;c<4;c++) a->cell[r][c].weight=unpack_i8(load->word,c);
        a->weights_loaded|=(uint8_t)(1u<<r);
    } else if(load->kind==LOAD_BIAS) {
        a->bias[r]=unpack_i32(load->word);a->biases_loaded|=(uint8_t)(1u<<r);
    } else { a->input_word=load->word;a->input_loaded=true; }
}
static void macArrayProcess(MacArray *a,const Control *ctrl,uint64_t cycle)
{
    if(a->weights_loaded!=15 || a->biases_loaded!=15 || !a->input_loaded || ctrl->wave<1 || ctrl->wave>7) {
        a->error=true;log_error(cycle,"MAC_array","compute before complete operand setup");return;
    }
    /* Snapshot ALL old registers before ANY cell updates. This is the edge.
     * Software loop iterations evaluate independent hardware cells, not clocks. */
    Mac old[4][4];memcpy(old,a->cell,sizeof old);
    for(unsigned r=0;r<4;r++) for(unsigned c=0;c<4;c++) {
        if(ctrl->wave!=r+c+1) continue;
        if((r && !old[r-1][c].valid) || (c && !old[r][c-1].valid)) { a->error=true;return; }
        int8_t x=r?old[r-1][c].x:unpack_i8(a->input_word,c);
        int32_t incoming=c?old[r][c-1].psum:a->bias[r];
        int16_t product=(int16_t)((int)x*(int)old[r][c].weight);
        int64_t sum=(int64_t)incoming+product;
        /* Diagram forbids accumulator saturation. Reject unsupported overflow. */
        if(sum<INT32_MIN || sum>INT32_MAX) { a->error=true;log_error(cycle,"MAC_array","INT32 overflow row=%u col=%u",r+1,c+1);return; }
        a->cell[r][c].x=x;a->cell[r][c].psum=(int32_t)sum;a->cell[r][c].valid=true;a->updates++;
        if(c==3) a->sums.row[r]=(int32_t)sum;
        log_flow(cycle,"MAC_array","MAC_UPDATE","C%u MAC%u%u x=%d w=%d psum_in=%" PRId32 " product=%d psum_out=%" PRId32 " row_done=%u",
            ctrl->wave,r+1,c+1,(int)x,(int)old[r][c].weight,incoming,(int)product,(int32_t)sum,c==3);
    }
    bus_write(a->ports.row_sums,&a->sums,sizeof a->sums);
    if(ctrl->wave==7) a->input_loaded=false;
}
void macArrayBusses(void *context,uint64_t cycle)
{
    MacArray *a=context;FetchLoad load;Control c;
    log_block_begin("MAC array (16 cells)");
    if(bus_read(a->ports.load,&load,sizeof load)) {
        macArrayLoad(a,&load);
        log_flow(cycle,"MAC_array",load.kind==LOAD_WEIGHT?"W_LOAD":load.kind==LOAD_BIAS?"BIAS_LOAD":"X_CAPTURE",
                 "index=%u word=0x%08X",load.index,load.word);
        if(load.kind==LOAD_INPUT) {
            for(unsigned r=0;r<4;r++) for(unsigned col=0;col<4;col++) a->cell[r][col].valid=false;
        }
    }
    if(bus_read(a->ports.control,&c,sizeof c) && c.kind==EDGE_COMPUTE) macArrayProcess(a,&c,cycle);
    log_port("CONTROL",a->ports.control,LOG_INPUT,LOG_BYTES);
    log_port("ARRAY_LOAD",a->ports.load,LOG_INPUT,LOG_BYTES);
    log_port("ROW_SUMS",a->ports.row_sums,LOG_OUTPUT,LOG_I32X4);
    if(logging_ports_enabled()) for(unsigned r=0;r<4;r++) {
        printf("  ROW%u bias=%" PRId32,r+1,a->bias[r]);
        for(unsigned col=0;col<4;col++) { Mac *m=&a->cell[r][col];printf(" | MAC%u%u w=%d x=%d psum=%" PRId32 " valid=%u",r+1,col+1,m->weight,m->x,m->psum,m->valid); }
        putchar('\n');
    }
}
bool mac_array_init(MacArray *a,BusNetwork *n,const Bus *ctrl,const Bus *load)
{
    *a=(MacArray){0};a->ports.control=ctrl;a->ports.load=load;a->ports.row_sums=&a->sums_bus;
    return bus_init(&a->sums_bus,sizeof(RowSums)) && bus_register_link(n,&a->sums_bus) && bus_register_module(n,macArrayBusses,a);
}
