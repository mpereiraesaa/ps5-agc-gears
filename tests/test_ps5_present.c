#include "../src/ps5_present.h"

#include <assert.h>
#include <string.h>

enum mock_mode { MOCK_OK, MOCK_ERROR, MOCK_OVERFLOW, MOCK_WILD };
static enum mock_mode mode;

static int mock_set_flip(uint32_t **cursor, uint32_t capacity,
                         uint32_t driver_mode, int32_t handle, int32_t index,
                         uint32_t flip_mode, uint64_t flip_arg)
{
    assert(capacity == 64u && driver_mode == 0u);
    assert(handle == 7 && index == 0 && flip_mode == 1u && flip_arg == 9u);
    if (mode == MOCK_ERROR)
        return -99;
    if (mode == MOCK_OVERFLOW) {
        *cursor += 65;
        return 0;
    }
    if (mode == MOCK_WILD) {
        *cursor = (uint32_t *)(uintptr_t)1;
        return 0;
    }
    const uint32_t fake_flip[6] = {UINT32_C(0xc0041000), 7u, 0u, 1u, 9u, 0u};
    memcpy(*cursor, fake_flip, sizeof(fake_flip));
    *cursor += 6;
    return 0;
}

int main(void)
{
    uint32_t words[80];
    struct ps5_present_stream stream;

    stream = (struct ps5_present_stream){words, words + 71, 0, 0};
    assert(ps5_present_compose_flip_and_fence(
               &stream, mock_set_flip, 7, 0, 1u, 9u, UINT64_C(0x1000)) ==
           PS5_PRESENT_PRECONDITION);
    assert(!stream.transaction_started);

    stream = (struct ps5_present_stream){words, words + 80, 0, 0};
    mode = MOCK_ERROR;
    assert(ps5_present_compose_flip_and_fence(
               &stream, mock_set_flip, 7, 0, 1u, 9u, UINT64_C(0x1000)) ==
           PS5_PRESENT_BUILDER_ERROR);
    assert(stream.transaction_started);

    stream = (struct ps5_present_stream){words, words + 80, 0, 0};
    mode = MOCK_WILD;
    assert(ps5_present_compose_flip_and_fence(
               &stream, mock_set_flip, 7, 0, 1u, 9u, UINT64_C(0x1000)) ==
           PS5_PRESENT_CURSOR_INVALID);

    stream = (struct ps5_present_stream){words, words + 80, 0, 0};
    mode = MOCK_OVERFLOW;
    assert(ps5_present_compose_flip_and_fence(
               &stream, mock_set_flip, 7, 0, 1u, 9u, UINT64_C(0x1000)) ==
           PS5_PRESENT_CURSOR_INVALID);

    memset(words, 0, sizeof(words));
    stream = (struct ps5_present_stream){words, words + 80, 0, 0};
    mode = MOCK_OK;
    assert(ps5_present_compose_flip_and_fence(
               &stream, mock_set_flip, 7, 0, 1u, 9u,
               UINT64_C(0x1122334455667800)) == PS5_PRESENT_OK);
    assert(stream.cursor == words + 14);
    const uint32_t expected[8] = {
        UINT32_C(0xc0064900), UINT32_C(0x06000528), UINT32_C(0x42010000),
        UINT32_C(0x55667800), UINT32_C(0x11223344), 0u, 0u, 0u,
    };
    assert(memcmp(words + 6, expected, sizeof(expected)) == 0);
    return 0;
}
