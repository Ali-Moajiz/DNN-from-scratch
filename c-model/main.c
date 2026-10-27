#include "controller.h"
#include <stdio.h>

/* Application wiring only. No demo/mock/test routines and no bus phase calls. */
int main(void)
{
    BusNetwork network;
    Controller controller;

    bus_network_init(&network);
    /* Creates ports and registers controllerBusses() with the traversal. */
    if(!controller_init(&controller,&network,true)) {
        fputs("Controller initialization failed.\n",stderr);
        return 1;
    }
    if(!controller_start(&controller,3)) return 1;

    /* Later, initialize/register other blocks here using shared port pointers.
     * Example: fetcher_init(..., controller.ports.fetch_data, ...).
     * Only the generic bus traversal is called inside the cycle loop.
     *
     * For this controller-only bring-up, stop after 16 cycles. Without a FIFO
     * producer the controller correctly remains in WAIT_FIFO. Once real blocks
     * are connected, remove the limit to run until controller_done().
     */
    for(unsigned cycle=0;cycle<16 && !controller_done(&controller);cycle++) {
        if(!bus_run_cycle(&network)) {
            fputs("Fatal bus error.\n",stderr);
            return 1;
        }
    }
    return 0;
}
