#include "ps5_present.h"

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(uintptr_t) == 8, "PS5 GPU addresses require 64 bits");

int ps5_present_compose_flip_and_fence(
    struct ps5_present_stream *stream,
    ps5_present_set_flip_fn set_flip,
    int32_t videoout_handle,
    int32_t buffer_index,
    uint32_t flip_mode,
    uint64_t flip_arg,
    uintptr_t fence_address)
{
    const uintptr_t start = stream ? (uintptr_t)stream->start : 0u;
    const uintptr_t end = stream ? (uintptr_t)stream->end : 0u;
    const uintptr_t initial = stream && stream->cursor
        ? (uintptr_t)stream->cursor : start;
    const size_t required =
        (PS5_PRESENT_SET_FLIP_MAX_DWORDS + PS5_PRESENT_RELEASE_DWORDS) *
        sizeof(uint32_t);

    if (!stream || !set_flip || !stream->start || !stream->end ||
        start > initial || initial > end || end - initial < required ||
        ((end - start) % sizeof(uint32_t)) != 0u ||
        (fence_address & 7u) != 0u)
        return PS5_PRESENT_PRECONDITION;

    uint32_t *cursor = stream->cursor ? stream->cursor : stream->start;
    /* Once SetFlip starts, VideoOut may own state even if validation fails. */
    stream->transaction_started = 1;
    if (set_flip(&cursor, PS5_PRESENT_SET_FLIP_MAX_DWORDS, 0u,
                 videoout_handle, buffer_index, flip_mode, flip_arg) != 0)
        return PS5_PRESENT_BUILDER_ERROR;

    const uintptr_t cursor_address = (uintptr_t)cursor;
    const uintptr_t builder_limit =
        initial + PS5_PRESENT_SET_FLIP_MAX_DWORDS * sizeof(uint32_t);
    if (builder_limit < initial || cursor_address < initial ||
        cursor_address > builder_limit || cursor_address > end ||
        ((cursor_address - initial) % sizeof(uint32_t)) != 0u ||
        end - cursor_address < PS5_PRESENT_RELEASE_DWORDS * sizeof(uint32_t))
        return PS5_PRESENT_CURSOR_INVALID;

    const uint32_t release[PS5_PRESENT_RELEASE_DWORDS] = {
        UINT32_C(0xc0064900), UINT32_C(0x06000528), UINT32_C(0x42010000),
        (uint32_t)fence_address, (uint32_t)(fence_address >> 32), 0u, 0u, 0u,
    };
    memcpy(cursor, release, sizeof(release));
    stream->cursor = cursor + PS5_PRESENT_RELEASE_DWORDS;
    return PS5_PRESENT_OK;
}
