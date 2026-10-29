#include "sram.h"
#include "logging.h"
/* PLACEHOLDERS ONLY. Replace these quantized parameters when training data arrives.
 * Layer 1 deliberately reproduces page 06 of the supplied drawing.
 * Each row is packed LSB first; biases are in product/accumulator units.
 */
// static const int8_t placeholder_weights[NETWORK_LAYERS][ARRAY_SIZE][ARRAY_SIZE] = {
//     {{1, 2, 3, 4}, {5, 6, 7, 8}, {-1, -2, -3, -4}, {2, 0, 2, 0}},
//     {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 1}, {1, 0, 0, 1}}};
// static const int32_t placeholder_biases[NETWORK_LAYERS][ARRAY_SIZE] = {
//     {4, -20, 10, 8}, {0, 4, -4, 8}};

static const int8_t placeholder_weights[NETWORK_LAYERS][ARRAY_SIZE][ARRAY_SIZE] = {
    {{22, 28, -58, 46}, {-46, -31, -127, -58}, {108, -17, 68, 76}, {80, 24, 80, 31}},
    {{3, 24, 2, 60}, {-64, 36, 29, 21}, {-64, -117, -99, -127}, {17, -37, -44, 9}}
};

static const int32_t placeholder_biases[NETWORK_LAYERS][ARRAY_SIZE] = {
    {38, 101, 127, -31},
    {-16, 127, -69, -81}
};

static const int8_t placeholder_input[ARRAY_SIZE] = {10, 20, 30, 40};
void sramBusses(void *context, uint64_t cycle)
{
    Sram *s = context;
    Control c;
    log_block_begin("SRAM");
    if (bus_read(s->ports.control, &c, sizeof c) && (c.kind == EDGE_READ_REQ || c.kind == EDGE_STORE))
    {
        if (c.address >= SRAM_WORDS)
        {
            s->error = true;
            log_error(cycle, "SRAM", "address out of range: %u", c.address);
            return;
        }
        if (c.kind == EDGE_READ_REQ)
        {
            uint32_t value = s->words[c.address];
            bus_write(s->ports.read_response, &value, sizeof value);
            s->reads++;
            log_flow(cycle, "SRAM", "READ", "addr=0x%03X data=0x%08X capture_edge=%llu", c.address, value, (unsigned long long)(cycle + 1));
        }
        else
        {
            uint32_t value;
            if (!bus_read(s->ports.write_data, &value, sizeof value))
            {
                s->error = true;
                log_error(cycle, "SRAM", "write without FIFO data");
                return;
            }
            s->words[c.address] = value;
            s->writes++;
            log_flow(cycle, "SRAM", "WRITE", "addr=0x%03X data=0x%08X layer=%u", c.address, value, c.layer + 1);
        }
    }
    log_port("CONTROL", s->ports.control, LOG_INPUT, LOG_BYTES);
    log_port("FIFO_WRITE_DATA", s->ports.write_data, LOG_INPUT, LOG_U32);
    log_port("READ_RESPONSE", s->ports.read_response, LOG_OUTPUT, LOG_U32);
}
bool sram_init(Sram *s, BusNetwork *n, const Bus *ctrl, const Bus *data)
{
    *s = (Sram){0};
    s->ports.control = ctrl;
    s->ports.write_data = data;
    s->ports.read_response = &s->response_bus;
    for (unsigned l = 0; l < NETWORK_LAYERS; l++)
        for (unsigned r = 0; r < ARRAY_SIZE; r++)
        {
            s->words[SRAM_L1_WEIGHTS + 4 * l + r] = pack_i8(placeholder_weights[l][r]);
            s->words[SRAM_L1_BIASES + 4 * l + r] = (uint32_t)placeholder_biases[l][r];
        }
    s->words[SRAM_ACTIVATION_A] = pack_i8(placeholder_input);
    return bus_init(&s->response_bus, sizeof(uint32_t)) && bus_register_link(n, &s->response_bus) && bus_register_module(n, sramBusses, s);
}
