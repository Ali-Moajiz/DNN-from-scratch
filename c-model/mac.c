#include "mac.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
/* Direct SRAM Q -> destination registers. No intermediate fetch/copy module.
 * All sixteen weights capture on ONE edge; lane r holds row r's four bytes. */
static void macArrayCapture(MacArray *a,LoadKind kind,const Line128 *q,uint64_t cycle)
{
    if(kind==LOAD_WEIGHT) {
        for(unsigned r=0;r<4;r++) for(unsigned c=0;c<4;c++) a->cell[r][c].weight=unpack_i8(q->lane[r],c);
        a->weights_loaded=15;log_flow(cycle,"MAC_array","W_CAPTURE_128","16 weights loaded simultaneously");
    } else if(kind==LOAD_BIAS) {
        for(unsigned r=0;r<4;r++) a->bias[r]=unpack_i32(q->lane[r]);
        a->biases_loaded=15;log_flow(cycle,"MAC_array","BIAS_CAPTURE_128","4 INT32 biases loaded simultaneously");
    } else if(kind==LOAD_INPUT) {
        a->input_word=q->lane[0];a->input_loaded=true;
        for(unsigned r=0;r<4;r++) for(unsigned c=0;c<4;c++) a->cell[r][c].valid=false;
        log_flow(cycle,"MAC_array","X_CAPTURE","low32=0x%08X",a->input_word);
    }
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
    MacArray *a=context;Line128 q;Control c;
    log_block_begin("MAC array (16 cells)");
    if(bus_read(a->ports.control,&c,sizeof c)) {
        if(c.read_enable && c.read_kind==LOAD_WEIGHT) {
            a->weights_loaded=0;a->biases_loaded=0;a->input_loaded=false;
        }
        if(c.capture) {
            if(!bus_read(a->ports.sram_data,&q,sizeof q)) { a->error=true;return; }
            macArrayCapture(a,c.capture_kind,&q,cycle);
        }
        if(c.kind==EDGE_COMPUTE) macArrayProcess(a,&c,cycle);
    }
    log_port("CONTROL",a->ports.control,LOG_INPUT,LOG_BYTES);
    log_port("SRAM_Q[127:0]",a->ports.sram_data,LOG_INPUT,LOG_U128);
    log_port("ROW_SUMS",a->ports.row_sums,LOG_OUTPUT,LOG_I32X4);
    if(logging_ports_enabled()) for(unsigned r=0;r<4;r++) {
        printf("  ROW%u bias=%" PRId32,r+1,a->bias[r]);
        for(unsigned col=0;col<4;col++) { Mac *m=&a->cell[r][col];printf(" | MAC%u%u w=%d x=%d psum=%" PRId32 " valid=%u",r+1,col+1,m->weight,m->x,m->psum,m->valid); }
        putchar('\n');
    }
}
bool mac_array_init(MacArray *a,BusNetwork *n,const Bus *ctrl,const Bus *q)
{
    *a=(MacArray){0};a->ports.control=ctrl;a->ports.sram_data=q;a->ports.row_sums=&a->sums_bus;
    return bus_init(&a->sums_bus,sizeof(RowSums)) && bus_register_link(n,&a->sums_bus) && bus_register_module(n,macArrayBusses,a);
}
