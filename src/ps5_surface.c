#include "ps5_surface.h"

#include <string.h>

enum {
    PS5_TILE_WIDTH = 512u,
    PS5_TILE_HEIGHT = 128u,
    PS5_COLOR_BYTES_PER_PIXEL = 4u,
};

static uint64_t surface_tiled_footprint(uint32_t width, uint32_t height)
{
    const uint64_t tile_pixels =
        (uint64_t)PS5_TILE_WIDTH * PS5_TILE_HEIGHT;
    const uint64_t last_band =
        ((uint64_t)(height - 1u) / PS5_TILE_HEIGHT) * width * PS5_TILE_HEIGHT;
    const uint64_t last_tile =
        ((uint64_t)(width - 1u) / PS5_TILE_WIDTH) * tile_pixels;
    return (last_band + last_tile + tile_pixels) *
           PS5_COLOR_BYTES_PER_PIXEL;
}

int ps5_surface_make_plan(uint32_t videoout_resolution,
                          struct ps5_surface_plan *plan)
{
    if (!plan)
        return PS5_SURFACE_INVALID;

    memset(plan, 0, sizeof(*plan));
    plan->width = videoout_resolution == 2u ? 3840u : 1920u;
    plan->height = videoout_resolution == 2u ? 2160u : 1080u;
    plan->pitch_width = (plan->width + 63u) & ~63u;
    plan->pitch_height = (plan->height + 63u) & ~63u;
    plan->tiled_footprint = surface_tiled_footprint(plan->width, plan->height);
    plan->allocation_bytes = PS5_SURFACE_ALLOCATION_BYTES;
    plan->buffer_offsets[1] = PS5_SURFACE_BUFFER_STRIDE;
    plan->format_word = PS5_SURFACE_FORMAT_WORD;
    plan->register_count = PS5_SURFACE_BUFFER_COUNT;
    plan->flip_mode = 1u;

    if (plan->tiled_footprint > PS5_SURFACE_BUFFER_STRIDE ||
        plan->buffer_offsets[1] + plan->tiled_footprint >
            plan->allocation_bytes)
        return PS5_SURFACE_INVALID;
    return PS5_SURFACE_OK;
}
