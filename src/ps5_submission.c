#include "ps5_submission.h"

#include <string.h>

int ps5_submission_build_and_submit(
    const struct ps5_submission_input *input,
    struct ps5_submission_state *state)
{
    if (!state)
        return PS5_SUBMISSION_PRECONDITION;
    memset(state, 0, sizeof(*state));
    const uintptr_t stream_start = input && input->stream
        ? (uintptr_t)input->stream->start : 0u;
    const uintptr_t stream_end = input && input->stream
        ? (uintptr_t)input->stream->end : 0u;
    const uintptr_t fence_start = input ? (uintptr_t)input->gpu_fence : 0u;
    const uintptr_t fence_end = fence_start + sizeof(uint64_t);
    if (!input || !input->stream || !input->set_flip || !input->submit ||
        !input->gpu_fence || input->videoout_handle < 0 ||
        input->buffer_index < 0 || input->buffer_index >= 2 ||
        !input->surface_registered || !input->render_resources_ready ||
        !input->videoout_event_armed || (fence_start & 7u) != 0u ||
        stream_start >= stream_end || fence_end < fence_start ||
        (fence_start < stream_end && fence_end > stream_start))
        return PS5_SUBMISSION_PRECONDITION;

    *input->gpu_fence = UINT64_C(1);
    input->stream->transaction_started = 0;
    state->builder_result = ps5_present_compose_flip_and_fence(
        input->stream, input->set_flip, input->videoout_handle,
        input->buffer_index, input->flip_mode, input->flip_arg,
        (uintptr_t)input->gpu_fence);
    state->transaction_started = input->stream->transaction_started;
    if (state->transaction_started)
        state->retain_all_resources = 1;
    if (state->builder_result != PS5_PRESENT_OK)
        return PS5_SUBMISSION_COMPOSE_FAILED;

    state->command_dwords =
        (uint32_t)(input->stream->cursor - input->stream->start);
    state->submit_called = 1;
    state->retain_all_resources = 1;
    state->submit_result = input->submit(
        input->stream->start, state->command_dwords, input->submit_opaque);
    if (state->submit_result != 0)
        return PS5_SUBMISSION_SUBMIT_FAILED;
    return PS5_SUBMISSION_OK;
}
