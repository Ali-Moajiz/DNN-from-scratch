#include "bus.h"
#include <string.h>

bool bus_init(Bus *b, size_t size)
{
    if (!b || size==0 || size>BUS_MAX_PAYLOAD) return false;
    *b=(Bus){0}; b->payload_size=size; return true;
}

bool bus_read(const Bus *b, void *payload, size_t size)
{
    if (!b || !payload || size!=b->payload_size || !b->valid) return false;
    memcpy(payload,b->current,size);     /* read never consumes an event */
    return true;
}

bool bus_write(Bus *b, const void *payload, size_t size)
{
    if (!b) return false;
    if (!payload || size!=b->payload_size || b->written) {
        b->error=true; return false;     /* reject size mismatch / two writes */
    }
    memcpy(b->next,payload,size);        /* invisible to reads until commit */
    b->next_valid=true; b->written=true;
    return true;
}

void bus_network_init(BusNetwork *n) { if(n) *n=(BusNetwork){0}; }

bool bus_register_link(BusNetwork *n, Bus *b)
{
    if (!n || !b || b->payload_size==0 || n->link_count==BUS_MAX_LINKS) return false;
    for(size_t i=0;i<n->link_count;i++) if(n->links[i]==b) return false;
    n->links[n->link_count++]=b; return true;
}

bool bus_register_module(BusNetwork *n, BusCycleFunction fn, void *module)
{
    if (!n || !fn || !module || n->module_count==BUS_MAX_MODULES) return false;
    for(size_t i=0;i<n->module_count;i++)
        if(n->modules[i].cycle==fn && n->modules[i].module==module) return false;
    n->modules[n->module_count++]=(BusModule){fn,module}; return true;
}

bool bus_run_cycle(BusNetwork *n)
{
    if (!n || n->error) return false;
    /* 1. Prepare next-cycle signals. Current signals remain immutable. */
    for(size_t i=0;i<n->link_count;i++) {
        Bus *b=n->links[i];
        if(b->error) { n->error=true; return false; }
        memcpy(b->next,b->current,b->payload_size);
        b->next_valid=false; b->written=false;
    }
    /* 2. Every registered block executes once, with private persistent state. */
    for(size_t i=0;i<n->module_count;i++)
        n->modules[i].cycle(n->modules[i].module,n->cycle);
    /* Abort publication on any bus error. Treat it as a fatal simulation error:
     * private module states may have advanced, so do not retry this cycle. */
    for(size_t i=0;i<n->link_count;i++) if(n->links[i]->error) {
        n->error=true; return false;
    }
    /* 3. Publish all links together. New data is visible on the NEXT cycle. */
    for(size_t i=0;i<n->link_count;i++) {
        Bus *b=n->links[i];
        memcpy(b->current,b->next,b->payload_size); b->valid=b->next_valid;
    }
    n->cycle++; return true;
}
