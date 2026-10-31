#include "npu.h"
#include "logging.h"
bool npu_init(Npu *n)
{
    if (!n)
        return false;
    *n = (Npu){0};
    bus_network_init(&n->network);
    bus_network_set_logging(&n->network, true);
    n->network.cycle = 1; /* Cycle 1 is S1, after host start/config acceptance. */
    const LayerConfig config[NETWORK_LAYERS] = {
        {SRAM_L1_WEIGHTS, SRAM_L1_BIASES, SRAM_ACTIVATION_A, SRAM_ACTIVATION_B, SRAM_L1_CONFIG},
        {SRAM_L2_WEIGHTS, SRAM_L2_BIASES, SRAM_ACTIVATION_B, SRAM_ACTIVATION_A, SRAM_L2_CONFIG}};
    /* Embedded bus addresses are stable before initialization. All constructors
     * finish before the first traversal. Connections are ordinary shared pointers. */
    if (!controller_init(&n->controller, &n->network, config, &n->fifo.valid_bus, &n->sram.response_bus) ||
        !sram_init(&n->sram, &n->network, &n->controller.control_bus, &n->fifo.data_bus) ||
        !mac_array_init(&n->array, &n->network, &n->controller.control_bus, &n->sram.response_bus) ||
        !activation_init(&n->activation, &n->network, &n->controller.control_bus, &n->array.sums_bus) ||
        !scale_clamp_init(&n->scale, &n->network, &n->controller.control_bus, &n->activation.output_bus) ||
        !fifo_init(&n->fifo, &n->network, &n->controller.control_bus, &n->scale.output_bus))
        return false;
    log_flow(0, "NPU", "INITIALIZED", "network=4->4->4 array=4x4 parameters=trained_npu_params.h SRAM_width=128 direct_capture=true layers=2");
    return true;
}
NpuRunStatus npu_run(Npu *n, uint64_t limit)
{
    if (!n)
        return NPU_RUN_ERROR;
    for (uint64_t i = 0; i < limit && !n->controller.done; i++)
    {
        bool ok = bus_run_cycle(&n->network);
        if (!ok || n->controller.error || n->sram.error || n->array.error || n->activation.error || n->scale.error || n->fifo.error)
        {
            log_error(n->network.cycle - 1, "NPU", "stopping after block/bus fault");
            return NPU_RUN_ERROR;
        }
    }
    if (!n->controller.done)
    {
        log_flow(n->network.cycle, "NPU", "RUN_LIMIT", "not complete");
        return NPU_RUN_LIMIT;
    }
    log_flow(n->network.cycle - 1, "NPU", "COMPLETE", "cycles=%llu reads=%u writes=%u mac_updates=%u output_byte_addr=0x%04X word=0x%08X",
             (unsigned long long)(n->network.cycle - 1), n->sram.reads, n->sram.writes, n->array.updates, SRAM_ACTIVATION_A, n->sram.lines[SRAM_ACTIVATION_A / 16].lane[0]);
    return NPU_RUN_COMPLETE;
}
