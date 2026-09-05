#ifndef PS5_AGC_GEARS_FRAME_TRACKER_H
#define PS5_AGC_GEARS_FRAME_TRACKER_H

#include <stdint.h>

enum { GEARS_FRAME_BUFFER_COUNT = 2 };

typedef enum GearsFramePhase {
    GEARS_FRAME_EMPTY = 0,
    GEARS_FRAME_PREPARED = 1,
    GEARS_FRAME_SUBMITTED = 2,
} GearsFramePhase;

typedef struct GearsFrameSlot {
    uint64_t token;
    GearsFramePhase phase;
    uint8_t gpu_complete;
    uint8_t videoout_complete;
} GearsFrameSlot;

typedef struct GearsFrameTracker {
    uint64_t next_token;
    GearsFrameSlot slots[GEARS_FRAME_BUFFER_COUNT];
} GearsFrameTracker;

int gears_frame_tracker_init(GearsFrameTracker *tracker, uint64_t first_token);
int gears_frame_begin(GearsFrameTracker *tracker, uint32_t buffer,
                      uint64_t *token_out);
int gears_frame_cancel_pre_submit(GearsFrameTracker *tracker, uint32_t buffer,
                                  uint64_t token);
int gears_frame_mark_submitted(GearsFrameTracker *tracker, uint32_t buffer,
                               uint64_t token);
int gears_frame_mark_gpu_complete(GearsFrameTracker *tracker, uint32_t buffer,
                                  uint64_t token);
int gears_frame_mark_videoout_complete(GearsFrameTracker *tracker,
                                       uint32_t buffer, uint64_t token);
int gears_frame_can_reuse(const GearsFrameTracker *tracker, uint32_t buffer);

#endif
