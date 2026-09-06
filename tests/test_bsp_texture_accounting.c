#include "../src/bsp_texture_accounting.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

static BspTextureResidencyInput valid_input(void)
{
    return (BspTextureResidencyInput){
        200000u,
        60000u,
        10000u,
        20000u,
        30000u,
        12000u,
        32000u,
        8000u,
        10000u,
        2u,
    };
}

int main(void)
{
    BspTextureResidencyInput input = valid_input();
    BspTextureResidency resident;
    assert(bsp_texture_residency_build(&input, &resident) == 0);
    assert(resident.pool_resident_bytes == 144000u);
    assert(resident.dynamic_lightmap_allocation_bytes == 24000u);
    assert(resident.dynamic_lightmap_image_bytes == 20000u);
    assert(resident.texture_payload_bytes == 60000u);

    BspTextureAccounting accounting;
    assert(bsp_texture_accounting_init(&accounting, &input) == 0);
    BspTextureUploadFrame frame;
    assert(bsp_texture_accounting_record(
               &accounting, 0u, 128u, 10000u, 1, &frame) == 0);
    assert(frame.total_bytes == 10128u && frame.first_upload == 1u);
    assert(bsp_texture_accounting_record(
               &accounting, 1u, 128u, 10000u, 1, &frame) == 0);
    assert(bsp_texture_accounting_record(
               &accounting, 2u, 128u, 256u, 0, &frame) == 0);
    assert(frame.total_bytes == 384u && frame.cumulative_total_bytes == 20640u);
    assert(bsp_texture_accounting_record(
               &accounting, 3u, 128u, 256u, 0, &frame) == 0);

    BspTextureUploadSummary summary;
    assert(bsp_texture_accounting_finalize(&accounting, 4u, &summary) == 0);
    assert(summary.frames == 4u);
    assert(summary.transient_bytes_per_frame == 128u);
    assert(summary.bounded_lightmap_bytes_per_frame == 256u);
    assert(summary.transient_bytes_total == 512u);
    assert(summary.lightmap_bytes_total == 20512u);
    assert(summary.upload_bytes_total == 21024u);
    assert(summary.frame_bytes_min == 384u);
    assert(summary.frame_bytes_max == 10128u);
    assert(summary.full_upload_frames == 2u);
    assert(summary.bounded_upload_frames == 2u);
    assert(summary.sequence_hash != 0u);

    BspTextureAccounting repeat;
    assert(bsp_texture_accounting_init(&repeat, &input) == 0);
    for (uint64_t index = 0u; index < 4u; ++index)
        assert(bsp_texture_accounting_record(
                   &repeat, index, 128u, index < 2u ? 10000u : 256u,
                   index < 2u, &frame) == 0);
    assert(bsp_texture_accounting_finalize(&repeat, 4u, &summary) == 0);
    assert(summary.sequence_hash == accounting.upload.sequence_hash);

    BspTextureAccounting before = repeat;
    assert(bsp_texture_accounting_record(
               &repeat, 5u, 128u, 256u, 0, &frame) != 0);
    assert(memcmp(&before, &repeat, sizeof(repeat)) == 0);
    assert(bsp_texture_accounting_record(
               &repeat, 4u, 129u, 256u, 0, &frame) != 0);
    assert(memcmp(&before, &repeat, sizeof(repeat)) == 0);

    input = valid_input();
    input.pool_capacity_bytes = 100000u;
    assert(bsp_texture_residency_build(&input, &resident) != 0);
    input = valid_input();
    input.base_texture_bytes = UINT64_MAX;
    assert(bsp_texture_residency_build(&input, &resident) != 0);
    input = valid_input();
    input.dynamic_lightmap_slots = 1u;
    assert(bsp_texture_residency_build(&input, &resident) != 0);
    assert(bsp_texture_residency_build(0, &resident) != 0);
    assert(bsp_texture_accounting_init(0, &input) != 0);
    assert(bsp_texture_accounting_finalize(&accounting, 5u, &summary) != 0);
    return 0;
}
