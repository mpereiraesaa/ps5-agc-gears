#ifndef PS5_AGC_GEARS_AGC_WRITER_H
#define PS5_AGC_GEARS_AGC_WRITER_H

#include "../include/ps5_platform.h"

#include <stddef.h>
#include <stdint.h>

typedef uint32_t *(*ps5_agc_emit_flip_fn)(
    void *writer, uint32_t video_handle, int32_t buffer_index,
    uint32_t flip_mode, int64_t flip_arg);
typedef uint32_t *(*ps5_agc_emit_draw_auto_fn)(
    void *writer, uint32_t vertex_count, uint64_t modifier);
typedef uint32_t *(*ps5_agc_emit_draw_index_fn)(
    void *writer, uint32_t index_count, const void *gpu_index_address,
    uint64_t modifier);
typedef uint32_t *(*ps5_agc_emit_sh_direct_fn)(
    void *writer, uint32_t compact_offset, const uint32_t *values,
    uint32_t count);
typedef uint32_t *(*ps5_agc_emit_indirect_fn)(
    void *writer, const void *registers, uint32_t count);
typedef uint32_t *(*ps5_agc_emit_dma_fill_fn)(
    void *writer, void *destination, uint32_t repeated_word,
    uint32_t byte_count);
typedef uint32_t *(*ps5_agc_emit_acquire_mem_fn)(
    void *writer, uint8_t engine, uint32_t cb_db_op, uint32_t gcr_control,
    const volatile void *base, uint64_t size_bytes, uint32_t poll_cycles);
typedef uint32_t (*ps5_agc_wait_size_fn)(void);
typedef uint32_t (*ps5_agc_wait_emit_fn)(
    uint32_t **cursor, uint32_t packet_dwords, uint32_t reserved,
    uint32_t video_handle, int32_t buffer_index);

enum ps5_agc_writer_result {
    PS5_AGC_WRITER_OK = 0,
    PS5_AGC_WRITER_PRECONDITION = -1,
    PS5_AGC_WRITER_CURSOR_INVALID = -2,
    PS5_AGC_WRITER_NOT_GPU_VISIBLE = -3,
    PS5_AGC_WRITER_PACKET_SIZE = -4,
    PS5_AGC_WRITER_EMIT_FAILED = -5,
};

int ps5_agc_writer_set_flip(
    uint32_t **cursor, uint32_t capacity_dwords, uint32_t driver_mode,
    int32_t video_handle, int32_t buffer_index, uint32_t flip_mode,
    uint64_t flip_arg, ps5_agc_emit_flip_fn emit);
int ps5_agc_writer_draw_auto(
    uint32_t **cursor, uint32_t capacity_dwords, uint32_t vertex_count,
    uint64_t modifier, ps5_agc_emit_draw_auto_fn emit);
int ps5_agc_writer_draw_index(
    uint32_t **cursor, uint32_t capacity_dwords, uint32_t index_count,
    const uint16_t *gpu_indices, const void *gpu_mapping,
    size_t gpu_mapping_bytes, uint64_t modifier,
    ps5_agc_emit_draw_index_fn emit);
int ps5_agc_writer_set_sh_direct(
    uint32_t **cursor, uint32_t capacity_dwords, uint32_t compact_offset,
    const uint32_t *values, uint32_t count, ps5_agc_emit_sh_direct_fn emit);
int ps5_agc_writer_set_indirect(
    uint32_t **cursor, uint32_t capacity_dwords, const void *registers,
    uint32_t count, const void *gpu_mapping, size_t gpu_mapping_bytes,
    ps5_agc_emit_indirect_fn emit);

/* This operation exists for depth/metadata initialization only. Color clears
 * are draws through the render-target pipeline. */
int ps5_agc_writer_fill_depth(
    uint32_t **cursor, uint32_t capacity_dwords, void *destination,
    uint32_t repeated_word, uint32_t byte_count,
    const void *gpu_mapping, size_t gpu_mapping_bytes,
    ps5_agc_emit_dma_fill_fn emit);

int ps5_agc_writer_acquire_mem(
    uint32_t **cursor, uint32_t capacity_dwords, const void *base,
    uint64_t bytes, uint8_t engine, uint32_t gcr_control,
    uint32_t poll_cycles, const void *gpu_mapping, size_t gpu_mapping_bytes,
    ps5_agc_emit_acquire_mem_fn emit);

int ps5_agc_writer_wait_rendering(
    uint32_t **cursor, uint32_t capacity_dwords, uint32_t driver_mode,
    int32_t video_handle, int32_t buffer_index,
    ps5_agc_wait_size_fn get_size, ps5_agc_wait_emit_fn emit);

#endif
