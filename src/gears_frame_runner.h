#ifndef PS5_AGC_GEARS_FRAME_RUNNER_H
#define PS5_AGC_GEARS_FRAME_RUNNER_H

#include <stdint.h>

#include "gears_animation.h"
#include "gears_telemetry.h"

typedef uint64_t (*GearsNowNsFn)(void *user);
typedef int (*GearsFrameFn)(const GearsAnimationFrame *frame, void *user);
typedef int (*GearsVideoWaitFn)(const GearsAnimationFrame *frame,
                                uint64_t *observed_token, void *user);
typedef void (*GearsTelemetryFn)(uint64_t frame_index,
                                 const GearsTelemetrySnapshot *snapshot,
                                 int terminal_error, void *user);

typedef enum GearsRunState {
    GEARS_RUN_ACTIVE = 0,
    GEARS_RUN_COMPLETE = 1,
    GEARS_RUN_PRE_SUBMIT_FAILURE = 2,
    GEARS_RUN_POST_SUBMIT_RETAIN = 3,
} GearsRunState;

typedef struct GearsFrameRunnerInput {
    uint64_t start_ns;
    uint64_t first_flip_token;
    uint64_t frame_deadline_ns;
    uint32_t srd_tables[GEARS_SCENE_DRAW_COUNT];
    uint32_t vertex_counts[GEARS_SCENE_DRAW_COUNT];
    GearsNowNsFn now_ns;
    GearsFrameFn compose;
    GearsFrameFn submit;
    GearsFrameFn wait_gpu;
    GearsVideoWaitFn wait_videoout;
    GearsTelemetryFn telemetry;
    void *user;
} GearsFrameRunnerInput;

typedef struct GearsFrameRunnerResult {
    GearsRunState state;
    uint64_t frames_completed;
    uint32_t max_frames_in_flight;
    uint64_t failed_frame;
    int callback_result;
    uint64_t retained_token;
    uint64_t loop_elapsed_ns;
    uint64_t frame_interval_ns_average;
    GearsTelemetrySnapshot telemetry;
} GearsFrameRunnerResult;

typedef struct GearsFrameLoopPending {
    GearsAnimationFrame frame;
    uint64_t frame_start;
    uint64_t compose_ns;
    int active;
} GearsFrameLoopPending;

typedef struct GearsFrameLoop {
    GearsFrameRunnerInput input;
    GearsAnimation animation;
    GearsTelemetry stats;
    GearsFrameLoopPending pending[GEARS_FRAME_BUFFER_COUNT];
    GearsFrameRunnerResult result;
    uint32_t active_frames;
    uint64_t loop_start_ns;
    int initialized;
} GearsFrameLoop;

int gears_frame_loop_init(GearsFrameLoop *loop,
                          const GearsFrameRunnerInput *input);
int gears_frame_loop_step(GearsFrameLoop *loop);
int gears_frame_loop_drain(GearsFrameLoop *loop);
int gears_frame_loop_result(GearsFrameLoop *loop,
                            GearsFrameRunnerResult *result);

#endif
