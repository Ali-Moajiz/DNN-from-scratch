#ifndef TRAINED_NPU_PARAMS_H
#define TRAINED_NPU_PARAMS_H
#include <stdint.h>
/* W[output][input]; INT32 biases are already in accumulator units.
 * Requantize: clamp_int8(round_ties_even((int64_t)acc * M / 2^S)).
 * Configuration SRAM addresses/packing are defined by the C model.
 * Float scales below are HOST metadata, not required in the MAC. */
#define NPU_INPUT_SCALE 1.057168434924028
#define NPU_HIDDEN_SCALE 1.147024048373249
#define NPU_OUTPUT_SCALE 2.357453263723774
static const int8_t trained_weights[2][4][4] = {{{19, 25, -57, 43}, {-51, -37, -127, -46}, {111, -9, 69, 50}, {75, 21, 78, 25}}, {{3, 26, 1, 63}, {-62, 38, 30, 17}, {-65, -121, -121, -127}, {17, -40, -45, 10}}};
static const int32_t trained_biases[2][4] = {{42, 124, 183, -54}, {-13, 112, -60, -72}};
static const int32_t requant_multiplier[2] = {1585768005, 1143168641};
static const uint32_t requant_shift[2] = {38, 38};
static const uint32_t relu_enable[2] = {1, 0};
#endif
