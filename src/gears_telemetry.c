#include "gears_telemetry.h"

#include <string.h>

static uint64_t maximum(uint64_t a, uint64_t b) { return a > b ? a : b; }

void gears_telemetry_reset(GearsTelemetry *telemetry)
{
    if (telemetry) memset(telemetry, 0, sizeof(*telemetry));
}

int gears_telemetry_record_frame(GearsTelemetry *telemetry,
                                 uint64_t compose_ns, uint64_t gpu_wait_ns,
                                 uint64_t video_wait_ns, int deadline_missed)
{
    if (!telemetry || telemetry->frames_completed == UINT64_MAX ||
        (deadline_missed && telemetry->deadline_misses == UINT64_MAX) ||
        UINT64_MAX - telemetry->compose_ns_total < compose_ns ||
        UINT64_MAX - telemetry->gpu_wait_ns_total < gpu_wait_ns ||
        UINT64_MAX - telemetry->video_wait_ns_total < video_wait_ns)
        return -1;
    ++telemetry->frames_completed;
    telemetry->compose_ns_total += compose_ns;
    telemetry->gpu_wait_ns_total += gpu_wait_ns;
    telemetry->video_wait_ns_total += video_wait_ns;
    telemetry->compose_ns_max = maximum(telemetry->compose_ns_max, compose_ns);
    telemetry->gpu_wait_ns_max = maximum(telemetry->gpu_wait_ns_max, gpu_wait_ns);
    telemetry->video_wait_ns_max = maximum(telemetry->video_wait_ns_max,
                                            video_wait_ns);
    if (deadline_missed) ++telemetry->deadline_misses;
    return 0;
}

void gears_telemetry_record_error(GearsTelemetry *telemetry)
{
    if (telemetry && telemetry->errors != UINT64_MAX) ++telemetry->errors;
}

int gears_telemetry_snapshot(const GearsTelemetry *telemetry,
                             GearsTelemetrySnapshot *snapshot)
{
    if (!telemetry || !snapshot) return -1;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->frames_completed = telemetry->frames_completed;
    if (telemetry->frames_completed) {
        snapshot->compose_ns_average = telemetry->compose_ns_total /
                                       telemetry->frames_completed;
        snapshot->gpu_wait_ns_average = telemetry->gpu_wait_ns_total /
                                        telemetry->frames_completed;
        snapshot->video_wait_ns_average = telemetry->video_wait_ns_total /
                                          telemetry->frames_completed;
    }
    snapshot->compose_ns_max = telemetry->compose_ns_max;
    snapshot->gpu_wait_ns_max = telemetry->gpu_wait_ns_max;
    snapshot->video_wait_ns_max = telemetry->video_wait_ns_max;
    snapshot->deadline_misses = telemetry->deadline_misses;
    snapshot->errors = telemetry->errors;
    return 0;
}

int gears_telemetry_should_sample(uint64_t frame_index, int error)
{
    return error || frame_index == 0 || ((frame_index + 1) % 60) == 0;
}
