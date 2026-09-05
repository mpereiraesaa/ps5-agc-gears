#include "../src/ps5_videoout.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int sequence;
static int fail_at;
static int unregister_rc;
static unsigned char queue_token;

static int step(int value) { return ++sequence == fail_at ? -99 : value; }
static int mock_open(int32_t user, int32_t bus, int32_t index, const void *p)
{ assert(user == 0xff && bus == 0 && index == 0 && !p); return step(7); }
static int mock_close(int32_t handle) { assert(handle == 7); return step(0); }
static int mock_rate(int32_t handle, int32_t rate)
{ assert(handle == 7 && rate == 0); return step(0); }
static int mock_create(void **queue, const char *name)
{ assert(name && name[0]); *queue = &queue_token; return step(0); }
static int mock_delete(void *queue)
{ assert(queue == &queue_token); return step(0); }
static int mock_add(void *queue, int32_t handle, void *user)
{ assert(queue == &queue_token && handle == 7 && !user); return step(0); }
static int mock_delete_event(void *queue, int32_t handle)
{ assert(queue == &queue_token && handle == 7); return step(0); }
static void mock_attribute(void *attribute, uint64_t format, uint32_t tiling,
                           uint32_t width, uint32_t height, uint64_t option,
                           uint32_t r0, uint64_t r1)
{
    assert(attribute && format == PS5_SURFACE_FORMAT_WORD && tiling == 0u &&
           width == 1920u && height == 1080u && option == 0u && r0 == 0u &&
           r1 == 0u);
    memset(attribute, 0x5a, sizeof(struct ps5_video_attribute));
}
static int mock_register(int32_t handle, int32_t set, int32_t start,
                         void *opaque, int32_t count, void *attribute,
                         int32_t option, void *reserved)
{
    const struct ps5_video_buffer *buffers = opaque;
    assert(handle == 7 && set == 0 && start == 0 && count == 2 && attribute &&
           option == 0 && !reserved && buffers[0].data && buffers[1].data ==
           (unsigned char *)buffers[0].data + PS5_SURFACE_BUFFER_STRIDE);
    return step(0);
}
static int mock_unregister(int32_t handle, int32_t set)
{ assert(handle == 7 && set == 0); ++sequence; return unregister_rc; }

static const struct ps5_videoout_ops ops = {
    mock_open, mock_close, mock_rate, mock_create, mock_delete, mock_add,
    mock_delete_event, mock_attribute, mock_register, mock_unregister
};

int main(void)
{
    struct ps5_surface_plan plan;
    assert(ps5_surface_make_plan(0u, &plan) == PS5_SURFACE_OK);
    unsigned char *framebuffer = malloc((size_t)PS5_SURFACE_ALLOCATION_BYTES);
    assert(framebuffer);
    struct ps5_videoout video;
    sequence = fail_at = unregister_rc = 0;
    assert(ps5_videoout_open(&video, &ops, &plan, framebuffer) ==
           PS5_VIDEOOUT_OK);
    assert(video.handle == 7 && video.equeue == &queue_token &&
           video.event_added && video.buffers_registered);
    assert(ps5_videoout_close(&video, &ops) == PS5_VIDEOOUT_OK);
    assert(video.handle == -1 && !video.equeue && !video.event_added &&
           !video.buffers_registered);

    sequence = fail_at = 0;
    unregister_rc = (int)PS5_VIDEOOUT_RESOURCE_BUSY;
    assert(ps5_videoout_open(&video, &ops, &plan, framebuffer) ==
           PS5_VIDEOOUT_OK);
    assert(ps5_videoout_close(&video, &ops) == PS5_VIDEOOUT_OK);
    assert(video.unregister_deferred_to_close && !video.buffers_registered);

    for (int injected_step = 1; injected_step <= 5; ++injected_step) {
        sequence = 0;
        fail_at = injected_step;
        unregister_rc = 0;
        const int rc = ps5_videoout_open(&video, &ops, &plan, framebuffer);
        assert(rc != PS5_VIDEOOUT_OK);
        fail_at = 0;
        (void)ps5_videoout_close(&video, &ops);
    }
    free(framebuffer);
    return 0;
}
