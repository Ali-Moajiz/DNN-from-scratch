#ifndef DATAPATH_H
#define DATAPATH_H
#include "bus.h"
#define ARRAY_SIZE 4
#define NETWORK_LAYERS 2
#define LAYER_TOTAL_EDGES 14u
#define NPU_INFERENCE_CYCLES (NETWORK_LAYERS * LAYER_TOTAL_EDGES)
/* Explicit logical 128-bit value: lane[0] is bits31:0, independent of host endian. */
typedef struct { uint32_t lane[4]; } Line128;
/* All bases are aligned BYTE addresses. */
typedef struct {
    uint32_t weight_base, bias_base, input_base, output_base, config_base;
} LayerConfig;
typedef enum { EDGE_LOAD, EDGE_COMPUTE, EDGE_POST, EDGE_STORE } EdgeKind;
typedef enum { LOAD_WEIGHT, LOAD_BIAS, LOAD_CONFIG, LOAD_INPUT } LoadKind;
/* Dedicated control wires; SRAM data is on a separate registered 128-bit link. */
typedef struct {
    EdgeKind kind;
    bool read_enable, capture;
    LoadKind read_kind, capture_kind;
    uint32_t address;
    uint16_t byte_enable;
    unsigned layer, wave;
    int32_t multiplier;
    uint8_t shift;
    bool relu;
} Control;
typedef struct { int32_t row[ARRAY_SIZE]; } RowSums;
typedef struct { int8_t row[ARRAY_SIZE]; } Quantized;
static inline int8_t unpack_i8(uint32_t word,unsigned byte)
{
    int value=(int)((word>>(8*byte))&255u);
    return (int8_t)(value<128?value:value-256);
}
static inline int32_t unpack_i32(uint32_t word)
{
    return word<=INT32_MAX?(int32_t)word:-1-(int32_t)(UINT32_MAX-word);
}
static inline uint32_t pack_i8(const int8_t data[ARRAY_SIZE])
{
    uint32_t word=0;
    for(unsigned i=0;i<ARRAY_SIZE;i++) word|=(uint32_t)(uint8_t)data[i]<<(8*i);
    return word;
}
#endif
