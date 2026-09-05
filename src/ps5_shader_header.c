#include "ps5_shader_header.h"

#include <string.h>

_Static_assert(sizeof(struct ps5_shader_specials) == 0x30, "specials ABI");
_Static_assert(offsetof(struct ps5_shader_arena, user_data) == 0x60,
               "user-data placement");
_Static_assert(offsetof(struct ps5_shader_arena, specials) == 0x98,
               "specials placement");
_Static_assert(offsetof(struct ps5_shader_arena, cx) == 0xc8,
               "CX placement");
_Static_assert(offsetof(struct ps5_shader_arena, sh) == 0x118,
               "SH placement");
_Static_assert(sizeof(struct ps5_shader_arena) == 0x148, "arena ABI");

static void *self_relative(void *field, void *target)
{
    return (void *)((uintptr_t)target - (uintptr_t)field);
}

int ps5_shader_header_build(struct ps5_shader_arena *arena, uint8_t type,
                            uint32_t shader_bytes,
                            const struct ps5_shader_metadata *metadata)
{
    if (!arena || !metadata || shader_bytes < 0x30u || (shader_bytes & 3u) ||
        (type != PS5_SHADER_PRE_RASTER && type != PS5_SHADER_PIXEL) ||
        metadata->pre_raster_cx_count != 10u ||
        metadata->pixel_cx_count != 9u || !metadata->pre_raster_cx ||
        !metadata->pixel_cx)
        return -1;
    memset(arena, 0, sizeof(*arena));
    arena->header.file_header = UINT32_C(0x34333231);
    arena->header.version = UINT32_C(0x18);
    arena->header.user_data = self_relative(&arena->header.user_data,
                                             &arena->user_data);
    arena->header.cx_registers = self_relative(&arena->header.cx_registers,
                                                arena->cx);
    arena->header.sh_registers = self_relative(&arena->header.sh_registers,
                                                arena->sh);
    arena->header.specials = self_relative(&arena->header.specials,
                                            &arena->specials);
    arena->header.header_size = sizeof(*arena);
    arena->header.shader_size = shader_bytes;
    arena->header.target = 5u;
    arena->header.special_sizes_bytes = sizeof(arena->specials);
    arena->header.type = type;
    arena->header.num_sh_registers = 6u;
    arena->specials.ge_cntl = (ps5_agc_register){0x25b,
        type == PS5_SHADER_PRE_RASTER ? metadata->ge_cntl : 0u};
    arena->specials.shader_stages_en = (ps5_agc_register){0x2d5,
        type == PS5_SHADER_PRE_RASTER ? metadata->shader_stages_en : 0u};
    arena->specials.gs_out_prim_type = (ps5_agc_register){0x2ce,
        type == PS5_SHADER_PRE_RASTER ? metadata->gs_out_prim_type : 0u};
    arena->specials.ge_user_vgpr_en.offset = 0x25c;
    arena->specials.draw_modifier = metadata->draw_modifier;
    if (type == PS5_SHADER_PIXEL) {
        arena->header.num_cx_registers = 9u;
        memcpy(arena->cx, metadata->pixel_cx, 9u * sizeof(ps5_agc_register));
        arena->sh[0].offset = arena->sh[1].offset = 0x006;
        arena->sh[2].offset = 0x008;
        arena->sh[3].offset = 0x009;
        arena->sh[4] = (ps5_agc_register){0x00a, metadata->ps_rsrc1};
        arena->sh[5] = (ps5_agc_register){0x00b, metadata->ps_rsrc2};
    } else {
        arena->header.num_cx_registers = 10u;
        memcpy(arena->cx, metadata->pre_raster_cx,
               10u * sizeof(ps5_agc_register));
        arena->sh[0].offset = arena->sh[1].offset = 0x080;
        arena->sh[2] = (ps5_agc_register){0x08a, metadata->gs_rsrc1};
        arena->sh[3] = (ps5_agc_register){0x08b, metadata->gs_rsrc2};
        arena->sh[4].offset = 0x0c8;
        arena->sh[5].offset = 0x0c9;
    }
    return 0;
}

int ps5_shader_header_validate(const struct ps5_shader_arena *arena,
                               uint8_t type, uint32_t shader_bytes,
                               const struct ps5_shader_metadata *metadata)
{
    if (!arena || !metadata || arena->header.file_header != 0x34333231u ||
        arena->header.version != 0x18u || arena->header.code ||
        arena->header.type != type || arena->header.target != 5u ||
        arena->header.header_size != sizeof(*arena) ||
        arena->header.shader_size != shader_bytes ||
        arena->header.num_sh_registers != 6u ||
        arena->specials.draw_modifier != metadata->draw_modifier)
        return -1;
    const uintptr_t user = (uintptr_t)&arena->header.user_data +
                           (uintptr_t)arena->header.user_data;
    const uintptr_t cx = (uintptr_t)&arena->header.cx_registers +
                         (uintptr_t)arena->header.cx_registers;
    const uintptr_t sh = (uintptr_t)&arena->header.sh_registers +
                         (uintptr_t)arena->header.sh_registers;
    const uintptr_t specials = (uintptr_t)&arena->header.specials +
                               (uintptr_t)arena->header.specials;
    if (user != (uintptr_t)&arena->user_data || cx != (uintptr_t)arena->cx ||
        sh != (uintptr_t)arena->sh ||
        specials != (uintptr_t)&arena->specials)
        return -2;
    if (type == PS5_SHADER_PIXEL)
        return arena->header.num_cx_registers == 9u &&
               arena->sh[4].value == metadata->ps_rsrc1 &&
               memcmp(arena->cx, metadata->pixel_cx,
                      9u * sizeof(ps5_agc_register)) == 0 ? 0 : -3;
    return arena->header.num_cx_registers == 10u &&
           arena->sh[2].value == metadata->gs_rsrc1 &&
           memcmp(arena->cx, metadata->pre_raster_cx,
                  10u * sizeof(ps5_agc_register)) == 0 ? 0 : -4;
}
