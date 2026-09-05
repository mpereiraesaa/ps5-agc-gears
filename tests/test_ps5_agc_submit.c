#include "../src/ps5_agc_submit.h"
#include "../include/ps5_platform.h"

#include <assert.h>

static const void *flushed_address;
static size_t flushed_bytes;
static int submit_result;
static unsigned submits;

static void mock_flush(const void *address, size_t bytes)
{
    flushed_address = address;
    flushed_bytes = bytes;
}

static int32_t mock_submit(void *opaque)
{
    const struct ps5_agc_submit *description = opaque;
    ++submits;
    assert(description->words == flushed_address);
    assert(description->count == flushed_bytes / sizeof(uint32_t));
    assert(description->flag == 0u && description->padding[0] == 0u &&
           description->padding[1] == 0u && description->padding[2] == 0u);
    return submit_result;
}

int main(void)
{
    uint32_t mapping[64] = {0};
    struct ps5_agc_submit_context context = {
        mapping, sizeof(mapping), mock_flush, mock_submit
    };
    flushed_address = 0;
    flushed_bytes = 0u;
    submit_result = -77;
    submits = 0u;
    assert(ps5_agc_submit_checked(mapping + 4, 12u, &context) == -77);
    assert(flushed_address == mapping + 4 && flushed_bytes == 48u &&
           submits == 1u);

    submits = 0u;
    assert(ps5_agc_submit_checked(mapping + 60, 8u, &context) ==
           PS5_AGC_SUBMIT_NOT_GPU_VISIBLE);
    assert(submits == 0u);
    assert(ps5_agc_submit_checked(mapping, 0u, &context) ==
           PS5_AGC_SUBMIT_PRECONDITION);
    context.flush = 0;
    assert(ps5_agc_submit_checked(mapping, 1u, &context) ==
           PS5_AGC_SUBMIT_PRECONDITION);
    return 0;
}
