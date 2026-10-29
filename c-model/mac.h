#ifndef MAC_H
#define MAC_H
#include "datapath.h"
/* Each processing element has a stationary weight, forwarded x register,
 * and a registered partial sum. Horizontal psum is NOT a combinational chain. */
typedef struct
{
    int8_t weight, x;
    int32_t psum;
    bool valid;
} Mac;

typedef struct
{
    Mac cell[ARRAY_SIZE][ARRAY_SIZE];
    int32_t bias[ARRAY_SIZE];
    uint32_t input_word;
    RowSums sums;
    Bus sums_bus;
    struct
    {
        const Bus *control, *load;
        Bus *row_sums;
    } ports;
    uint8_t weights_loaded, biases_loaded;
    bool input_loaded, error;
    unsigned updates;
} MacArray;

bool mac_array_init(MacArray *, BusNetwork *, const Bus *, const Bus *);
void macArrayBusses(void *, uint64_t);
#endif
