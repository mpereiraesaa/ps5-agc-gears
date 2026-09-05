#include "gears_frame_runner.h"

#include <string.h>

static void snapshot(GearsFrameLoop *loop, uint64_t frame, int error, int force)
{
    (void)gears_telemetry_snapshot(&loop->stats, &loop->result.telemetry);
    if (loop->input.telemetry &&
        (force || gears_telemetry_should_sample(frame, error)))
        loop->input.telemetry(frame, &loop->result.telemetry, error,
                              loop->input.user);
}

static int retain_failure(GearsFrameLoop *loop,
                          const GearsFrameLoopPending *pending,
                          uint64_t failed_frame, int callback_result)
{
    loop->result.state = GEARS_RUN_POST_SUBMIT_RETAIN;
    loop->result.failed_frame = failed_frame;
    loop->result.callback_result = callback_result;
    loop->result.retained_token = pending->frame.token;
    gears_telemetry_record_error(&loop->stats);
    snapshot(loop, failed_frame, 1, 0);
    return 2;
}

static int retire(GearsFrameLoop *loop, GearsFrameLoopPending *pending)
{
    const uint64_t gpu_start = loop->input.now_ns(loop->input.user);
    int rc = loop->input.wait_gpu(&pending->frame, loop->input.user);
    const uint64_t gpu_end = loop->input.now_ns(loop->input.user);
    if (rc != 0 || gears_animation_mark_gpu_complete(
                       &loop->animation, &pending->frame) != 0)
        return retain_failure(loop, pending, pending->frame.frame_index,
                              rc ? rc : -2);

    const uint64_t video_start = loop->input.now_ns(loop->input.user);
    uint64_t observed_token = 0;
    rc = loop->input.wait_videoout(&pending->frame, &observed_token,
                                   loop->input.user);
    const uint64_t video_end = loop->input.now_ns(loop->input.user);
    if (rc != 0 || gears_animation_mark_videoout_complete(
                       &loop->animation, pending->frame.buffer,
                       observed_token) != 0)
        return retain_failure(loop, pending, pending->frame.frame_index,
                              rc ? rc : -3);

    const uint64_t frame_end = loop->input.now_ns(loop->input.user);
    const uint64_t present_interval = loop->last_retire_ns
        ? frame_end - loop->last_retire_ns : 0u;
    const int over_budget = present_interval &&
        loop->input.present_interval_budget_ns &&
        present_interval > loop->input.present_interval_budget_ns;

    /* GPU and VideoOut ownership are complete. Release the runner slot before
     * fallible accounting so both state machines remain consistent. */
    pending->active = 0;
    --loop->active_frames;
    loop->last_retire_ns = frame_end;
    if (gears_telemetry_record_frame(&loop->stats, pending->compose_ns,
                                     gpu_end - gpu_start,
                                     video_end - video_start,
                                     present_interval, over_budget) != 0) {
        loop->result.state = GEARS_RUN_TELEMETRY_FAILURE;
        loop->result.failed_frame = pending->frame.frame_index;
        loop->result.callback_result = -4;
        gears_telemetry_record_error(&loop->stats);
        snapshot(loop, pending->frame.frame_index, 1, 0);
        return 1;
    }
    ++loop->result.frames_completed;
    snapshot(loop, pending->frame.frame_index, 0, 0);
    return 0;
}

int gears_frame_loop_init(GearsFrameLoop *loop,
                          const GearsFrameRunnerInput *input)
{
    if (!loop || !input || !input->start_ns || !input->now_ns ||
        !input->compose || !input->submit || !input->wait_gpu ||
        !input->wait_videoout)
        return -1;
    memset(loop, 0, sizeof(*loop));
    loop->input = *input;
    gears_telemetry_reset(&loop->stats);
    if (gears_animation_init(&loop->animation, input->start_ns,
                             input->first_flip_token) != 0)
        return -1;
    loop->loop_start_ns = input->now_ns(input->user);
    loop->result.state = GEARS_RUN_ACTIVE;
    loop->initialized = 1;
    return 0;
}

