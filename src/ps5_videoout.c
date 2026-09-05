#include "ps5_videoout.h"

#include <string.h>

static int valid_ops(const struct ps5_videoout_ops *ops)
{
    return ops && ops->open && ops->close && ops->set_flip_rate &&
           ops->create_equeue && ops->delete_equeue && ops->add_flip_event &&
           ops->delete_flip_event && ops->set_attribute &&
           ops->register_buffers && ops->unregister_buffers;
}

int ps5_videoout_open(struct ps5_videoout *video,
                      const struct ps5_videoout_ops *ops,
                      const struct ps5_surface_plan *plan,
                      void *framebuffer)
{
    if (!video || !valid_ops(ops) || !plan || !framebuffer ||
        plan->register_count != PS5_SURFACE_BUFFER_COUNT ||
        plan->allocation_bytes != PS5_SURFACE_ALLOCATION_BYTES ||
        plan->buffer_offsets[0] != 0u ||
        plan->buffer_offsets[1] != PS5_SURFACE_BUFFER_STRIDE)
        return PS5_VIDEOOUT_PRECONDITION;
    memset(video, 0, sizeof(*video));
    video->handle = -1;
    video->handle = ops->open(0xff, 0, 0, 0);
    if (video->handle < 0)
        return PS5_VIDEOOUT_OPEN_FAILED;
    if (ops->create_equeue(&video->equeue, "ps5-agc-gears-flip") != 0 ||
        !video->equeue)
        return PS5_VIDEOOUT_EQUEUE_FAILED;
    if (ops->add_flip_event(video->equeue, video->handle, 0) != 0)
        return PS5_VIDEOOUT_EVENT_FAILED;
    video->event_added = 1;
    if (ops->set_flip_rate(video->handle, (int32_t)plan->flip_rate) != 0)
        return PS5_VIDEOOUT_RATE_FAILED;

    struct ps5_video_buffer buffers[PS5_SURFACE_BUFFER_COUNT] = {
        {framebuffer, 0, 0, 0},
        {(unsigned char *)framebuffer + plan->buffer_offsets[1], 0, 0, 0},
    };
    struct ps5_video_attribute attribute = {{0}};
    ops->set_attribute(&attribute, plan->format_word, 0u, plan->width,
                       plan->height, 0u, 0u, 0u);
    if (ops->register_buffers(video->handle, 0,
                              (int32_t)plan->register_start_index, buffers,
                              (int32_t)plan->register_count, &attribute, 0,
                              0) != 0)
        return PS5_VIDEOOUT_REGISTER_FAILED;
    video->buffers_registered = 1;
    return PS5_VIDEOOUT_OK;
}

int ps5_videoout_close(struct ps5_videoout *video,
                       const struct ps5_videoout_ops *ops)
{
    if (!video || !valid_ops(ops))
        return PS5_VIDEOOUT_PRECONDITION;
    if (video->buffers_registered) {
        const int rc = ops->unregister_buffers(video->handle, 0);
        if (rc == 0)
            video->buffers_registered = 0;
        else if ((uint32_t)rc == PS5_VIDEOOUT_RESOURCE_BUSY)
            video->unregister_deferred_to_close = 1;
        else
            return PS5_VIDEOOUT_UNREGISTER_FAILED;
    }
    if (video->event_added) {
        if (ops->delete_flip_event(video->equeue, video->handle) != 0)
            return PS5_VIDEOOUT_EVENT_DELETE_FAILED;
        video->event_added = 0;
    }
    if (video->equeue) {
        if (ops->delete_equeue(video->equeue) != 0)
            return PS5_VIDEOOUT_EQUEUE_DELETE_FAILED;
        video->equeue = 0;
    }
    if (video->handle >= 0) {
        if (ops->close(video->handle) != 0)
            return PS5_VIDEOOUT_CLOSE_FAILED;
        video->handle = -1;
        video->buffers_registered = 0;
    }
    return PS5_VIDEOOUT_OK;
}
