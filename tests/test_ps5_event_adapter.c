#include "../src/ps5_event_adapter.h"

#include <assert.h>
#include <string.h>

static int wait_rc;
static int event_count;
static int decode_rc;
static int64_t decoded_token;

static int mock_wait(void *queue, void *event, int count, int *out,
                     unsigned int *timeout_us)
{
    assert(queue && event && count == 1 && timeout_us);
    *out = event_count;
    return wait_rc;
}

static int mock_decode(const void *event, int64_t *flip_arg)
{
    assert(event && flip_arg);
    *flip_arg = decoded_token;
    return decode_rc;
}

int main(void)
{
    uint8_t event[64];
    volatile uint64_t fence = 1u;
    struct ps5_frame_completion state;
    struct ps5_event_diagnostics diagnostics;
    struct ps5_event_poll poll = {
        .equeue = event,
        .event_storage = event,
        .gpu_fence = &fence,
        .wait_timeout_us = 1000u,
        .wait_equeue = mock_wait,
        .decode_flip_event = mock_decode,
        .diagnostics = &diagnostics,
    };
    memset(&diagnostics, 0, sizeof(diagnostics));
    assert(ps5_frame_completion_begin(&state, 42u, 1) == PS5_FRAME_WAITING);
    wait_rc = -1;
    event_count = 0;
    assert(ps5_event_poll_completion(&state, &poll) == PS5_FRAME_WAITING);
    assert(diagnostics.wait_calls == 1u && diagnostics.decode_calls == 0u);

    fence = 0u;
    wait_rc = 0;
    event_count = 1;
    decoded_token = 42;
    assert(ps5_event_poll_completion(&state, &poll) == PS5_FRAME_DONE);
    assert(diagnostics.decode_calls == 1u && diagnostics.last_flip_arg == 42);

    assert(ps5_frame_completion_begin(&state, 7u, 1) == PS5_FRAME_WAITING);
    fence = 0u;
    decode_rc = -1;
    assert(ps5_event_poll_completion(&state, &poll) == PS5_EVENT_INVALID);
    assert(state.parked && state.retain_all_resources);

    assert(ps5_frame_completion_begin(&state, 7u, 1) == PS5_FRAME_WAITING);
    decode_rc = 0;
    wait_rc = -1;
    event_count = 1;
    assert(ps5_event_poll_completion(&state, &poll) == PS5_EVENT_INVALID);
    assert(state.parked && state.retain_all_resources);
    return 0;
}
