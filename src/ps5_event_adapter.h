#ifndef PS5_AGC_GEARS_EVENT_ADAPTER_H
#define PS5_AGC_GEARS_EVENT_ADAPTER_H

#include "ps5_frame_completion.h"

typedef int (*ps5_wait_equeue_fn)(void *queue, void *event, int count,
                                  int *out, unsigned int *timeout_us);
typedef int (*ps5_decode_flip_event_fn)(const void *event, int64_t *flip_arg);

struct ps5_event_diagnostics {
    uint32_t wait_calls;
    int last_wait_rc;
    int last_event_count;
    uint32_t decode_calls;
    int last_decode_rc;
    int64_t last_flip_arg;
};

struct ps5_event_poll {
    void *equeue;
    void *event_storage;
    volatile uint64_t *gpu_fence;
    unsigned int wait_timeout_us;
    int fence_deadline_expired;
    int videoout_deadline_expired;
    ps5_wait_equeue_fn wait_equeue;
    ps5_decode_flip_event_fn decode_flip_event;
    struct ps5_event_diagnostics *diagnostics;
};

enum ps5_event_adapter_result {
    PS5_EVENT_PRECONDITION = -30,
    PS5_EVENT_INVALID = -31,
};

int ps5_event_poll_completion(struct ps5_frame_completion *state,
                              const struct ps5_event_poll *poll);

#endif
