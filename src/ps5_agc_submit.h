#ifndef PS5_AGC_GEARS_AGC_SUBMIT_H
#define PS5_AGC_GEARS_AGC_SUBMIT_H

#include <stddef.h>
#include <stdint.h>

typedef void (*ps5_agc_cache_flush_fn)(const void *address, size_t bytes);
typedef int32_t (*ps5_agc_driver_submit_fn)(void *description);

struct ps5_agc_submit_context {
    const void *gpu_mapping;
    size_t gpu_mapping_bytes;
    ps5_agc_cache_flush_fn flush;
    ps5_agc_driver_submit_fn submit;
};

enum ps5_agc_submit_result {
    PS5_AGC_SUBMIT_OK = 0,
    PS5_AGC_SUBMIT_PRECONDITION = -1,
    PS5_AGC_SUBMIT_NOT_GPU_VISIBLE = -2,
};

int ps5_agc_submit_checked(const uint32_t *commands, uint32_t dwords,
                           void *opaque);

#endif
