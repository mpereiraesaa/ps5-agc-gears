#include "../src/ps5_agc_writer.h"

#include <assert.h>
#include <string.h>

static unsigned behavior;
static uint32_t wait_size;

static void advance(void *opaque, uint32_t words)
{
    struct ps5_agc_command_buffer *writer = opaque;
    if (behavior == 1u)
        writer->up = writer->top + 1;
    else if (behavior == 2u)
        writer->up += words - 1u;
    else {
        memset(writer->up, 0xa5, (size_t)words * sizeof(uint32_t));
        writer->up += words;
    }
}

static uint32_t *emit_flip(void *writer, uint32_t handle, int32_t index,
                           uint32_t mode, int64_t arg)
{
    assert(handle == 7u && index == 1 && mode == 2u && arg == 9);
    advance(writer, 6u);
    return ((struct ps5_agc_command_buffer *)writer)->up;
}

static uint32_t *emit_draw(void *writer, uint32_t vertices, uint64_t modifier)
{
    assert(vertices == 36u && modifier == UINT64_C(0x1234));
    advance(writer, 3u);
    return ((struct ps5_agc_command_buffer *)writer)->up;
}

static uint32_t *emit_indexed(void *writer, uint32_t count,
                              const void *indices, uint64_t modifier)
{
    assert(count == 6u && indices && modifier == UINT64_C(0x1234));
    advance(writer, 6u);
    return ((struct ps5_agc_command_buffer *)writer)->up;
}

static uint32_t *emit_direct(void *writer, uint32_t offset,
                             const uint32_t *values, uint32_t count)
{
    assert(offset == 0x8cu && values[0] == 11u && count == 24u);
    advance(writer, count + 2u);
    return ((struct ps5_agc_command_buffer *)writer)->up;
}

static uint32_t *emit_indirect(void *writer, const void *registers,
                               uint32_t count)
{
    assert(registers && count == 2u);
    advance(writer, 4u);
    return ((struct ps5_agc_command_buffer *)writer)->up;
}

static uint32_t *emit_fill(void *writer, void *destination, uint32_t word,
                           uint32_t bytes)
{
    assert(destination && word == 0x3f800000u && bytes == 64u);
    advance(writer, 8u);
    return ((struct ps5_agc_command_buffer *)writer)->up;
}

static uint32_t get_wait_size(void) { return wait_size; }

static uint32_t emit_wait(uint32_t **cursor, uint32_t words,
                          uint32_t reserved, uint32_t handle, int32_t index)
{
    assert(reserved == 0u && handle == 7u && index == 1);
    if (behavior == 3u)
        return 5u;
    *cursor += behavior == 2u ? words - 1u : words;
    return 0u;
}

int main(void)
{
    union {
        uint64_t align;
        uint32_t words[128];
    } gpu = {0};
    uint32_t *cursor = gpu.words;

    behavior = 0u;
    assert(ps5_agc_writer_set_flip(&cursor, 64u, 0u, 7, 1, 2u, 9u,
                                   emit_flip) == PS5_AGC_WRITER_OK);
    assert(cursor == gpu.words + 6);
    assert(ps5_agc_writer_draw_auto(&cursor, 64u, 36u, UINT64_C(0x1234),
                                    emit_draw) == PS5_AGC_WRITER_OK);
    assert(cursor == gpu.words + 9);
    assert(ps5_agc_writer_draw_index(
               &cursor, 64u, 6u, gpu.words + 112u, gpu.words,
               sizeof(gpu.words), UINT64_C(0x1234), emit_indexed) ==
           PS5_AGC_WRITER_OK);
    assert(cursor == gpu.words + 15);

    uint32_t values[24] = {11u};
    assert(ps5_agc_writer_set_sh_direct(&cursor, 64u, 0x8cu, values, 24u,
                                        emit_direct) == PS5_AGC_WRITER_OK);
    assert(cursor == gpu.words + 41);

    assert(ps5_agc_writer_set_indirect(&cursor, 64u, gpu.words + 96, 2u,
                                       gpu.words, sizeof(gpu.words),
                                       emit_indirect) == PS5_AGC_WRITER_OK);
    assert(cursor == gpu.words + 45);
    assert(ps5_agc_writer_set_indirect(&cursor, 64u, values, 2u,
                                       gpu.words, sizeof(gpu.words),
                                       emit_indirect) ==
           PS5_AGC_WRITER_NOT_GPU_VISIBLE);
    assert(cursor == gpu.words + 45);
    assert(ps5_agc_writer_draw_index(
               &cursor, 64u, 6u, values, gpu.words, sizeof(gpu.words),
               UINT64_C(0x1234), emit_indexed) ==
           PS5_AGC_WRITER_NOT_GPU_VISIBLE);
    assert(cursor == gpu.words + 45);

    assert(ps5_agc_writer_fill_depth(&cursor, 64u, gpu.words + 64,
                                     0x3f800000u, 64u, gpu.words,
                                     sizeof(gpu.words), emit_fill) ==
           PS5_AGC_WRITER_OK);
    assert(cursor == gpu.words + 53);

    wait_size = 5u;
    assert(ps5_agc_writer_wait_rendering(&cursor, 16u, 0u, 7, 1,
                                         get_wait_size, emit_wait) ==
           PS5_AGC_WRITER_OK);
    assert(cursor == gpu.words + 58);

    uint32_t *const stable = cursor;
    behavior = 1u;
    assert(ps5_agc_writer_draw_auto(&cursor, 3u, 36u, UINT64_C(0x1234),
                                    emit_draw) ==
           PS5_AGC_WRITER_CURSOR_INVALID);
    assert(cursor == stable);
    behavior = 2u;
    assert(ps5_agc_writer_draw_auto(&cursor, 3u, 36u, UINT64_C(0x1234),
                                    emit_draw) ==
           PS5_AGC_WRITER_PACKET_SIZE);
    assert(cursor == stable);
    assert(ps5_agc_writer_wait_rendering(&cursor, 16u, 0u, 7, 1,
                                         get_wait_size, emit_wait) ==
           PS5_AGC_WRITER_CURSOR_INVALID);
    assert(cursor == stable);
    behavior = 3u;
    assert(ps5_agc_writer_wait_rendering(&cursor, 16u, 0u, 7, 1,
                                         get_wait_size, emit_wait) ==
           PS5_AGC_WRITER_EMIT_FAILED);
    assert(cursor == stable);

    behavior = 0u;
    wait_size = 17u;
    assert(ps5_agc_writer_wait_rendering(&cursor, 16u, 0u, 7, 1,
                                         get_wait_size, emit_wait) ==
           PS5_AGC_WRITER_PACKET_SIZE);
    assert(ps5_agc_writer_draw_auto(&cursor, 2u, 36u, 0u, emit_draw) ==
           PS5_AGC_WRITER_PRECONDITION);
    assert(ps5_agc_writer_set_flip(&cursor, 64u, 1u, 7, 1, 2u, 9u,
                                   emit_flip) == PS5_AGC_WRITER_PRECONDITION);
    return 0;
}
