#include <assert.h>
#include <stdint.h>

#include "../src/gears_telemetry.h"

int main(void)
{
    GearsTelemetry telemetry;
    gears_telemetry_reset(&telemetry);
    for (uint64_t frame = 0; frame < 10000; ++frame) {
        assert(gears_telemetry_record_frame(
            &telemetry, 100 + frame, 200 + frame * 2,
            300 + frame * 3, frame ? 16666667 : 0,
            frame == 123) == 0);
        assert(gears_telemetry_should_sample(frame, 0) ==
               (frame == 0 || ((frame + 1) % 60) == 0));
    }
    gears_telemetry_record_error(&telemetry);
    assert(gears_telemetry_should_sample(7, 1));
    GearsTelemetrySnapshot snapshot;
    assert(gears_telemetry_snapshot(&telemetry, &snapshot) == 0);
    assert(snapshot.frames_completed == 10000);
    assert(snapshot.compose_ns_average == 5099);
    assert(snapshot.gpu_wait_ns_average == 10199);
    assert(snapshot.video_wait_ns_average == 15298);
    assert(snapshot.compose_ns_max == 10099);
    assert(snapshot.gpu_wait_ns_max == 20198);
    assert(snapshot.video_wait_ns_max == 30297);
    assert(snapshot.present_intervals == 9999);
    assert(snapshot.present_interval_ns_average == 16666667);
    assert(snapshot.present_interval_ns_max == 16666667);
    assert(snapshot.present_interval_over_budget == 1 && snapshot.errors == 1);
    telemetry.compose_ns_total = UINT64_MAX;
    assert(gears_telemetry_record_frame(&telemetry, 1, 0, 0, 0, 0) == -1);
    return 0;
}
