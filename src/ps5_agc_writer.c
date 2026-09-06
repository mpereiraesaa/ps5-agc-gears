#include "ps5_agc_writer.h"

#include "ps5_gpu_span.h"

#include <limits.h>

static uint8_t out_of_space(struct ps5_agc_command_buffer *writer,
                            uint32_t requested, void *user_data)
{
    (void)writer;
    (void)requested;
    (void)user_data;
    return 0;
}

static int begin_writer(struct ps5_agc_command_buffer *writer,
                        uint32_t **cursor, uint32_t capacity)
{
    if (!writer || !cursor || !*cursor || capacity == 0u ||
        capacity > (uint32_t)(SIZE_MAX / sizeof(uint32_t)))
        return PS5_AGC_WRITER_PRECONDITION;
    writer->bottom = *cursor;
    writer->top = *cursor + capacity;
    writer->up = *cursor;
    writer->down = writer->top;
    writer->callback = (uintptr_t)out_of_space;
    writer->user_data = 0;
    writer->reserved_dwords = 0u;
    writer->padding = 0u;
    return PS5_AGC_WRITER_OK;
}

static int finish_writer(uint32_t **cursor,
                         const struct ps5_agc_command_buffer *writer,
                         uint32_t minimum, uint32_t maximum)
{
    const uint32_t *const begin = *cursor;
    if (!writer || writer->bottom != begin || writer->top < begin ||
        writer->up < begin || writer->up > writer->top ||
        writer->down != writer->top)
        return PS5_AGC_WRITER_CURSOR_INVALID;
    const size_t emitted = (size_t)(writer->up - begin);
    if (emitted < minimum || emitted > maximum)
        return PS5_AGC_WRITER_PACKET_SIZE;
    *cursor = writer->up;
    return PS5_AGC_WRITER_OK;
}

int ps5_agc_writer_set_flip(
    uint32_t **cursor, uint32_t capacity, uint32_t driver_mode,
    int32_t video_handle, int32_t buffer_index, uint32_t flip_mode,
    uint64_t flip_arg, ps5_agc_emit_flip_fn emit)
{
    struct ps5_agc_command_buffer writer;
    if (!emit || driver_mode != 0u || video_handle < 0 || buffer_index < 0 ||
        begin_writer(&writer, cursor, capacity) != PS5_AGC_WRITER_OK)
        return PS5_AGC_WRITER_PRECONDITION;
    (void)emit(&writer, (uint32_t)video_handle, buffer_index, flip_mode,
               (int64_t)flip_arg);
    return finish_writer(cursor, &writer, 1u, capacity);
}

int ps5_agc_writer_draw_auto(
    uint32_t **cursor, uint32_t capacity, uint32_t vertex_count,
    uint64_t modifier, ps5_agc_emit_draw_auto_fn emit)
{
    struct ps5_agc_command_buffer writer;
    if (!emit || vertex_count == 0u || capacity < 3u ||
        begin_writer(&writer, cursor, capacity) != PS5_AGC_WRITER_OK)
        return PS5_AGC_WRITER_PRECONDITION;
    (void)emit(&writer, vertex_count, modifier);
    return finish_writer(cursor, &writer, 3u, 3u);
}

int ps5_agc_writer_draw_index(
    uint32_t **cursor, uint32_t capacity, uint32_t index_count,
    const uint16_t *gpu_indices, const void *gpu_mapping,
    size_t gpu_mapping_bytes, uint64_t modifier,
    ps5_agc_emit_draw_index_fn emit)
{
    struct ps5_agc_command_buffer writer;
    if (!emit || !gpu_indices || index_count == 0u ||
        index_count % 3u != 0u ||
        ((uintptr_t)gpu_indices & (sizeof(uint16_t) - 1u)) != 0u ||
        capacity < 6u ||
        begin_writer(&writer, cursor, capacity) != PS5_AGC_WRITER_OK)
        return PS5_AGC_WRITER_PRECONDITION;
    if (!ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes, gpu_indices,
                              (size_t)index_count * sizeof(uint16_t)))
        return PS5_AGC_WRITER_NOT_GPU_VISIBLE;
    (void)emit(&writer, index_count, gpu_indices, modifier);
    return finish_writer(cursor, &writer, 6u, 6u);
}

