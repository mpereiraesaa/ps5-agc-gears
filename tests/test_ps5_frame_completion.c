#include "../src/ps5_frame_completion.h"

#include <assert.h>

int main(void)
{
    struct ps5_frame_completion state;
    struct ps5_frame_observation observation = {1u, 0, 0u, 0, 0};
    assert(ps5_frame_completion_begin(&state, 42u, 1) == PS5_FRAME_WAITING);
    assert(state.retain_all_resources && !state.cleanup_allowed);
    assert(ps5_frame_completion_observe(&state, &observation) ==
           PS5_FRAME_WAITING);

    observation.fence_value = 0u;
    assert(ps5_frame_completion_observe(&state, &observation) ==
           PS5_FRAME_WAITING);
    observation.videoout_event_observed = 1;
    observation.videoout_event_flip_arg = 42u;
    assert(ps5_frame_completion_observe(&state, &observation) ==
           PS5_FRAME_DONE);
    assert(state.cleanup_allowed && !state.retain_all_resources);

    assert(ps5_frame_completion_begin(&state, 7u, 1) == PS5_FRAME_WAITING);
    observation = (struct ps5_frame_observation){1u, 0, 0u, 1, 0};
    assert(ps5_frame_completion_observe(&state, &observation) ==
           PS5_FRAME_FENCE_TIMEOUT);
    assert(state.parked && state.retain_all_resources);

    assert(ps5_frame_completion_begin(&state, 12u, 1) == PS5_FRAME_WAITING);
    observation = (struct ps5_frame_observation){0u, 1, 13u, 0, 0};
    assert(ps5_frame_completion_observe(&state, &observation) ==
           PS5_FRAME_UNEXPECTED_FLIP_ARG);

    assert(ps5_frame_completion_begin(&state, 11u, 1) == PS5_FRAME_WAITING);
    observation = (struct ps5_frame_observation){1u, 1, 11u, 0, 0};
    assert(ps5_frame_completion_observe(&state, &observation) ==
           PS5_FRAME_EVENT_BEFORE_FENCE);

    assert(ps5_frame_completion_begin(&state, 8u, 1) == PS5_FRAME_WAITING);
    observation = (struct ps5_frame_observation){0u, 0, 0u, 0, 1};
    assert(ps5_frame_completion_observe(&state, &observation) ==
           PS5_FRAME_VIDEOOUT_TIMEOUT);
    assert(state.parked && state.retain_all_resources);

    assert(ps5_frame_completion_begin(&state, 9u, 0) ==
           PS5_FRAME_PRECONDITION);
    return 0;
}
