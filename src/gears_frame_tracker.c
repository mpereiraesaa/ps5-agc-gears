#include "gears_frame_tracker.h"

#include <string.h>

static const uint64_t kMaxPositiveFlipToken = UINT64_C(0x00007fffffffffff);

static GearsFrameSlot *slot_for(GearsFrameTracker *tracker, uint32_t buffer)
{
    return (!tracker || buffer >= GEARS_FRAME_BUFFER_COUNT) ?
           0 : &tracker->slots[buffer];
}

int gears_frame_tracker_init(GearsFrameTracker *tracker, uint64_t first_token)
{
    if (!tracker || !first_token || first_token > kMaxPositiveFlipToken)
        return -1;
    memset(tracker, 0, sizeof(*tracker));
    tracker->next_token = first_token;
    return 0;
}

int gears_frame_can_reuse(const GearsFrameTracker *tracker, uint32_t buffer)
{
    if (!tracker || buffer >= GEARS_FRAME_BUFFER_COUNT) return 0;
    const GearsFrameSlot *slot = &tracker->slots[buffer];
    return slot->phase == GEARS_FRAME_EMPTY ||
           (slot->phase == GEARS_FRAME_SUBMITTED && slot->gpu_complete &&
            slot->videoout_complete);
}

int gears_frame_begin(GearsFrameTracker *tracker, uint32_t buffer,
                      uint64_t *token_out)
{
    GearsFrameSlot *slot = slot_for(tracker, buffer);
    if (!slot || !token_out || !gears_frame_can_reuse(tracker, buffer) ||
        !tracker->next_token || tracker->next_token > kMaxPositiveFlipToken)
        return -1;
    memset(slot, 0, sizeof(*slot));
    slot->token = tracker->next_token++;
    slot->phase = GEARS_FRAME_PREPARED;
    *token_out = slot->token;
    return 0;
}

int gears_frame_cancel_pre_submit(GearsFrameTracker *tracker, uint32_t buffer,
                                  uint64_t token)
{
    GearsFrameSlot *slot = slot_for(tracker, buffer);
    if (!slot || slot->phase != GEARS_FRAME_PREPARED || slot->token != token)
        return -1;
    memset(slot, 0, sizeof(*slot));
    return 0;
}

int gears_frame_mark_submitted(GearsFrameTracker *tracker, uint32_t buffer,
                               uint64_t token)
{
    GearsFrameSlot *slot = slot_for(tracker, buffer);
    if (!slot || slot->phase != GEARS_FRAME_PREPARED || slot->token != token)
        return -1;
    slot->phase = GEARS_FRAME_SUBMITTED;
    return 0;
}

int gears_frame_mark_gpu_complete(GearsFrameTracker *tracker, uint32_t buffer,
                                  uint64_t token)
{
    GearsFrameSlot *slot = slot_for(tracker, buffer);
    if (!slot || slot->phase != GEARS_FRAME_SUBMITTED || slot->token != token)
        return -1;
    slot->gpu_complete = 1;
    return 0;
}

int gears_frame_mark_videoout_complete(GearsFrameTracker *tracker,
                                       uint32_t buffer, uint64_t token)
{
    GearsFrameSlot *slot = slot_for(tracker, buffer);
    if (!slot || slot->phase != GEARS_FRAME_SUBMITTED || slot->token != token)
        return -1;
    slot->videoout_complete = 1;
    return 0;
}
