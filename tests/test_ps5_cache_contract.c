#include "../src/ps5_cache_contract.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    _Alignas(256) uint8_t mapping[1024] = {0};
    Ps5CpuToGpuPlan plan;
    assert(ps5_cache_cpu_to_gpu_plan(mapping, sizeof(mapping),
                                     mapping + 300u, 300u, &plan) == 0);
    assert(plan.flush_address == mapping + 300u && plan.flush_bytes == 300u);
    assert(plan.acquire_base == mapping + 256u && plan.acquire_bytes == 512u);
    assert(plan.engine == 1u);
    assert(plan.gcr_control == UINT32_C(0x9000));
    assert(plan.poll_cycles == UINT32_C(0x190));
    assert(ps5_cache_cpu_to_gpu_plan(mapping, sizeof(mapping),
                                     mapping + 900u, 200u, &plan) != 0);

    size_t count = 0u;
    const enum ps5_cache_contract_step *steps =
        ps5_cache_contract_steps(&count);
    const enum ps5_cache_contract_step expected[] = {
        PS5_CACHE_CPU_WRITE, PS5_CACHE_CPU_CLFLUSH, PS5_CACHE_CPU_MFENCE,
        PS5_CACHE_GPU_ACQUIRE_RANGE, PS5_CACHE_GPU_RELEASE_FENCE,
        PS5_CACHE_CPU_FENCE_ACQUIRE, PS5_CACHE_VIDEOOUT_TOKEN_MATCH,
        PS5_CACHE_RETIRE_RESOURCES,
    };
    assert(steps && count == sizeof(expected) / sizeof(expected[0]));
    for (size_t index = 0; index < count; ++index)
        assert(steps[index] == expected[index]);
    assert(ps5_cache_gpu_to_cpu_complete(0u, 91u, 91u));
    assert(!ps5_cache_gpu_to_cpu_complete(1u, 91u, 91u));
    assert(!ps5_cache_gpu_to_cpu_complete(0u, 91u, 92u));
    return 0;
}
