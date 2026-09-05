#ifndef PS5_AGC_GEARS_PRESENT_H
#define PS5_AGC_GEARS_PRESENT_H

#include <stdint.h>

enum {
    PS5_PRESENT_SET_FLIP_MAX_DWORDS = 64u,
    PS5_PRESENT_RELEASE_DWORDS = 8u,
};

typedef int (*ps5_present_set_flip_fn)(
    uint32_t **cursor, uint32_t capacity_dwords, uint32_t driver_mode,
    int32_t videoout_handle, int32_t buffer_index, uint32_t flip_mode,
    uint64_t flip_arg);

struct ps5_present_stream {
    uint32_t *start;
    uint32_t *end;
    uint32_t *cursor;
    int transaction_started;
};

enum ps5_present_result {
    PS5_PRESENT_OK = 0,
    PS5_PRESENT_PRECONDITION = -1,
    PS5_PRESENT_BUILDER_ERROR = -2,
    PS5_PRESENT_CURSOR_INVALID = -3,
};

int ps5_present_compose_flip_and_fence(
    struct ps5_present_stream *stream,
    ps5_present_set_flip_fn set_flip,
    int32_t videoout_handle,
    int32_t buffer_index,
    uint32_t flip_mode,
    uint64_t flip_arg,
    uintptr_t fence_address);

#endif
