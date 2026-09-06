#include "ps5_cache_contract.h"

#include "ps5_gpu_span.h"

#include <limits.h>

int ps5_cache_cpu_to_gpu_plan(const void *gpu_mapping,
                              size_t gpu_mapping_bytes,
                              const void *written_address,
                              size_t written_bytes,
                              Ps5CpuToGpuPlan *plan)
{
    if (!plan || !ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes,
                                        written_address, written_bytes))
        return -1;
    const uintptr_t mapping = (uintptr_t)gpu_mapping;
    const uintptr_t written = (uintptr_t)written_address;
    const size_t offset = (size_t)(written - mapping);
    const size_t acquire_offset = offset & ~(size_t)255u;
    if (offset > SIZE_MAX - written_bytes)
        return -1;
    const size_t written_end = offset + written_bytes;
    if (written_end > SIZE_MAX - 255u)
        return -1;
    const size_t acquire_end = (written_end + 255u) & ~(size_t)255u;
    if (acquire_end > gpu_mapping_bytes ||
        acquire_end <= acquire_offset)
        return -1;
    *plan = (Ps5CpuToGpuPlan){
        .flush_address = written_address,
        .flush_bytes = written_bytes,
        .acquire_base = (const uint8_t *)gpu_mapping + acquire_offset,
        .acquire_bytes = acquire_end - acquire_offset,
        .gcr_control = PS5_GCR_CPU_TO_GPU,
    };
    return 0;
}

int ps5_cache_gpu_to_cpu_complete(uint64_t fence_value,
                                  uint64_t expected_token,
                                  uint64_t observed_token)
{
    return fence_value == 0u && expected_token != 0u &&
                   observed_token == expected_token
               ? 1
               : 0;
}

const enum ps5_cache_contract_step *ps5_cache_contract_steps(size_t *count)
{
    static const enum ps5_cache_contract_step steps[] = {
        PS5_CACHE_CPU_WRITE,
        PS5_CACHE_CPU_CLFLUSH,
        PS5_CACHE_CPU_MFENCE,
        PS5_CACHE_GPU_ACQUIRE_TEXTURE_GL2,
        PS5_CACHE_GPU_RELEASE_FENCE,
        PS5_CACHE_CPU_FENCE_ACQUIRE,
        PS5_CACHE_VIDEOOUT_TOKEN_MATCH,
        PS5_CACHE_RETIRE_RESOURCES,
    };
    if (!count)
        return 0;
    *count = sizeof(steps) / sizeof(steps[0]);
    return steps;
}
