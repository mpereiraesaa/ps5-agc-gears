#include "ps5_frame_completion.h"

#include <string.h>

int ps5_frame_completion_begin(struct ps5_frame_completion *state,
                               uint64_t expected_flip_arg,
                               int transaction_submitted)
{
    if (!state || !transaction_submitted)
        return PS5_FRAME_PRECONDITION;
    memset(state, 0, sizeof(*state));
    state->expected_flip_arg = expected_flip_arg;
    state->transaction_submitted = 1;
    state->retain_all_resources = 1;
    return PS5_FRAME_WAITING;
}

int ps5_frame_completion_observe(
    struct ps5_frame_completion *state,
    const struct ps5_frame_observation *observation)
{
    if (!state || !observation || !state->transaction_submitted ||
        state->cleanup_allowed || state->parked)
        return PS5_FRAME_PRECONDITION;

    if (observation->videoout_event_observed &&
        observation->fence_value != 0u) {
        state->parked = 1;
        return PS5_FRAME_EVENT_BEFORE_FENCE;
    }
    if (observation->videoout_event_observed &&
        observation->videoout_event_flip_arg != state->expected_flip_arg) {
        state->parked = 1;
        return PS5_FRAME_UNEXPECTED_FLIP_ARG;
    }
    if (observation->fence_value == 0u)
        state->fence_zero_seen = 1;
    if (observation->videoout_event_observed)
        state->matching_videoout_event_seen = 1;

    if (state->fence_zero_seen && state->matching_videoout_event_seen) {
        state->cleanup_allowed = 1;
        state->retain_all_resources = 0;
        return PS5_FRAME_DONE;
    }
    if (!state->fence_zero_seen && observation->fence_deadline_expired) {
        state->parked = 1;
        return PS5_FRAME_FENCE_TIMEOUT;
    }
    if (state->fence_zero_seen && !state->matching_videoout_event_seen &&
        observation->videoout_deadline_expired) {
        state->parked = 1;
        return PS5_FRAME_VIDEOOUT_TIMEOUT;
    }
    return PS5_FRAME_WAITING;
}
