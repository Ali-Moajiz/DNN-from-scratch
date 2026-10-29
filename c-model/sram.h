#ifndef SRAM_H
#define SRAM_H
#include "datapath.h"
#define SRAM_WORDS 4096
#define SRAM_ACTIVATION_A 0x000u
#define SRAM_ACTIVATION_B 0x300u
#define SRAM_L1_WEIGHTS 0x100u
#define SRAM_L2_WEIGHTS 0x104u
#define SRAM_L1_BIASES 0x200u
#define SRAM_L2_BIASES 0x204u

typedef struct
{
    uint32_t words[SRAM_WORDS];
    Bus response_bus;
    struct
    {
        const Bus *control, *write_data;
        Bus *read_response;
    } ports;
    unsigned reads, writes;
    bool error;
} Sram;

bool sram_init(Sram *, BusNetwork *, const Bus *control, const Bus *write_data);
void sramBusses(void *, uint64_t);
#endif
