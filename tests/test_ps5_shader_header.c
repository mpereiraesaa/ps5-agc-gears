#include "../src/ps5_shader_header.h"

#include <assert.h>
#include <string.h>

static const ps5_agc_register pre_cx[10] = {
    {0x1ff, 0x80}, {0x2d3, 0x20001}, {0x207, 0}, {0x1c2, 1}, {0x1c3, 4},
    {0x1b1, 0}, {0x2ab, 1}, {0x2e4, 0}, {0x2ce, 1}, {0x291, 0x2004007e},
};
static const ps5_agc_register pixel_cx[9] = {
    {0x08f, 0x0f}, {0x203, 0x810}, {0x310, 0}, {0x1b8, 0x01000000},
    {0x1b4, 2}, {0x1b3, 2}, {0x1b6, 2}, {0x1c5, 4}, {0x1c4, 0},
};
static const struct ps5_shader_metadata metadata = {
    .gs_rsrc1 = 0x2a2c0142, .gs_rsrc2 = 0x38,
    .ps_rsrc1 = 0x022c0001, .ps_rsrc2 = 2,
    .ge_cntl = 0xfc80, .shader_stages_en = 0x02412010,
    .gs_out_prim_type = 2, .draw_modifier = 5,
    .pre_raster_cx = pre_cx, .pre_raster_cx_count = 10,
    .pixel_cx = pixel_cx, .pixel_cx_count = 9,
};

static void verify(uint8_t type, uint32_t bytes)
{
    struct ps5_shader_arena arena;
    assert(ps5_shader_header_build(&arena, type, bytes, &metadata) == 0);
    assert(ps5_shader_header_validate(&arena, type, bytes, &metadata) == 0);
    assert(arena.specials.draw_modifier == 5u);
    if (type == PS5_SHADER_PRE_RASTER) {
        assert(arena.sh[2].value == metadata.gs_rsrc1);
        assert(arena.sh[4].offset == 0x0c8u);
    } else {
        assert(arena.sh[4].value == metadata.ps_rsrc1);
        assert(arena.sh[2].offset == 0x008u);
    }
}

int main(void)
{
    verify(PS5_SHADER_PRE_RASTER, 384u + 48u);
    verify(PS5_SHADER_PIXEL, 160u + 48u);
    struct ps5_shader_arena arena;
    memset(&arena, 0xa5, sizeof(arena));
    assert(ps5_shader_header_build(&arena, 0u, 28u, &metadata) == -1);
    assert(ps5_shader_header_build(&arena, PS5_SHADER_PIXEL, 44u,
                                   &metadata) == -1);
    return 0;
}
