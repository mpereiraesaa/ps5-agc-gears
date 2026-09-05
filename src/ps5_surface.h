#ifndef PS5_AGC_GEARS_SURFACE_H
#define PS5_AGC_GEARS_SURFACE_H

#include <stdint.h>

enum {
    PS5_SURFACE_BUFFER_COUNT = 2u,
};

#define PS5_SURFACE_ALLOCATION_BYTES UINT64_C(0x08000000)
#define PS5_SURFACE_BUFFER_STRIDE UINT64_C(0x04000000)
#define PS5_SURFACE_ALIGNMENT UINT64_C(0x00020000)
#define PS5_SURFACE_FORMAT_WORD UINT64_C(0x8000000022000000)

struct ps5_surface_plan {
    uint32_t width;
    uint32_t height;
    uint32_t pitch_width;
    uint32_t pitch_height;
    uint64_t tiled_footprint;
    uint64_t allocation_bytes;
    uint64_t buffer_offsets[PS5_SURFACE_BUFFER_COUNT];
    uint64_t format_word;
    uint32_t register_start_index;
    uint32_t register_count;
    uint32_t flip_mode;
    uint32_t flip_rate;
};

enum ps5_surface_result {
    PS5_SURFACE_OK = 0,
    PS5_SURFACE_INVALID = -1,
};

int ps5_surface_make_plan(uint32_t videoout_resolution,
                          struct ps5_surface_plan *plan);

#endif
