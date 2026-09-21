#include "ps5_pipeline.h"

#include <string.h>

_Static_assert(sizeof(struct ps5_pipeline_registers) == 0x318,
               "FW 12.02 pipeline register plan ABI");

static const uint32_t rt_offsets[PS5_PIPELINE_RT_REGISTERS] = {
    0x318, 0x31b, 0x31c, 0x31d, 0x31e, 0x31f, 0x321, 0x323,
    0x324, 0x325, 0x390, 0x398, 0x3a0, 0x3a8, 0x3b0, 0x3b8,
};

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

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
    uint32_t width, uint32_t height)
{
    if (!out || !linked_cx || !linked_uc ||
        !pre_raster_cx || !pixel_cx || !pre_raster_sh || !pixel_sh ||
        !width || !height || width > UINT16_MAX || height > UINT16_MAX)
        return -1;
    /* A NULL render target is a DEPTH-ONLY pass: the pass binds a depth
     * attachment and no colour one, which Vulkan expresses as
     * colorAttachmentCount = 0. The colour block still has to be emitted,
     * because these context registers are part of the plan's fixed layout, so
     * it is emitted with every value zero. That is not a placeholder: bits
     * [2,6] of CB_COLOR0_INFO (context offset 0x31c) are its FORMAT field and
     * COLOR_INVALID is 0, so an all-zero block IS the "no surface bound" state
     * this hardware reads. It is the same state RADV programs for an absent
     * colour attachment, which writes CB_COLOR0_INFO = FORMAT(COLOR_INVALID)
     * and leaves the rest of the CB_COLOR0 block alone
     * (radv_cmd_buffer.c radv_emit_framebuffer_state). RADV's RB+ exception,
     * which forces the first slot to COLOR_32 for a depth-only pass, does not
     * apply here: rbplus_allowed requires GFX10_3 or one of the named older
     * chips (ac_gpu_info.c), and this part is GFX10_1. */
    for (uint32_t i = 0; render_target && i < PS5_PIPELINE_RT_REGISTERS; ++i)
        if (render_target[i].offset != rt_offsets[i])
            return -2;

    memset(out, 0, sizeof(*out));
    if (render_target)
        memcpy(out->cx, render_target,
               PS5_PIPELINE_RT_REGISTERS * sizeof(ps5_agc_register));
    else
        for (uint32_t i = 0; i < PS5_PIPELINE_RT_REGISTERS; ++i)
            out->cx[i] = (ps5_agc_register){rt_offsets[i], 0u};
    const ps5_agc_register viewport[PS5_PIPELINE_VIEWPORT_REGISTERS] = {
        {0x10f, float_bits((float)width * 0.5f)},
        {0x110, float_bits((float)width * 0.5f)},
        {0x111, float_bits((float)height * -0.5f)},
        {0x112, float_bits((float)height * 0.5f)},
        {0x113, float_bits(1.0f)}, {0x114, 0u},
        {0x0b4, 0u}, {0x0b5, float_bits(1.0f)},
        {0x2fa, float_bits(1.0f)}, {0x2fb, float_bits(1.0f)},
        {0x2fc, float_bits(1.0f)}, {0x2fd, float_bits(1.0f)},
        {0x090, UINT32_C(0x80000000)},
        {0x091, width | (height << 16)},
        /* CB_TARGET_MASK (context offset 0x08e) names the colour channels the
         * colour target writes. A depth-only pass writes none, so the mask is
         * zero there; with a colour target the single render target keeps all
         * four channels enabled. */
        {0x08e, render_target ? UINT32_C(0x0000000f) : UINT32_C(0)},
    };
    memcpy(out->cx + PS5_PIPELINE_RT_REGISTERS, viewport, sizeof(viewport));
    memcpy(out->cx + PS5_PIPELINE_RT_REGISTERS +
               PS5_PIPELINE_VIEWPORT_REGISTERS,
           linked_cx, sizeof(*linked_cx));
    const uint32_t shader_base = PS5_PIPELINE_RT_REGISTERS +
                                 PS5_PIPELINE_VIEWPORT_REGISTERS +
                                 PS5_PIPELINE_LINKED_CX_REGISTERS;
    memcpy(out->cx + shader_base, pre_raster_cx,
           PS5_PIPELINE_PRE_RASTER_CX_REGISTERS * sizeof(ps5_agc_register));
    memcpy(out->cx + shader_base + PS5_PIPELINE_PRE_RASTER_CX_REGISTERS,
           pixel_cx,
           PS5_PIPELINE_PIXEL_CX_REGISTERS * sizeof(ps5_agc_register));
    memcpy(out->sh, pre_raster_sh, 6u * sizeof(ps5_agc_register));
    memcpy(out->sh + 6, pixel_sh, 6u * sizeof(ps5_agc_register));
    memcpy(out->uc, linked_uc, sizeof(*linked_uc));
    return 0;
}
