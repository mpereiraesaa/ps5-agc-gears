#include <assert.h>
#include <stdint.h>

#include "../src/gears_animation.h"

int main(void)
{
    const uint32_t tables[3] = {0x10000, 0x20000, 0x30000};
    const uint32_t counts[3] = {1200, 600, 600};
    const uint64_t start = UINT64_C(1000000000);
    const uint64_t first = UINT64_C(0x420000000001);
    GearsAnimation animation;
    assert(gears_animation_init(&animation, start, first) == 0);
    for (uint64_t i = 0; i < 10000; ++i) {
        GearsAnimationFrame frame;
        const uint64_t now = start + i * UINT64_C(16666667);
        assert(gears_animation_prepare(&animation, now, tables, counts,
                                       &frame) == 0);
        assert(frame.frame_index == i);
        assert(frame.buffer == (i & 1));
        assert(frame.token == first + i);
        assert(gears_animation_mark_submitted(&animation, &frame) == 0);
        assert(gears_animation_mark_videoout_complete(
                   &animation, frame.buffer, frame.token + 1) == -1);
        assert(gears_animation_mark_gpu_complete(&animation, &frame) == 0);
        assert(!gears_frame_can_reuse(&animation.ownership, frame.buffer));
        assert(gears_animation_mark_videoout_complete(
                   &animation, frame.buffer, frame.token) == 0);
        assert(gears_frame_can_reuse(&animation.ownership, frame.buffer));
    }
    assert(animation.next_frame_index == 10000);

    GearsAnimationFrame cancelled;
    assert(gears_animation_prepare(&animation,
        start + UINT64_C(10000) * UINT64_C(16666667), tables, counts,
        &cancelled) == 0);
    assert(gears_animation_cancel(&animation, &cancelled) == 0);
    assert(gears_animation_mark_submitted(&animation, &cancelled) == -1);
    return 0;
}
