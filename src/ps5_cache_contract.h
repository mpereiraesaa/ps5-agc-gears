#ifndef PS5_AGC_GEARS_CACHE_CONTRACT_H
#define PS5_AGC_GEARS_CACHE_CONTRACT_H

#include <stddef.h>
#include <stdint.h>

enum {
    PS5_GCR_GLK_INVALIDATE = 1u << 7,
    PS5_GCR_GLV_INVALIDATE = 1u << 8,
    PS5_GCR_GL1_INVALIDATE = 1u << 9,
    PS5_GCR_GL2_INVALIDATE = 1u << 14,
    PS5_GCR_CPU_TO_GPU = PS5_GCR_GLK_INVALIDATE |
                         PS5_GCR_GLV_INVALIDATE |
                         PS5_GCR_GL1_INVALIDATE |
                         PS5_GCR_GL2_INVALIDATE,
};

enum ps5_cache_contract_step {
    PS5_CACHE_CPU_WRITE = 1,
    PS5_CACHE_CPU_CLFLUSH = 2,
    PS5_CACHE_CPU_MFENCE = 3,
    PS5_CACHE_GPU_ACQUIRE_TEXTURE_GL2 = 4,
    PS5_CACHE_GPU_RELEASE_FENCE = 5,
    PS5_CACHE_CPU_FENCE_ACQUIRE = 6,
    PS5_CACHE_VIDEOOUT_TOKEN_MATCH = 7,
    PS5_CACHE_RETIRE_RESOURCES = 8,
};

typedef struct Ps5CpuToGpuPlan {
    const void *flush_address;
    size_t flush_bytes;
    const void *acquire_base;
    uint64_t acquire_bytes;
    uint32_t gcr_control;
} Ps5CpuToGpuPlan;

int ps5_cache_cpu_to_gpu_plan(const void *gpu_mapping,
                              size_t gpu_mapping_bytes,
                              const void *written_address,
                              size_t written_bytes,
                              Ps5CpuToGpuPlan *plan);
int ps5_cache_gpu_to_cpu_complete(uint64_t fence_value,
                                  uint64_t expected_token,
                                  uint64_t observed_token);
const enum ps5_cache_contract_step *ps5_cache_contract_steps(size_t *count);

#endif
