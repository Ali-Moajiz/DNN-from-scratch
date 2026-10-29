#include "npu.h"
#include "logging.h"
int main(void)
{
    Npu system;

    if (!npu_init(&system))
    {
        log_error(0, "NPU", "initialization failed");
        return 1;
    }
    
    return npu_run(&system, 100) == NPU_RUN_COMPLETE ? 0 : 1;
}
