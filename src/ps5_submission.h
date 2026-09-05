#ifndef PS5_AGC_GEARS_SUBMISSION_H
#define PS5_AGC_GEARS_SUBMISSION_H

#include "ps5_present.h"

typedef int (*ps5_submit_commands_fn)(const uint32_t *commands,
                                      uint32_t command_dwords,
                                      void *opaque);

struct ps5_submission_input {
    struct ps5_present_stream *stream;
    ps5_present_set_flip_fn set_flip;
    ps5_submit_commands_fn submit;
    void *submit_opaque;
    volatile uint64_t *gpu_fence;
    int32_t videoout_handle;
    int32_t buffer_index;
    uint32_t flip_mode;
    uint64_t flip_arg;
    int surface_registered;
    int render_resources_ready;
    int videoout_event_armed;
};

struct ps5_submission_state {
    int transaction_started;
    int submit_called;
    int retain_all_resources;
    uint32_t command_dwords;
    int builder_result;
    int submit_result;
};

enum ps5_submission_result {
    PS5_SUBMISSION_OK = 0,
    PS5_SUBMISSION_PRECONDITION = -10,
    PS5_SUBMISSION_COMPOSE_FAILED = -11,
    PS5_SUBMISSION_SUBMIT_FAILED = -12,
};

int ps5_submission_build_and_submit(
    const struct ps5_submission_input *input,
    struct ps5_submission_state *state);

#endif
