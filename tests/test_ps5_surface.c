#include "../src/ps5_surface.h"

#include <assert.h>

int main(void)
{
    struct ps5_surface_plan plan;
    assert(ps5_surface_make_plan(2u, &plan) == PS5_SURFACE_OK);
    assert(plan.width == 3840u && plan.height == 2160u);
    assert(plan.pitch_width == 3840u && plan.pitch_height == 2176u);
    assert(plan.tiled_footprint == UINT64_C(0x02000000));
    assert(plan.buffer_offsets[0] == 0u);
    assert(plan.buffer_offsets[1] == PS5_SURFACE_BUFFER_STRIDE);
    assert(plan.buffer_offsets[1] + plan.tiled_footprint <=
           plan.allocation_bytes);
    assert(plan.format_word == PS5_SURFACE_FORMAT_WORD);
    assert(plan.register_start_index == 0u && plan.register_count == 2u);
    assert(plan.flip_mode == 1u && plan.flip_rate == 0u);

    assert(ps5_surface_make_plan(0u, &plan) == PS5_SURFACE_OK);
    assert(plan.width == 1920u && plan.height == 1080u);
    assert(plan.pitch_width == 1920u && plan.pitch_height == 1088u);
    assert(plan.tiled_footprint == UINT64_C(0x00880000));
    assert(ps5_surface_make_plan(2u, 0) == PS5_SURFACE_INVALID);
    return 0;
}
