#include "ps5_agc_native.h"

#include "../include/ps5_agc.h"
#include "../include/ps5_agc_driver.h"
#include "../src/ps5_agc_writer.h"

int ps5_native_set_flip(uint32_t **cursor, uint32_t capacity,
                        uint32_t driver_mode, int32_t handle,
                        int32_t buffer_index, uint32_t flip_mode,
                        uint64_t flip_arg)
{
    return ps5_agc_writer_set_flip(cursor, capacity, driver_mode, handle,
                                   buffer_index, flip_mode, flip_arg,
                                   sceAgcDcbSetFlip);
}

int ps5_native_draw_auto(uint32_t **cursor, uint32_t capacity,
                         uint32_t vertex_count, uint64_t modifier)
{
    return ps5_agc_writer_draw_auto(cursor, capacity, vertex_count, modifier,
                                    sceAgcDcbDrawIndexAuto);
}

int ps5_native_draw_index(uint32_t **cursor, uint32_t capacity,
                          uint32_t index_count, const uint16_t *gpu_indices,
                          const void *gpu_mapping, size_t gpu_mapping_bytes,
                          uint64_t modifier)
{
    return ps5_agc_writer_draw_index(cursor, capacity, index_count,
                                     gpu_indices, gpu_mapping,
                                     gpu_mapping_bytes, modifier,
                                     sceAgcDcbDrawIndex);
}

int ps5_native_set_sh_direct(uint32_t **cursor, uint32_t capacity,
                             uint32_t offset, const uint32_t *values,
                             uint32_t count)
{
    return ps5_agc_writer_set_sh_direct(cursor, capacity, offset, values,
                                        count,
                                        sceAgcCbSetShRegisterRangeDirect);
}

int ps5_native_set_indirect(uint32_t **cursor, uint32_t capacity,
                            const void *registers, uint32_t count,
                            const void *gpu_mapping, size_t gpu_mapping_bytes,
                            enum ps5_native_register_bank bank)
{
    ps5_agc_emit_indirect_fn emit = 0;
    if (bank == PS5_NATIVE_REGISTERS_CX)
        emit = sceAgcDcbSetCxRegistersIndirect;
    else if (bank == PS5_NATIVE_REGISTERS_UC)
        emit = sceAgcDcbSetUcRegistersIndirect;
    else if (bank == PS5_NATIVE_REGISTERS_SH)
        emit = sceAgcDcbSetShRegistersIndirect;
    else
        return PS5_AGC_WRITER_PRECONDITION;
    return ps5_agc_writer_set_indirect(cursor, capacity, registers, count,
                                       gpu_mapping, gpu_mapping_bytes, emit);
}

static uint32_t *emit_depth_fill(void *writer, void *destination,
                                 uint32_t repeated_word, uint32_t byte_count)
{
    return ps5_agc_dcb_fill_l2_sync(writer, destination, repeated_word,
                                    byte_count);
}

int ps5_native_fill_depth(uint32_t **cursor, uint32_t capacity,
                          void *destination, uint32_t repeated_word,
                          uint32_t byte_count, const void *gpu_mapping,
                          size_t gpu_mapping_bytes)
{
    return ps5_agc_writer_fill_depth(cursor, capacity, destination,
                                     repeated_word, byte_count, gpu_mapping,
                                     gpu_mapping_bytes, emit_depth_fill);
}

int ps5_native_wait_rendering(uint32_t **cursor, uint32_t capacity,
                              uint32_t driver_mode, int32_t handle,
                              int32_t buffer_index)
{
    return ps5_agc_writer_wait_rendering(
        cursor, capacity, driver_mode, handle, buffer_index,
        sceAgcDriverGetWaitRenderingPacketSizeInDwords,
        sceAgcDriverWaitUntilSafeForRendering);
}

void ps5_native_cache_flush(const void *address, size_t bytes)
{
    const unsigned char *line = address;
    const unsigned char *const end = line + bytes;
    for (; line < end; line += 64u)
        __asm__ volatile("clflush (%0)" : : "r"(line) : "memory");
    __asm__ volatile("mfence" : : : "memory");
}

void ps5_native_submit_context_init(struct ps5_agc_submit_context *context,
                                    const void *gpu_mapping,
                                    size_t gpu_mapping_bytes)
{
    if (!context)
        return;
    context->gpu_mapping = gpu_mapping;
    context->gpu_mapping_bytes = gpu_mapping_bytes;
    context->flush = ps5_native_cache_flush;
    context->submit = sceAgcDriverSubmitDcb;
}
