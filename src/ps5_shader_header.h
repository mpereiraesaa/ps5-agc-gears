#ifndef PS5_AGC_GEARS_SHADER_HEADER_H
#define PS5_AGC_GEARS_SHADER_HEADER_H

#include "../include/ps5_shader.h"

enum {
    PS5_SHADER_PIXEL = 1,
    PS5_SHADER_PRE_RASTER = 2,
    PS5_SHADER_MAX_CX_REGISTERS = 10,
};

struct ps5_shader_metadata {
    uint32_t gs_rsrc1, gs_rsrc2;
    uint32_t ps_rsrc1, ps_rsrc2;
    uint32_t ge_cntl, shader_stages_en, gs_out_prim_type;
    uint64_t draw_modifier;
    const ps5_agc_register *pre_raster_cx;
    uint32_t pre_raster_cx_count;
    const ps5_agc_register *pixel_cx;
    uint32_t pixel_cx_count;
};

struct ps5_shader_specials {
    ps5_agc_register ge_cntl;
    ps5_agc_register shader_stages_en;
    uint32_t dispatch_modifier;
    uint16_t user_data_range_start;
    uint16_t user_data_range_end;
    uint64_t draw_modifier;
    ps5_agc_register gs_out_prim_type;
    ps5_agc_register ge_user_vgpr_en;
};

struct ps5_shader_arena {
    struct ps5_shader_header header;
    struct ps5_shader_user_data user_data;
    struct ps5_shader_specials specials;
    ps5_agc_register cx[PS5_SHADER_MAX_CX_REGISTERS];
    ps5_agc_register sh[6];
};

int ps5_shader_header_build(struct ps5_shader_arena *arena, uint8_t type,
                            uint32_t shader_bytes,
                            const struct ps5_shader_metadata *metadata);
int ps5_shader_header_validate(const struct ps5_shader_arena *arena,
                               uint8_t type, uint32_t shader_bytes,
                               const struct ps5_shader_metadata *metadata);

#endif
