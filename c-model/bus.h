#ifndef BUS_H
#define BUS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* GENERIC simulation transport. No controller/MAC/SRAM signal names here.
 * One Bus carries any fixed-size C payload between a producer and consumers.
 * Values persist; valid is an event flag that expires unless written again.
 * Do not put pointers in payloads: memcpy copies a value, not pointed-to data.
 */
#define BUS_MAX_PAYLOAD 64
#define BUS_MAX_LINKS 16
#define BUS_MAX_MODULES 16

typedef struct {
    unsigned char current[BUS_MAX_PAYLOAD], next[BUS_MAX_PAYLOAD];
    size_t payload_size;
    bool valid, next_valid, written, error;
} Bus;

bool bus_init(Bus *bus, size_t payload_size);
bool bus_read(const Bus *bus, void *payload, size_t payload_size);
bool bus_write(Bus *bus, const void *payload, size_t payload_size);

/* Every module registers the same callback signature. */
typedef void (*BusCycleFunction)(void *module, uint64_t cycle);
typedef struct { BusCycleFunction cycle; void *module; } BusModule;
typedef struct {
    Bus *links[BUS_MAX_LINKS];
    BusModule modules[BUS_MAX_MODULES];
    size_t link_count, module_count;
    uint64_t cycle;
    bool error;
} BusNetwork;

void bus_network_init(BusNetwork *network);
bool bus_register_link(BusNetwork *network, Bus *link);
bool bus_register_module(BusNetwork *network, BusCycleFunction cycle, void *module);
/* THE simulation-cycle owner: prepares links, traverses modules, commits links. */
bool bus_run_cycle(BusNetwork *network);
#endif
