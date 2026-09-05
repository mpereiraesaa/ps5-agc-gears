#include <assert.h>
#include <string.h>

#include "../src/gears_frame_runner.h"

typedef struct Fake {
    uint64_t now;
    uint32_t calls;
    uint32_t samples;
    int fail_submit;
    int wrong_video_token;
    uint32_t in_flight;
    uint32_t max_in_flight;
    uint64_t last_token;
} Fake;

static uint64_t now_ns(void *opaque)
{
    Fake *fake = opaque;
    fake->now += 1000;
    return fake->now;
}
static int ok_frame(const GearsAnimationFrame *frame, void *opaque)
{
    Fake *fake = opaque;
    assert(frame->buffer == (frame->frame_index & 1));
    ++fake->calls;
    return 0;
}
static int submit_frame(const GearsAnimationFrame *frame, void *opaque)
{
    Fake *fake = opaque;
    if (fake->last_token)
        assert(frame->token == fake->last_token + 1);
    fake->last_token = frame->token;
    ++fake->calls;
    ++fake->in_flight;
    if (fake->in_flight > fake->max_in_flight)
        fake->max_in_flight = fake->in_flight;
    return fake->fail_submit && frame->frame_index == 7 ? -77 : 0;
}
static int wait_video(const GearsAnimationFrame *frame, uint64_t *token,
                      void *opaque)
{
    Fake *fake = opaque;
    ++fake->calls;
    *token = frame->token + (fake->wrong_video_token ? 1 : 0);
    assert(fake->in_flight > 0);
    --fake->in_flight;
    return 0;
}
static void sample(uint64_t frame, const GearsTelemetrySnapshot *snapshot,
                   int error, void *opaque)
{
    Fake *fake = opaque;
    (void)frame; (void)snapshot; (void)error;
    ++fake->samples;
}
static GearsFrameRunnerInput make_input(Fake *fake)
{
    GearsFrameRunnerInput input;
    memset(&input, 0, sizeof(input));
    input.start_ns = fake->now;
    input.first_flip_token = 0x420000000001;
    input.frame_deadline_ns = 10000;
    input.srd_tables[0] = 0x10000; input.srd_tables[1] = 0x20000;
    input.srd_tables[2] = 0x30000;
    input.vertex_counts[0] = 1200; input.vertex_counts[1] = 600;
    input.vertex_counts[2] = 600;
    input.now_ns = now_ns; input.compose = ok_frame; input.submit = submit_frame;
    input.wait_gpu = ok_frame; input.wait_videoout = wait_video;
    input.telemetry = sample; input.user = fake;
    return input;
}
int main(void)
{
    Fake fake = {.now = 1000000000};
    GearsFrameRunnerInput input = make_input(&fake);
    GearsFrameRunnerResult result;
    GearsFrameLoop loop;
    assert(gears_frame_loop_init(&loop, &input) == 0);
    for (uint32_t i = 0; i < 10000; ++i)
        assert(gears_frame_loop_step(&loop) == 0);
    assert(gears_frame_loop_drain(&loop) == 0);
    assert(gears_frame_loop_result(&loop, &result) == 0);
    assert(result.state == GEARS_RUN_COMPLETE && result.frames_completed == 10000);
    assert(result.telemetry.frames_completed == 10000);
    assert(result.max_frames_in_flight == 2);
    assert(result.loop_elapsed_ns > 0 && result.frame_interval_ns_average > 0);
    assert(fake.samples == 168); /* frame 0 + each 60th + final snapshot */
    assert(fake.max_in_flight == 2 && fake.in_flight == 0);

    memset(&fake, 0, sizeof(fake)); fake.now = 1000000000;
    input = make_input(&fake);
    assert(gears_frame_loop_init(&loop, &input) == 0);
    for (uint32_t i = 0; i < 10001; ++i)
        assert(gears_frame_loop_step(&loop) == 0);
    assert(gears_frame_loop_result(&loop, &result) == 0);
    assert(result.state == GEARS_RUN_ACTIVE);
    assert(result.frames_completed == 9999);
    assert(result.max_frames_in_flight == 2 && fake.in_flight == 2);
    assert(loop.animation.next_frame_index == 10001);
    assert(gears_frame_loop_drain(&loop) == 0);
    assert(gears_frame_loop_result(&loop, &result) == 0);
    assert(result.state == GEARS_RUN_COMPLETE);
    assert(result.frames_completed == 10001 && fake.in_flight == 0);

    memset(&fake, 0, sizeof(fake)); fake.now = 1000000000; fake.fail_submit = 1;
    input = make_input(&fake);
    assert(gears_frame_loop_init(&loop, &input) == 0);
    int rc = 0;
    for (uint32_t i = 0; i < 20 && rc == 0; ++i)
        rc = gears_frame_loop_step(&loop);
    assert(rc == 2);
    assert(gears_frame_loop_result(&loop, &result) == 0);
    assert(result.state == GEARS_RUN_POST_SUBMIT_RETAIN);
    assert(result.failed_frame == 7 && result.retained_token != 0);

    memset(&fake, 0, sizeof(fake)); fake.now = 1000000000;
    fake.wrong_video_token = 1; input = make_input(&fake);
    assert(gears_frame_loop_init(&loop, &input) == 0);
    assert(gears_frame_loop_step(&loop) == 0);
    assert(gears_frame_loop_step(&loop) == 0);
    assert(gears_frame_loop_drain(&loop) == 2);
    assert(gears_frame_loop_result(&loop, &result) == 0);
    assert(result.state == GEARS_RUN_POST_SUBMIT_RETAIN);
    assert(result.failed_frame == 0 && result.callback_result == -3);
    return 0;
}
