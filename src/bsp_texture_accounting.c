#include "bsp_texture_accounting.h"

#include <limits.h>
#include <string.h>

enum {
    EXPECTED_DYNAMIC_LIGHTMAP_SLOTS = 2,
};

static int add_u64(uint64_t *value, uint64_t addend)
{
    if (!value || UINT64_MAX - *value < addend)
        return -1;
    *value += addend;
    return 0;
}

static int multiply_u64(uint64_t value, uint64_t multiplier,
                        uint64_t *out)
{
    if (!out || (multiplier != 0u && value > UINT64_MAX / multiplier))
        return -1;
    *out = value * multiplier;
    return 0;
}

static uint64_t hash_u64(uint64_t hash, uint64_t value)
{
    for (unsigned byte = 0u; byte < 8u; ++byte) {
        hash ^= (value >> (byte * 8u)) & UINT64_C(0xff);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int bsp_texture_residency_build(const BspTextureResidencyInput *input,
                                BspTextureResidency *out)
{
    if (!input || !out || input->pool_capacity_bytes == 0u ||
        input->bsp_allocation_bytes == 0u ||
        input->shader_allocation_bytes == 0u ||
        input->depth_allocation_bytes == 0u ||
        input->transient_allocation_bytes == 0u ||
        input->dynamic_lightmap_slot_allocation_bytes == 0u ||
        input->base_texture_bytes == 0u ||
        input->source_lightmap_bytes == 0u ||
        input->dynamic_lightmap_image_bytes == 0u ||
        input->dynamic_lightmap_slots != EXPECTED_DYNAMIC_LIGHTMAP_SLOTS ||
        input->dynamic_lightmap_image_bytes >
            input->dynamic_lightmap_slot_allocation_bytes)
        return -1;

    uint64_t dynamic_allocation = 0u;
    uint64_t dynamic_images = 0u;
    if (multiply_u64(input->dynamic_lightmap_slot_allocation_bytes,
                     input->dynamic_lightmap_slots,
                     &dynamic_allocation) != 0 ||
        multiply_u64(input->dynamic_lightmap_image_bytes,
                     input->dynamic_lightmap_slots,
                     &dynamic_images) != 0)
        return -1;

    uint64_t bundle_texture_bytes = input->base_texture_bytes;
    if (add_u64(&bundle_texture_bytes, input->source_lightmap_bytes) != 0 ||
        bundle_texture_bytes > input->bsp_allocation_bytes)
        return -1;
    uint64_t texture_payload = bundle_texture_bytes;
    if (add_u64(&texture_payload, dynamic_images) != 0)
        return -1;

    uint64_t resident = input->bsp_allocation_bytes;
    if (add_u64(&resident, input->shader_allocation_bytes) != 0 ||
        add_u64(&resident, input->depth_allocation_bytes) != 0 ||
        add_u64(&resident, input->transient_allocation_bytes) != 0 ||
        add_u64(&resident, dynamic_allocation) != 0 ||
        resident > input->pool_capacity_bytes || texture_payload > resident)
        return -1;

    *out = (BspTextureResidency){
        input->pool_capacity_bytes,
        resident,
        input->bsp_allocation_bytes,
        input->shader_allocation_bytes,
        input->depth_allocation_bytes,
        input->transient_allocation_bytes,
        dynamic_allocation,
        texture_payload,
        input->base_texture_bytes,
        input->source_lightmap_bytes,
        dynamic_images,
        input->dynamic_lightmap_slots,
    };
    return 0;
}

int bsp_texture_accounting_init(BspTextureAccounting *accounting,
                                const BspTextureResidencyInput *input)
{
    if (!accounting)
        return -1;
    BspTextureResidency residency;
    if (bsp_texture_residency_build(input, &residency) != 0)
        return -1;
    memset(accounting, 0, sizeof(*accounting));
    accounting->residency = residency;
    accounting->upload.sequence_hash = UINT64_C(14695981039346656037);
    accounting->initialized = 1u;
    return 0;
}

int bsp_texture_accounting_record(BspTextureAccounting *accounting,
                                  uint64_t frame,
                                  uint64_t transient_bytes,
                                  uint64_t lightmap_bytes,
                                  int first_upload,
                                  BspTextureUploadFrame *out)
{
    if (!accounting || !accounting->initialized || !out ||
        frame != accounting->upload.frames || transient_bytes == 0u ||
        lightmap_bytes == 0u || (first_upload != 0 && first_upload != 1) ||
        (first_upload != (frame < accounting->residency.dynamic_lightmap_slots)) ||
        (first_upload && lightmap_bytes !=
            accounting->residency.dynamic_lightmap_image_bytes /
                accounting->residency.dynamic_lightmap_slots) ||
        (!first_upload && lightmap_bytes >=
            accounting->residency.dynamic_lightmap_image_bytes /
                accounting->residency.dynamic_lightmap_slots) ||
        (accounting->upload.transient_bytes_per_frame != 0u &&
         accounting->upload.transient_bytes_per_frame != transient_bytes) ||
        (accounting->upload.bounded_lightmap_bytes_per_frame != 0u &&
         !first_upload &&
         accounting->upload.bounded_lightmap_bytes_per_frame != lightmap_bytes))
        return -1;

    uint64_t frame_total = transient_bytes;
    uint64_t transient_total = accounting->upload.transient_bytes_total;
    uint64_t lightmap_total = accounting->upload.lightmap_bytes_total;
    uint64_t upload_total = accounting->upload.upload_bytes_total;
    if (add_u64(&frame_total, lightmap_bytes) != 0 ||
        add_u64(&transient_total, transient_bytes) != 0 ||
        add_u64(&lightmap_total, lightmap_bytes) != 0 ||
        add_u64(&upload_total, frame_total) != 0 ||
        accounting->upload.frames == UINT64_MAX ||
        (first_upload && accounting->upload.full_upload_frames == UINT64_MAX) ||
        (!first_upload &&
         accounting->upload.bounded_upload_frames == UINT64_MAX))
        return -1;

    BspTextureUploadSummary next = accounting->upload;
    next.transient_bytes_per_frame = transient_bytes;
    if (!first_upload)
        next.bounded_lightmap_bytes_per_frame = lightmap_bytes;
    next.transient_bytes_total = transient_total;
    next.lightmap_bytes_total = lightmap_total;
    next.upload_bytes_total = upload_total;
    next.frame_bytes_min = next.frames == 0u || frame_total < next.frame_bytes_min
                               ? frame_total
                               : next.frame_bytes_min;
    if (frame_total > next.frame_bytes_max)
        next.frame_bytes_max = frame_total;
    if (first_upload)
        ++next.full_upload_frames;
    else
        ++next.bounded_upload_frames;
    uint64_t hash = next.sequence_hash;
    hash = hash_u64(hash, frame);
    hash = hash_u64(hash, transient_bytes);
    hash = hash_u64(hash, lightmap_bytes);
    hash = hash_u64(hash, frame_total);
    hash = hash_u64(hash, (uint64_t)first_upload);
    next.sequence_hash = hash;
    ++next.frames;
    accounting->upload = next;
    *out = (BspTextureUploadFrame){
        frame,
        transient_bytes,
        lightmap_bytes,
        frame_total,
        transient_total,
        lightmap_total,
        upload_total,
        hash,
        (uint8_t)first_upload,
    };
    return 0;
}

int bsp_texture_accounting_finalize(
    const BspTextureAccounting *accounting, uint64_t expected_frames,
    BspTextureUploadSummary *out)
{
    if (!accounting || !accounting->initialized || !out ||
        expected_frames < accounting->residency.dynamic_lightmap_slots ||
        accounting->upload.frames != expected_frames ||
        accounting->upload.full_upload_frames !=
            accounting->residency.dynamic_lightmap_slots ||
        accounting->upload.bounded_upload_frames !=
            expected_frames - accounting->residency.dynamic_lightmap_slots ||
        accounting->upload.transient_bytes_per_frame == 0u ||
        accounting->upload.bounded_lightmap_bytes_per_frame == 0u ||
        accounting->upload.frame_bytes_min == 0u ||
        accounting->upload.frame_bytes_max <
            accounting->upload.frame_bytes_min ||
        accounting->upload.sequence_hash == 0u)
        return -1;
    uint64_t components = accounting->upload.transient_bytes_total;
    if (add_u64(&components, accounting->upload.lightmap_bytes_total) != 0 ||
        components != accounting->upload.upload_bytes_total)
        return -1;
    *out = accounting->upload;
    return 0;
}
