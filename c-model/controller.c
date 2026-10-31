#include "controller.h"
#include "logging.h"
#include <stdio.h>
static const char *load_name(LoadKind kind)
{
    static const char *names[] = {"W", "BIAS", "CFG", "X"};
    return names[kind];
}
void controllerBusses(void *context, uint64_t cycle)
{
    Controller *c = context;
    Line128 q;
    log_block_begin("Sequencer / AGU");
    if (c->done)
        return;
    if (c->edge == 0)
    {
        c->multiplier = 0;
        c->shift = 0;
        c->relu = false;
        c->config_loaded = 0;
    }
    /* Four reads at E1..E4; destination tag is delayed one clock.
     * Read current Q BEFORE SRAM publishes a new response later this cycle. */
    bool capture = c->edge >= 1 && c->edge <= 4;
    if (capture && !bus_read(c->ports.sram_data, &q, sizeof q))
    {
        c->error = true;
        log_error(cycle, "AGU", "missing SRAM Q for capture");
        return;
    }
    if (c->edge == 3)
    { /* E4 captures CFG while issuing X. */
        if (q.lane[0] == 0 || q.lane[0] > INT32_MAX || q.lane[1] > 62 || q.lane[2] > 1 || q.lane[3] != 0)
        {
            c->error = true;
            log_error(cycle, "AGU", "invalid M/S/flags or nonzero reserved config word");
            return;
        }
        c->multiplier = (int32_t)q.lane[0];
        c->shift = (uint8_t)q.lane[1];
        c->relu = q.lane[2] != 0;
        c->config_loaded = 1;
        log_flow(cycle, "AGU", "CFG_CAPTURE", "M=%d S=%u relu=%u", c->multiplier, c->shift, c->relu);
    }
    LayerConfig *cfg = &c->layers[c->layer];
    Control out = {.kind = EDGE_LOAD, .layer = c->layer, .capture = capture, .multiplier = c->multiplier, .shift = c->shift, .relu = c->relu};
    if (capture)
        out.capture_kind = (LoadKind)(c->edge - 1);
    if (c->edge < 4)
    {
        const uint32_t addresses[] = {cfg->weight_base, cfg->bias_base, cfg->config_base, cfg->input_base};
        out.read_enable = true;
        out.read_kind = (LoadKind)c->edge;
        out.address = addresses[c->edge];
        log_flow(cycle, "AGU", "READ_ISSUE", "layer=%u kind=%s byte_addr=0x%04X", c->layer + 1, load_name(out.read_kind), out.address);
    }
    else if (c->edge >= 5 && c->edge <= 11)
    {
        if (!c->config_loaded)
        {
            c->error = true;
            return;
        }
        out.kind = EDGE_COMPUTE;
        out.wave = c->edge - 4;
    }
    else if (c->edge == 12)
        out.kind = EDGE_POST;
    else if (c->edge == 13)
    {
        uint8_t valid;
        if (!bus_read(c->ports.fifo_valid, &valid, sizeof valid) || !valid)
        {
            c->error = true;
            log_error(cycle, "AGU", "STORE without FIFO valid");
            return;
        }
        out.kind = EDGE_STORE;
        out.address = cfg->output_base;
        out.byte_enable = 0x000f;
    }
    bus_write(c->ports.control, &out, sizeof out);
    log_flow(cycle, "AGU", "EDGE", "layer=%u edge=%u read=%u capture=%s wave=%u POST=%u STORE=%u",
             c->layer + 1, c->edge + 1, out.read_enable, capture ? load_name(out.capture_kind) : "none", out.wave, out.kind == EDGE_POST, out.kind == EDGE_STORE);
    log_port("CONTROL", c->ports.control, LOG_OUTPUT, LOG_BYTES);
    log_port("SRAM_Q[127:0]", c->ports.sram_data, LOG_INPUT, LOG_U128);
    log_port("FIFO_VALID", c->ports.fifo_valid, LOG_INPUT, LOG_U8);
    if (logging_ports_enabled())
        printf("  M=%d S=%u relu=%u RD=%u CAP=%u CAP_KIND=%u BE=0x%04X\n",
               out.multiplier, out.shift, out.relu, out.read_enable, out.capture, (unsigned)out.capture_kind, out.byte_enable);
    if (++c->edge == LAYER_TOTAL_EDGES)
    {
        c->edge = 0;
        if (++c->layer == NETWORK_LAYERS)
            c->done = true;
    }
}
bool controller_init(Controller *c, BusNetwork *n, const LayerConfig cfg[NETWORK_LAYERS], const Bus *valid, const Bus *q)
{
    if (!c || !valid || !q)
        return false;
    *c = (Controller){0};
    c->ports.control = &c->control_bus;
    c->ports.fifo_valid = valid;
    c->ports.sram_data = q;
    for (unsigned l = 0; l < NETWORK_LAYERS; l++)
        c->layers[l] = cfg[l];
    return bus_init_wire(&c->control_bus, sizeof(Control)) && bus_register_link(n, &c->control_bus) && bus_register_module(n, controllerBusses, c);
}
