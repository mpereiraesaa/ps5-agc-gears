#include <assert.h>

#include "../src/gears_frame_tracker.h"

int main(void)
{
    GearsFrameTracker tracker;
    uint64_t token0, token1, token2;
    assert(gears_frame_tracker_init(&tracker, 0x420000000001) == 0);
    assert(gears_frame_begin(&tracker, 0, &token0) == 0);
    assert(gears_frame_begin(&tracker, 0, &token2) == -1);
    assert(gears_frame_mark_submitted(&tracker, 0, token0) == 0);
    assert(gears_frame_cancel_pre_submit(&tracker, 0, token0) == -1);
    assert(gears_frame_begin(&tracker, 1, &token1) == 0);
    assert(token1 == token0 + 1);
    assert(gears_frame_cancel_pre_submit(&tracker, 1, token1) == 0);
    assert(gears_frame_mark_gpu_complete(&tracker, 0, token0) == 0);
    assert(!gears_frame_can_reuse(&tracker, 0));
    assert(gears_frame_mark_videoout_complete(&tracker, 0, token0 + 1) == -1);
    assert(gears_frame_mark_videoout_complete(&tracker, 0, token0) == 0);
    assert(gears_frame_can_reuse(&tracker, 0));
    assert(gears_frame_begin(&tracker, 0, &token2) == 0);
    assert(token2 == token1 + 1);
    assert(gears_frame_tracker_init(&tracker, 0) == -1);
    assert(gears_frame_tracker_init(&tracker, 0x0000800000000000) == -1);
    return 0;
}
