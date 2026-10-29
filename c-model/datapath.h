#ifndef DATAPATH_H
#define DATAPATH_H
#include "bus.h"
#define ARRAY_SIZE 4
#define NETWORK_LAYERS 2
#define LAYER_CONFIG_WORDS 3u
#define LAYER_SETUP_READS (LAYER_CONFIG_WORDS + 4u + 4u + 1u)
#define LAYER_SETUP_EDGES (2u * LAYER_SETUP_READS)
#define LAYER_COMPUTE_EDGES 7u
#define LAYER_TOTAL_EDGES (LAYER_SETUP_EDGES + LAYER_COMPUTE_EDGES + 2u)
#define NPU_INFERENCE_CYCLES (NETWORK_LAYERS * LAYER_TOTAL_EDGES)
/* Word addresses, not byte addresses. W is output-row x input-column. */
typedef struct {
    uint32_t weight_base, bias_base, input_base, output_base, config_base;
} LayerConfig;
typedef enum { EDGE_READ_REQ, EDGE_READ_CAP, EDGE_COMPUTE, EDGE_POST, EDGE_STORE } EdgeKind;
typedef enum { LOAD_WEIGHT, LOAD_BIAS, LOAD_INPUT, LOAD_CONFIG } LoadKind;
/* Combinational decoder outputs. These are asserted for one edge only. */
typedef struct {
    EdgeKind kind;
    LoadKind load;
    uint32_t address;
    unsigned layer, index, wave;
    int32_t multiplier;
    uint8_t shift;
    bool relu;
} Control;
typedef struct { LoadKind kind; unsigned index; uint32_t word; } FetchLoad;
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
