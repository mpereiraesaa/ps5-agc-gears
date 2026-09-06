#ifndef PS5_AGC_GEARS_AGC_H
#define PS5_AGC_GEARS_AGC_H

/* Clean-room declarations for the subset exercised on FW 12.02.
 * This is a homebrew compatibility interface, not an official SDK header. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ps5_agc_register {
    uint32_t offset;
    uint32_t value;
} ps5_agc_register;

int32_t sceAgcInit(void *state, uint32_t size);
int32_t sceAgcCreateShader(void **shader, void *header, void *code);
int32_t sceAgcLinkShaders(void *cx, void *uc, void *reserved,
                          void *pre_raster, void *pixel, uint32_t primitive);
void *sceAgcGetRegisterDefaults(void);
uint32_t *sceAgcDcbSetCxRegistersIndirect(
    void *writer, const void *registers, uint32_t count);
uint32_t *sceAgcDcbSetUcRegistersIndirect(
    void *writer, const void *registers, uint32_t count);
uint32_t *sceAgcDcbSetShRegistersIndirect(
    void *writer, const void *registers, uint32_t count);
uint32_t *sceAgcCbSetShRegisterRangeDirect(
    void *writer, uint32_t compact_offset, const uint32_t *values,
    uint32_t count);
uint32_t *sceAgcDcbDrawIndexAuto(
    void *writer, uint32_t vertex_count, uint64_t modifier);
uint32_t *sceAgcDcbDrawIndex(
    void *writer, uint32_t index_count, const void *gpu_index_address,
    uint64_t modifier);
uint32_t *sceAgcDcbSetFlip(
    void *writer, uint32_t video_handle, int32_t buffer_index,
    uint32_t flip_mode, int64_t flip_arg);
uint32_t *sceAgcDcbDmaData(
    void *writer, uint32_t arg2, uint32_t dst_select, uint32_t arg4,
    uint64_t destination, uint32_t src_select, uint32_t arg7,
    uint64_t source_or_immediate, uint32_t byte_count,
    uint32_t raw_wait, uint32_t disable_write_confirm, uint32_t cp_sync);

static inline uint32_t *ps5_agc_dcb_fill_l2_sync(
    void *writer, void *gpu_destination, uint32_t repeated_word,
    uint32_t byte_count)
{
    return sceAgcDcbDmaData(
        writer, 0u, 3u, 0u, (uint64_t)(uintptr_t)gpu_destination,
        2u, 0u, repeated_word, byte_count, 0u, 0u, 1u);
}

enum {
    PS5_AGC_SH_USER_DATA_GS_0 = 0x8c,
    PS5_AGC_GEARS_PARAMETER_OFFSET = 0x8d,
    PS5_AGC_GEARS_PARAMETER_WORDS = 24,
    PS5_AGC_GEARS_DRAW_WORDS = 25,
};

#ifdef __cplusplus
}
#endif

#endif
