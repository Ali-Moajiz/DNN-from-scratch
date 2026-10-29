#ifndef NPU_H
#define NPU_H
#include "controller.h"
#include "fetcher.h"
#include "sram.h"
#include "mac.h"
#include "activation.h"
#include "scale_clamp.h"
#include "fifo.h"
typedef struct {
    BusNetwork network;
    Controller controller;
    Fetcher fetcher;
    Sram sram;
    MacArray array;
    Activation activation;
    ScaleClamp scale;
    ResultFifo fifo;
} Npu;
typedef enum { NPU_RUN_ERROR,NPU_RUN_COMPLETE,NPU_RUN_LIMIT } NpuRunStatus;
bool npu_init(Npu *);
NpuRunStatus npu_run(Npu *,uint64_t);
#endif
