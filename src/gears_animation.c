#include "gears_animation.h"

#include <string.h>

int gears_animation_init(GearsAnimation *animation, uint64_t start_ns,
                         uint64_t first_flip_token)
{
    if (!animation || !start_ns) return -1;
    memset(animation, 0, sizeof(*animation));
    animation->start_ns = start_ns;
    return gears_frame_tracker_init(&animation->ownership, first_flip_token);
}

int gears_animation_prepare(
    GearsAnimation *animation, uint64_t now_ns,
    const uint32_t tables[GEARS_SCENE_DRAW_COUNT],
    const uint32_t counts[GEARS_SCENE_DRAW_COUNT], GearsAnimationFrame *frame)
{
    if (!animation || !frame || now_ns < animation->start_ns) return -1;
    const uint32_t buffer = (uint32_t)(animation->next_frame_index & 1u);
    uint64_t token = 0;
    if (gears_frame_begin(&animation->ownership, buffer, &token) != 0)
        return -2;
    memset(frame, 0, sizeof(*frame));
    frame->buffer = buffer;
    frame->token = token;
    frame->frame_index = animation->next_frame_index;
    frame->time_seconds = (float)((double)(now_ns - animation->start_ns) /
                                  1000000000.0);
    if (gears_build_scene(frame->draws, frame->time_seconds, tables, counts) != 0) {
        (void)gears_frame_cancel_pre_submit(&animation->ownership, buffer, token);
        return -3;
    }
    ++animation->next_frame_index;
    return 0;
}

int gears_animation_cancel(GearsAnimation *animation,
                           const GearsAnimationFrame *frame)
{
    return (!animation || !frame) ? -1 : gears_frame_cancel_pre_submit(
        &animation->ownership, frame->buffer, frame->token);
}

int gears_animation_mark_submitted(GearsAnimation *animation,
                                   const GearsAnimationFrame *frame)
{
    return (!animation || !frame) ? -1 : gears_frame_mark_submitted(
        &animation->ownership, frame->buffer, frame->token);
}

int gears_animation_mark_gpu_complete(GearsAnimation *animation,
                                      const GearsAnimationFrame *frame)
{
    return (!animation || !frame) ? -1 : gears_frame_mark_gpu_complete(
        &animation->ownership, frame->buffer, frame->token);
}

int gears_animation_mark_videoout_complete(GearsAnimation *animation,
                                           uint32_t buffer,
                                           uint64_t observed_token)
{
    return !animation ? -1 : gears_frame_mark_videoout_complete(
        &animation->ownership, buffer, observed_token);
}
