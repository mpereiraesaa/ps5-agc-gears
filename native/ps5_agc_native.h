#ifndef PS5_AGC_GEARS_AGC_NATIVE_H
#define PS5_AGC_GEARS_AGC_NATIVE_H

#include "../src/ps5_agc_submit.h"

int ps5_native_set_flip(uint32_t **cursor, uint32_t capacity_dwords,
                        uint32_t driver_mode, int32_t videoout_handle,
                        int32_t buffer_index, uint32_t flip_mode,
                        uint64_t flip_arg);
int ps5_native_draw_auto(uint32_t **cursor, uint32_t capacity_dwords,
                         uint32_t vertex_count, uint64_t modifier);
int ps5_native_draw_index(uint32_t **cursor, uint32_t capacity_dwords,
                          uint32_t index_count, const uint16_t *gpu_indices,
                          const void *gpu_mapping, size_t gpu_mapping_bytes,
                          uint64_t modifier);
int ps5_native_set_sh_direct(uint32_t **cursor, uint32_t capacity_dwords,
                             uint32_t compact_offset,
                             const uint32_t *values, uint32_t count);
enum ps5_native_register_bank {
    PS5_NATIVE_REGISTERS_CX = 0,
    PS5_NATIVE_REGISTERS_UC = 1,
    PS5_NATIVE_REGISTERS_SH = 2,
};
int ps5_native_set_indirect(uint32_t **cursor, uint32_t capacity_dwords,
                            const void *registers, uint32_t count,
                            const void *gpu_mapping,
                            size_t gpu_mapping_bytes,
                            enum ps5_native_register_bank bank);
int ps5_native_fill_depth(uint32_t **cursor, uint32_t capacity_dwords,
                          void *destination, uint32_t repeated_word,
                          uint32_t byte_count, const void *gpu_mapping,
                          size_t gpu_mapping_bytes);
int ps5_native_wait_rendering(uint32_t **cursor, uint32_t capacity_dwords,
                              uint32_t driver_mode, int32_t videoout_handle,
                              int32_t buffer_index);
int ps5_native_acquire_mem(uint32_t **cursor, uint32_t capacity_dwords,
                           const void *base, uint64_t bytes, uint8_t engine,
                           uint32_t gcr_control, uint32_t poll_cycles,
                           const void *gpu_mapping, size_t gpu_mapping_bytes);
void ps5_native_cache_flush(const void *address, size_t bytes);

void ps5_native_submit_context_init(struct ps5_agc_submit_context *context,
                                    const void *gpu_mapping,
                                    size_t gpu_mapping_bytes);

#endif
