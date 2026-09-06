/* Link-time facade. The native module loader resolves these imports from
 * libSceAgc at runtime; no implementation is shipped in this stub. */
#include "../../include/ps5_agc.h"

#define UNUSED(value) ((void)(value))

int32_t sceAgcInit(void *state, uint32_t size)
{
    UNUSED(state); UNUSED(size); return -1;
}
int32_t sceAgcCreateShader(void **shader, void *header, void *code)
{
    UNUSED(shader); UNUSED(header); UNUSED(code); return -1;
}
int32_t sceAgcLinkShaders(void *cx, void *uc, void *reserved,
                          void *pre_raster, void *pixel, uint32_t primitive)
{
    UNUSED(cx); UNUSED(uc); UNUSED(reserved); UNUSED(pre_raster);
    UNUSED(pixel); UNUSED(primitive); return -1;
}
void *sceAgcGetRegisterDefaults(void) { return 0; }
uint32_t *sceAgcDcbSetCxRegistersIndirect(
    void *writer, const void *registers, uint32_t count)
{
    UNUSED(writer); UNUSED(registers); UNUSED(count); return 0;
}
uint32_t *sceAgcDcbSetUcRegistersIndirect(
    void *writer, const void *registers, uint32_t count)
{
    UNUSED(writer); UNUSED(registers); UNUSED(count); return 0;
}
uint32_t *sceAgcDcbSetShRegistersIndirect(
    void *writer, const void *registers, uint32_t count)
{
    UNUSED(writer); UNUSED(registers); UNUSED(count); return 0;
}
uint32_t *sceAgcCbSetShRegisterRangeDirect(
    void *writer, uint32_t offset, const uint32_t *values, uint32_t count)
{
    UNUSED(writer); UNUSED(offset); UNUSED(values); UNUSED(count); return 0;
}
uint32_t *sceAgcDcbDrawIndexAuto(
    void *writer, uint32_t vertex_count, uint64_t modifier)
{
    UNUSED(writer); UNUSED(vertex_count); UNUSED(modifier); return 0;
}
uint32_t *sceAgcDcbDrawIndex(
    void *writer, uint32_t index_count, const void *gpu_index_address,
    uint64_t modifier)
{
    UNUSED(writer); UNUSED(index_count); UNUSED(gpu_index_address);
    UNUSED(modifier); return 0;
}
uint32_t *sceAgcDcbSetFlip(
    void *writer, uint32_t handle, int32_t buffer_index,
    uint32_t flip_mode, int64_t flip_arg)
{
    UNUSED(writer); UNUSED(handle); UNUSED(buffer_index); UNUSED(flip_mode);
    UNUSED(flip_arg); return 0;
}
uint32_t *sceAgcDcbDmaData(
    void *writer, uint32_t arg2, uint32_t dst_select, uint32_t arg4,
    uint64_t destination, uint32_t src_select, uint32_t arg7,
    uint64_t source, uint32_t byte_count, uint32_t raw_wait,
    uint32_t disable_write_confirm, uint32_t cp_sync)
{
    UNUSED(writer); UNUSED(arg2); UNUSED(dst_select); UNUSED(arg4);
    UNUSED(destination); UNUSED(src_select); UNUSED(arg7); UNUSED(source);
    UNUSED(byte_count); UNUSED(raw_wait); UNUSED(disable_write_confirm);
    UNUSED(cp_sync); return 0;
}
uint32_t *sceAgcDcbAcquireMem(
    void *writer, uint8_t engine, uint32_t cb_db_op, uint32_t gcr_control,
    const volatile void *base, uint64_t size_bytes, uint32_t poll_cycles)
{
    UNUSED(writer); UNUSED(engine); UNUSED(cb_db_op); UNUSED(gcr_control);
    UNUSED(base); UNUSED(size_bytes); UNUSED(poll_cycles); return 0;
}
