#include "../src/ps5_submission.h"

#include <assert.h>
#include <string.h>

static int sequence;
static int builder_rc;
static int submit_rc;
static uint32_t expected_dwords = 14u;
static uint32_t expected_release_index = 6u;

static int mock_builder(uint32_t **cursor, uint32_t capacity, uint32_t mode,
                        int32_t handle, int32_t index, uint32_t flip_mode,
                        uint64_t flip_arg)
{
    assert(++sequence == 1);
    assert(capacity == 64u && mode == 0u && handle == 7 && index == 1);
    assert(flip_mode == 1u && flip_arg == 42u);
    if (builder_rc)
        return builder_rc;
    (*cursor)[0] = UINT32_C(0xc0041000);
    *cursor += 6;
    return 0;
}

static int mock_submit(const uint32_t *commands, uint32_t dwords, void *opaque)
{
    assert(++sequence == 2);
    assert(commands == opaque && dwords == expected_dwords);
    assert(commands[expected_release_index] == UINT32_C(0xc0064900));
    return submit_rc;
}

int main(void)
{
    uint32_t commands[80] = {0};
    volatile uint64_t fence = 99u;
    struct ps5_present_stream stream = {commands, commands + 80, 0, 0};
    struct ps5_submission_state state;
    struct ps5_submission_input input = {
        .stream = &stream,
        .set_flip = mock_builder,
        .submit = mock_submit,
        .submit_opaque = commands,
        .gpu_fence = &fence,
        .videoout_handle = 7,
        .buffer_index = 1,
        .flip_mode = 1u,
        .flip_arg = 42u,
        .surface_registered = 1,
        .render_resources_ready = 1,
        .videoout_event_armed = 1,
    };
    sequence = builder_rc = submit_rc = 0;
    assert(ps5_submission_build_and_submit(&input, &state) ==
           PS5_SUBMISSION_OK);
    assert(sequence == 2 && fence == 1u && state.command_dwords == 14u);
    assert(state.transaction_started && state.submit_called &&
           state.retain_all_resources);

    sequence = submit_rc = 0;
    builder_rc = -7;
    stream.cursor = 0;
    assert(ps5_submission_build_and_submit(&input, &state) ==
           PS5_SUBMISSION_COMPOSE_FAILED);
    assert(sequence == 1 && state.transaction_started &&
           !state.submit_called && state.retain_all_resources);

    sequence = builder_rc = 0;
    submit_rc = -8;
    stream.cursor = 0;
    assert(ps5_submission_build_and_submit(&input, &state) ==
           PS5_SUBMISSION_SUBMIT_FAILED);
    assert(sequence == 2 && state.submit_called && state.retain_all_resources);

    input.render_resources_ready = 0;
    stream.cursor = 0;
    sequence = builder_rc = submit_rc = 0;
    fence = 99u;
    assert(ps5_submission_build_and_submit(&input, &state) ==
           PS5_SUBMISSION_PRECONDITION);
    assert(sequence == 0 && fence == 99u && !state.transaction_started);

    input.render_resources_ready = 1;
    input.gpu_fence = (volatile uint64_t *)(void *)&commands[8];
    assert(ps5_submission_build_and_submit(&input, &state) ==
           PS5_SUBMISSION_PRECONDITION);

    /* A composed prefix must survive presentation append and be included in
     * the final submit length. */
    input.gpu_fence = &fence;
    stream.cursor = commands + 5;
    commands[0] = UINT32_C(0xfeed0001);
    sequence = builder_rc = submit_rc = 0;
    expected_dwords = 19u;
    expected_release_index = 11u;
    assert(ps5_submission_build_and_submit(&input, &state) ==
           PS5_SUBMISSION_OK);
    assert(commands[0] == UINT32_C(0xfeed0001));
    assert(state.command_dwords == 19u);
    return 0;
}
