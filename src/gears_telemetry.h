#ifndef PS5_AGC_GEARS_TELEMETRY_H
#define PS5_AGC_GEARS_TELEMETRY_H

#include <stdint.h>

typedef struct GearsTelemetry {
    uint64_t frames_completed;
    uint64_t compose_ns_total;
    uint64_t gpu_wait_ns_total;
    uint64_t video_wait_ns_total;
    uint64_t compose_ns_max;
    uint64_t gpu_wait_ns_max;
    uint64_t video_wait_ns_max;
    uint64_t deadline_misses;
    uint64_t errors;
} GearsTelemetry;

typedef struct GearsTelemetrySnapshot {
    uint64_t frames_completed;
    uint64_t compose_ns_average;
    uint64_t gpu_wait_ns_average;
    uint64_t video_wait_ns_average;
    uint64_t compose_ns_max;
    uint64_t gpu_wait_ns_max;
    uint64_t video_wait_ns_max;
    uint64_t deadline_misses;
    uint64_t errors;
} GearsTelemetrySnapshot;

void gears_telemetry_reset(GearsTelemetry *telemetry);
int gears_telemetry_record_frame(GearsTelemetry *telemetry,
                                 uint64_t compose_ns, uint64_t gpu_wait_ns,
                                 uint64_t video_wait_ns, int deadline_missed);
void gears_telemetry_record_error(GearsTelemetry *telemetry);
int gears_telemetry_snapshot(const GearsTelemetry *telemetry,
                             GearsTelemetrySnapshot *snapshot);
int gears_telemetry_should_sample(uint64_t frame_index, int error);

#endif