int ps5_agc_writer_set_sh_direct(
    uint32_t **cursor, uint32_t capacity, uint32_t compact_offset,
    const uint32_t *values, uint32_t count, ps5_agc_emit_sh_direct_fn emit)
{
    struct ps5_agc_command_buffer writer;
    if (!emit || !values || count == 0u || count > UINT32_MAX - 2u ||
        capacity < count + 2u ||
        begin_writer(&writer, cursor, capacity) != PS5_AGC_WRITER_OK)
        return PS5_AGC_WRITER_PRECONDITION;
    (void)emit(&writer, compact_offset, values, count);
    return finish_writer(cursor, &writer, count + 2u, count + 2u);
}

int ps5_agc_writer_set_indirect(
    uint32_t **cursor, uint32_t capacity, const void *registers,
    uint32_t count, const void *gpu_mapping, size_t gpu_mapping_bytes,
    ps5_agc_emit_indirect_fn emit)
{
    struct ps5_agc_command_buffer writer;
    if (!emit || !registers || count == 0u ||
        begin_writer(&writer, cursor, capacity) != PS5_AGC_WRITER_OK)
        return PS5_AGC_WRITER_PRECONDITION;
    if (!ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes, registers,
                              (size_t)count * 8u))
        return PS5_AGC_WRITER_NOT_GPU_VISIBLE;
    (void)emit(&writer, registers, count);
    return finish_writer(cursor, &writer, 1u, capacity);
}

int ps5_agc_writer_fill_depth(
    uint32_t **cursor, uint32_t capacity, void *destination,
    uint32_t repeated_word, uint32_t byte_count,
    const void *gpu_mapping, size_t gpu_mapping_bytes,
    ps5_agc_emit_dma_fill_fn emit)
{
    struct ps5_agc_command_buffer writer;
    if (!emit || !destination || byte_count == 0u || capacity < 16u ||
        begin_writer(&writer, cursor, capacity) != PS5_AGC_WRITER_OK)
        return PS5_AGC_WRITER_PRECONDITION;
    if (!ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes, destination,
                              byte_count))
        return PS5_AGC_WRITER_NOT_GPU_VISIBLE;
    (void)emit(&writer, destination, repeated_word, byte_count);
    return finish_writer(cursor, &writer, 1u, capacity);
}

int ps5_agc_writer_acquire_mem(
    uint32_t **cursor, uint32_t capacity, const void *base, uint64_t bytes,
    uint8_t engine, uint32_t gcr_control, uint32_t poll_cycles,
    const void *gpu_mapping, size_t gpu_mapping_bytes,
    ps5_agc_emit_acquire_mem_fn emit)
{
    struct ps5_agc_command_buffer writer;
    if (!emit || !base || bytes == 0u || bytes > SIZE_MAX ||
        ((uintptr_t)base & 255u) != 0u || (bytes & 255u) != 0u ||
        engine != 1u || gcr_control == 0u || poll_cycles == 0u ||
        capacity < 8u ||
        begin_writer(&writer, cursor, capacity) != PS5_AGC_WRITER_OK)
        return PS5_AGC_WRITER_PRECONDITION;
    if (!ps5_gpu_span_visible(gpu_mapping, gpu_mapping_bytes, base,
                              (size_t)bytes))
        return PS5_AGC_WRITER_NOT_GPU_VISIBLE;
    (void)emit(&writer, engine, 0u, gcr_control, base, bytes, poll_cycles);
    return finish_writer(cursor, &writer, 8u, 8u);
}

int ps5_agc_writer_wait_rendering(
    uint32_t **cursor, uint32_t capacity, uint32_t driver_mode,
    int32_t video_handle, int32_t buffer_index,
    ps5_agc_wait_size_fn get_size, ps5_agc_wait_emit_fn emit)
{
    if (!cursor || !*cursor || !get_size || !emit || driver_mode != 0u ||
        video_handle < 0 || buffer_index < 0)
        return PS5_AGC_WRITER_PRECONDITION;
    const uint32_t required = get_size();
    if (required == 0u || required > capacity)
        return PS5_AGC_WRITER_PACKET_SIZE;
    uint32_t *const begin = *cursor;
    uint32_t *next = begin;
    if (emit(&next, required, 0u, (uint32_t)video_handle, buffer_index) != 0u)
        return PS5_AGC_WRITER_EMIT_FAILED;
    if (next != begin + required)
        return PS5_AGC_WRITER_CURSOR_INVALID;
    *cursor = next;
    return PS5_AGC_WRITER_OK;
}
