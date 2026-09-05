#include "ps5_event_adapter.h"

int ps5_event_poll_completion(struct ps5_frame_completion *state,
                              const struct ps5_event_poll *poll)
{
    if (!state || !poll || !poll->equeue || !poll->event_storage ||
        !poll->gpu_fence || !poll->wait_equeue || !poll->decode_flip_event ||
        !state->transaction_submitted || state->cleanup_allowed || state->parked)
        return PS5_EVENT_PRECONDITION;

    struct ps5_frame_observation observation = {
        .fence_value = *poll->gpu_fence,
        .fence_deadline_expired = poll->fence_deadline_expired,
        .videoout_deadline_expired = poll->videoout_deadline_expired,
    };
    unsigned int timeout = poll->wait_timeout_us;
    int count = 0;
    const int wait_rc = poll->wait_equeue(
        poll->equeue, poll->event_storage, 1, &count, &timeout);
    if (poll->diagnostics) {
        ++poll->diagnostics->wait_calls;
        poll->diagnostics->last_wait_rc = wait_rc;
        poll->diagnostics->last_event_count = count;
    }
    if (count < 0 || count > 1 || (wait_rc != 0 && count != 0)) {
        state->parked = 1;
        return PS5_EVENT_INVALID;
    }
    if (wait_rc == 0 && count == 1) {
        int64_t flip_arg = 0;
        const int decode_rc =
            poll->decode_flip_event(poll->event_storage, &flip_arg);
        if (poll->diagnostics) {
            ++poll->diagnostics->decode_calls;
            poll->diagnostics->last_decode_rc = decode_rc;
            poll->diagnostics->last_flip_arg = flip_arg;
        }
        if (decode_rc != 0) {
            state->parked = 1;
            return PS5_EVENT_INVALID;
        }
        observation.videoout_event_observed = 1;
        observation.videoout_event_flip_arg = (uint64_t)flip_arg;
    }
    return ps5_frame_completion_observe(state, &observation);
}