int gears_frame_loop_step(GearsFrameLoop *loop)
{
    if (!loop || !loop->initialized || loop->result.state != GEARS_RUN_ACTIVE)
        return -1;
    const uint64_t index = loop->animation.next_frame_index;
    const uint32_t slot = (uint32_t)(index & 1u);
    GearsFrameLoopPending *next = &loop->pending[slot];
    if (next->active) {
        const int retired = retire(loop, next);
        if (retired != 0)
            return retired;
    }

    next->frame_start = loop->input.now_ns(loop->input.user);
    int rc = gears_animation_prepare(
        &loop->animation, next->frame_start, loop->input.srd_tables,
        loop->input.vertex_counts, &next->frame);
    if (rc != 0) {
        loop->result.failed_frame = index;
        loop->result.callback_result = rc;
        for (unsigned i = 0; i < GEARS_FRAME_BUFFER_COUNT; ++i)
            if (loop->pending[i].active)
                return retain_failure(loop, &loop->pending[i], index, rc);
        loop->result.state = GEARS_RUN_PRE_SUBMIT_FAILURE;
        gears_telemetry_record_error(&loop->stats);
        snapshot(loop, index, 1, 0);
        return 1;
    }

    const uint64_t compose_start = loop->input.now_ns(loop->input.user);
    rc = loop->input.compose(&next->frame, loop->input.user);
    next->compose_ns = loop->input.now_ns(loop->input.user) - compose_start;
    if (rc != 0) {
        (void)gears_animation_cancel(&loop->animation, &next->frame);
        for (unsigned i = 0; i < GEARS_FRAME_BUFFER_COUNT; ++i)
            if (loop->pending[i].active)
                return retain_failure(loop, &loop->pending[i], index, rc);
        loop->result.state = GEARS_RUN_PRE_SUBMIT_FAILURE;
        loop->result.failed_frame = index;
        loop->result.callback_result = rc;
        gears_telemetry_record_error(&loop->stats);
        snapshot(loop, index, 1, 0);
        return 1;
    }

    if (gears_animation_mark_submitted(&loop->animation, &next->frame) != 0)
        return -1;
    next->active = 1; /* Ambiguous before entering submit callback. */
    ++loop->active_frames;
    if (loop->active_frames > loop->result.max_frames_in_flight)
        loop->result.max_frames_in_flight = loop->active_frames;
    rc = loop->input.submit(&next->frame, loop->input.user);
    if (rc != 0)
        return retain_failure(loop, next, index, rc);
    return 0;
}

int gears_frame_loop_drain(GearsFrameLoop *loop)
{
    if (!loop || !loop->initialized || loop->result.state != GEARS_RUN_ACTIVE)
        return -1;
    while (loop->active_frames != 0u) {
        GearsFrameLoopPending *oldest = 0;
        for (unsigned i = 0; i < GEARS_FRAME_BUFFER_COUNT; ++i) {
            GearsFrameLoopPending *candidate = &loop->pending[i];
            if (candidate->active && (!oldest ||
                candidate->frame.frame_index < oldest->frame.frame_index))
                oldest = candidate;
        }
        if (!oldest || retire(loop, oldest) != 0)
            return loop->result.state == GEARS_RUN_POST_SUBMIT_RETAIN ? 2 : -1;
    }
    loop->result.loop_elapsed_ns =
        loop->input.now_ns(loop->input.user) - loop->loop_start_ns;
    loop->result.frame_interval_ns_average = loop->result.frames_completed
        ? loop->result.loop_elapsed_ns / loop->result.frames_completed : 0u;
    loop->result.state = GEARS_RUN_COMPLETE;
    if (loop->result.frames_completed != 0u) {
        const uint64_t final_frame = loop->animation.next_frame_index - 1u;
        if (!gears_telemetry_should_sample(final_frame, 0))
            snapshot(loop, final_frame, 0, 1);
    }
    return 0;
}

int gears_frame_loop_result(GearsFrameLoop *loop,
                            GearsFrameRunnerResult *result)
{
    if (!loop || !loop->initialized || !result)
        return -1;
    (void)gears_telemetry_snapshot(&loop->stats, &loop->result.telemetry);
    *result = loop->result;
    return 0;
}
