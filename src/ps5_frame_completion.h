#ifndef PS5_AGC_GEARS_FRAME_COMPLETION_H
#define PS5_AGC_GEARS_FRAME_COMPLETION_H

#include <stdint.h>

struct ps5_frame_observation {
    uint64_t fence_value;
    int videoout_event_observed;
    uint64_t videoout_event_flip_arg;
    int fence_deadline_expired;
    int videoout_deadline_expired;
};

struct ps5_frame_completion {
    uint64_t expected_flip_arg;
    int transaction_submitted;
    int fence_zero_seen;
    int matching_videoout_event_seen;
    int retain_all_resources;
    int cleanup_allowed;
    int parked;
};

enum ps5_frame_completion_result {
    PS5_FRAME_WAITING = 0,
    PS5_FRAME_DONE = 1,
    PS5_FRAME_PRECONDITION = -20,
    PS5_FRAME_FENCE_TIMEOUT = -21,
    PS5_FRAME_VIDEOOUT_TIMEOUT = -22,
    PS5_FRAME_EVENT_BEFORE_FENCE = -23,
    PS5_FRAME_UNEXPECTED_FLIP_ARG = -24,
};

int ps5_frame_completion_begin(struct ps5_frame_completion *state,
                               uint64_t expected_flip_arg,
                               int transaction_submitted);
int ps5_frame_completion_observe(
    struct ps5_frame_completion *state,
    const struct ps5_frame_observation *observation);

#endif
