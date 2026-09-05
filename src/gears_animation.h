#ifndef PS5_AGC_GEARS_ANIMATION_H
#define PS5_AGC_GEARS_ANIMATION_H

#include <stdint.h>

#include "gears_frame_tracker.h"
#include "gears_scene.h"

typedef struct GearsAnimationFrame {
    uint32_t buffer;
    uint64_t token;
    uint64_t frame_index;
    float time_seconds;
    GearsSceneDraw draws[GEARS_SCENE_DRAW_COUNT];
} GearsAnimationFrame;

typedef struct GearsAnimation {
    GearsFrameTracker ownership;
    uint64_t start_ns;
    uint64_t next_frame_index;
} GearsAnimation;

int gears_animation_init(GearsAnimation *animation, uint64_t start_ns,
                         uint64_t first_flip_token);
int gears_animation_prepare(
    GearsAnimation *animation, uint64_t now_ns,
    const uint32_t srd_tables[GEARS_SCENE_DRAW_COUNT],
    const uint32_t vertex_counts[GEARS_SCENE_DRAW_COUNT],
    GearsAnimationFrame *frame);
int gears_animation_cancel(GearsAnimation *animation,
                           const GearsAnimationFrame *frame);
int gears_animation_mark_submitted(GearsAnimation *animation,
                                   const GearsAnimationFrame *frame);
int gears_animation_mark_gpu_complete(GearsAnimation *animation,
                                      const GearsAnimationFrame *frame);
int gears_animation_mark_videoout_complete(GearsAnimation *animation,
                                           uint32_t buffer,
                                           uint64_t observed_token);

#endif
