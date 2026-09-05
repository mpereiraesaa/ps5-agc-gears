#ifndef PS5_AGC_GEARS_PIPELINE_H
#define PS5_AGC_GEARS_PIPELINE_H

#include "../include/ps5_agc.h"
#include "../include/ps5_agc_registers.h"

enum {
    PS5_PIPELINE_RT_REGISTERS = 16,
    PS5_PIPELINE_VIEWPORT_REGISTERS = 15,
    PS5_PIPELINE_LINKED_CX_REGISTERS = 34,
    PS5_PIPELINE_PRE_RASTER_CX_REGISTERS = 10,
    PS5_PIPELINE_PIXEL_CX_REGISTERS = 9,
    PS5_PIPELINE_CX_REGISTERS = 84,
    PS5_PIPELINE_SH_REGISTERS = 12,
    PS5_PIPELINE_UC_REGISTERS = 3,
};

struct ps5_pipeline_registers {
    ps5_agc_register cx[PS5_PIPELINE_CX_REGISTERS];
    ps5_agc_register sh[PS5_PIPELINE_SH_REGISTERS];
    ps5_agc_register uc[PS5_PIPELINE_UC_REGISTERS];
};

int ps5_pipeline_build(
    struct ps5_pipeline_registers *out,
    const ps5_agc_register render_target[PS5_PIPELINE_RT_REGISTERS],
    const struct ps5_agc_linked_cx *linked_cx,
    const struct ps5_agc_linked_uc *linked_uc,
    const ps5_agc_register
        pre_raster_cx[PS5_PIPELINE_PRE_RASTER_CX_REGISTERS],
    const ps5_agc_register pixel_cx[PS5_PIPELINE_PIXEL_CX_REGISTERS],
    const ps5_agc_register pre_raster_sh[6],
    const ps5_agc_register pixel_sh[6],
    uint32_t width, uint32_t height);

#endif
