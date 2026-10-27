#include "controller.h"
#include <stdio.h>
#include <inttypes.h>

static const char *const state_name[]={"IDLE","ENABLE_MAC","REQUEST_FETCH","WAIT_FIFO","DONE"};

/* Initialize this module's port storage and make its single cycle handler
 * visible to the generic traversal. No controller names live inside bus.c.
 * Keep the Controller at a stable address after initialization (do not copy it).
 */
bool controller_init(Controller *c, BusNetwork *network, bool logging)
{
    if(!c || !network || network->error ||
       network->link_count+5>BUS_MAX_LINKS ||
       network->module_count==BUS_MAX_MODULES) return false;
    /* Do not reinitialize an already registered module/link. */
    for(size_t i=0;i<network->module_count;i++)
        if(network->modules[i].module==c) return false;
    Bus *owned[]={&c->buses.mem_read,&c->buses.new_data,&c->buses.mem_write,
                  &c->buses.mac_ctrl,&c->buses.fetch_data};
    for(size_t i=0;i<network->link_count;i++)
        for(size_t j=0;j<5;j++) if(network->links[i]==owned[j]) return false;

    *c=(Controller){0}; c->logging=logging;
    c->ports=(ControllerPorts){&c->buses.mem_read,&c->buses.new_data,
        &c->buses.mem_write,&c->buses.mac_ctrl,&c->buses.fetch_data};
    const size_t sizes[]={sizeof(uint32_t),sizeof(uint8_t),sizeof(uint32_t),
                          sizeof(uint8_t),sizeof(uint8_t)};
    size_t old_links=network->link_count, old_modules=network->module_count;
    for(size_t i=0;i<5;i++) {
        if(!bus_init(owned[i],sizes[i]) || !bus_register_link(network,owned[i])) {
            network->link_count=old_links; return false;
        }
    }
    if(!bus_register_module(network,controllerBusses,c)) {
        network->link_count=old_links; network->module_count=old_modules;
        return false;
    }
    return true;
}

bool controller_start(Controller *c, uint32_t output_count)
{
    if(!c || (c->state!=CTRL_IDLE && c->state!=CTRL_DONE) || c->start_pending) return false;
    c->output_count=output_count; c->completed=0;
    c->active_mask=c->received_mask=0;
    c->start_pending=true; c->state=CTRL_IDLE;
    return true;
}

bool controller_done(const Controller *c) { return c && c->state==CTRL_DONE; }
static uint8_t next_mask(const Controller *c)
{
    return c->output_count-c->completed>=2 ? 3u : 1u;
}

/* Phase 1: sample only published/current input events into private fields. */
static void controllerbusRead(Controller *c)
{
    c->in.mem_read_valid=bus_read(c->ports.mem_read,&c->in.mem_read,sizeof(c->in.mem_read));
    c->in.new_data_valid=bus_read(c->ports.new_data,&c->in.new_data,sizeof(c->in.new_data));
}

/* Phase 2: controller behavior only; no bus accesses inside this function. */
static void controllerProcess(Controller *c)
{
    c->out=(ControllerOutputs){0};
    switch(c->state) {
    case CTRL_IDLE:
        if(c->start_pending) {
            c->start_pending=false;
            c->state=c->output_count ? CTRL_ENABLE_MAC : CTRL_DONE;
        }
        break;
    case CTRL_ENABLE_MAC:
        c->active_mask=next_mask(c); c->out.mac_ctrl=c->active_mask;
        c->state=CTRL_FETCH; break;
    case CTRL_FETCH:
        c->active_mask=next_mask(c); c->received_mask=0;
        c->out.mac_ctrl=c->active_mask;
        c->out.fetch_data=c->active_mask; c->out.fetch_valid=true;
        c->state=CTRL_WAIT_FIFO; break;
    case CTRL_WAIT_FIFO:
        c->out.mac_ctrl=c->active_mask;
        if(c->in.new_data_valid) {
            c->received_mask|=c->in.new_data & c->active_mask;
            if(c->received_mask==c->active_mask) {
                c->completed+=c->active_mask==3 ? 2u : 1u;
                if(c->completed==c->output_count) {
                    c->out.mac_ctrl=0; c->state=CTRL_DONE;
                } else c->state=CTRL_FETCH;
            }
        }
        break;
    case CTRL_DONE: break;
    }
}

/* Phase 3: stage outputs on shared links; generic traversal publishes them. */
static void controllerbusWrite(Controller *c)
{
    /* Level signal refreshed every cycle. FETCH is a single event per group. */
    bus_write(c->ports.mac_ctrl,&c->out.mac_ctrl,sizeof(c->out.mac_ctrl));
    if(c->out.fetch_valid)
        bus_write(c->ports.fetch_data,&c->out.fetch_data,sizeof(c->out.fetch_data));
    /* MEM_READ is sampled but unused; MEM_WRITE is never driven yet. */
}

/* The ONE function registered for this block. Future blocks follow the same
 * read -> process -> write pattern. Main never calls the private phases.
 */
void controllerBusses(void *context, uint64_t cycle)
{
    Controller *c=context; ControllerState old=c->state;
    controllerbusRead(c);
    controllerProcess(c);
    controllerbusWrite(c);
    if(c->logging) {
        printf("cycle %2" PRIu64 " CTRL %-13s -> %-13s MAC_CTRL=%u FETCH=",cycle,
               state_name[old],state_name[c->state],c->out.mac_ctrl);
        if(c->out.fetch_valid) printf("%u",c->out.fetch_data); else printf("-");
        printf(" NEW_DATA=");
        if(c->in.new_data_valid) printf("%u",c->in.new_data); else printf("-");
        printf(" completed=%" PRIu32 "/%" PRIu32 "\n",c->completed,c->output_count);
    }
}
