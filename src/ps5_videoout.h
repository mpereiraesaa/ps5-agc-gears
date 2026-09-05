#ifndef PS5_AGC_GEARS_VIDEOOUT_H
#define PS5_AGC_GEARS_VIDEOOUT_H

#include "ps5_surface.h"
#include "../include/ps5_platform.h"

typedef int (*ps5_video_open_fn)(int32_t, int32_t, int32_t, const void *);
typedef int (*ps5_video_close_fn)(int32_t);
typedef int (*ps5_video_rate_fn)(int32_t, int32_t);
typedef int (*ps5_equeue_create_fn)(void **, const char *);
typedef int (*ps5_equeue_delete_fn)(void *);
typedef int (*ps5_video_event_add_fn)(void *, int32_t, void *);
typedef int (*ps5_video_event_delete_fn)(void *, int32_t);
typedef void (*ps5_video_attribute_fn)(void *, uint64_t, uint32_t, uint32_t,
                                       uint32_t, uint64_t, uint32_t, uint64_t);
typedef int (*ps5_video_register_fn)(int32_t, int32_t, int32_t, void *,
                                     int32_t, void *, int32_t, void *);
typedef int (*ps5_video_unregister_fn)(int32_t, int32_t);

struct ps5_videoout_ops {
    ps5_video_open_fn open;
    ps5_video_close_fn close;
    ps5_video_rate_fn set_flip_rate;
    ps5_equeue_create_fn create_equeue;
    ps5_equeue_delete_fn delete_equeue;
    ps5_video_event_add_fn add_flip_event;
    ps5_video_event_delete_fn delete_flip_event;
    ps5_video_attribute_fn set_attribute;
    ps5_video_register_fn register_buffers;
    ps5_video_unregister_fn unregister_buffers;
};

struct ps5_videoout {
    int32_t handle;
    void *equeue;
    int event_added;
    int buffers_registered;
    int unregister_deferred_to_close;
};

enum ps5_videoout_result {
    PS5_VIDEOOUT_OK = 0,
    PS5_VIDEOOUT_PRECONDITION = -1,
    PS5_VIDEOOUT_OPEN_FAILED = -2,
    PS5_VIDEOOUT_EQUEUE_FAILED = -3,
    PS5_VIDEOOUT_EVENT_FAILED = -4,
    PS5_VIDEOOUT_RATE_FAILED = -5,
    PS5_VIDEOOUT_REGISTER_FAILED = -6,
    PS5_VIDEOOUT_UNREGISTER_FAILED = -7,
    PS5_VIDEOOUT_EVENT_DELETE_FAILED = -8,
    PS5_VIDEOOUT_EQUEUE_DELETE_FAILED = -9,
    PS5_VIDEOOUT_CLOSE_FAILED = -10,
};

#define PS5_VIDEOOUT_RESOURCE_BUSY UINT32_C(0x80290009)

int ps5_videoout_open(struct ps5_videoout *video,
                      const struct ps5_videoout_ops *ops,
                      const struct ps5_surface_plan *plan,
                      void *framebuffer);
int ps5_videoout_close(struct ps5_videoout *video,
                       const struct ps5_videoout_ops *ops);

#endif
