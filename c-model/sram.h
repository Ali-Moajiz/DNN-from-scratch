#ifndef SRAM_H
#define SRAM_H
#include "datapath.h"
#define SRAM_LINES 1024u
#define SRAM_BYTES (16u * SRAM_LINES)
/* BYTE addresses, one 16-byte line per tile object. */
#define SRAM_ACTIVATION_A 0x0000u
#define SRAM_L1_WEIGHTS 0x0100u
#define SRAM_L2_WEIGHTS 0x0110u
#define SRAM_L1_BIASES 0x0200u
#define SRAM_L2_BIASES 0x0210u
#define SRAM_ACTIVATION_B 0x0300u
#define SRAM_L1_CONFIG 0x0400u
#define SRAM_L2_CONFIG 0x0410u
typedef struct {
    Line128 lines[SRAM_LINES];
    Bus response_bus;
    struct { const Bus *control,*write_data; Bus *read_response; } ports;
    unsigned reads,writes;
    bool error;
} Sram;
static inline bool sram_address_valid(uint32_t address)
{ return address<SRAM_BYTES && (address&15u)==0; }
bool sram_init(Sram *,BusNetwork *,const Bus *control,const Bus *write_data);
void sramBusses(void *,uint64_t);
#endif
