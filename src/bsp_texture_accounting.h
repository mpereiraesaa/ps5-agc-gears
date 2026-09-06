#ifndef PS5_AGC_GEARS_BSP_TEXTURE_ACCOUNTING_H
#define PS5_AGC_GEARS_BSP_TEXTURE_ACCOUNTING_H

#include <stdint.h>

typedef struct BspTextureResidencyInput {
    uint64_t pool_capacity_bytes;
    uint64_t bsp_allocation_bytes;
    uint64_t shader_allocation_bytes;
    uint64_t depth_allocation_bytes;
    uint64_t transient_allocation_bytes;
    uint64_t dynamic_lightmap_slot_allocation_bytes;
    uint64_t base_texture_bytes;
    uint64_t source_lightmap_bytes;
    uint64_t dynamic_lightmap_image_bytes;
    uint32_t dynamic_lightmap_slots;
} BspTextureResidencyInput;

typedef struct BspTextureResidency {
    uint64_t pool_capacity_bytes;
    uint64_t pool_resident_bytes;
    uint64_t bsp_allocation_bytes;
    uint64_t shader_allocation_bytes;
    uint64_t depth_allocation_bytes;
    uint64_t transient_allocation_bytes;
    uint64_t dynamic_lightmap_allocation_bytes;
    uint64_t texture_payload_bytes;
    uint64_t base_texture_bytes;
    uint64_t source_lightmap_bytes;
    uint64_t dynamic_lightmap_image_bytes;
    uint32_t dynamic_lightmap_slots;
} BspTextureResidency;

typedef struct BspTextureUploadFrame {
    uint64_t frame;
    uint64_t transient_bytes;
    uint64_t lightmap_bytes;
    uint64_t total_bytes;
    uint64_t cumulative_transient_bytes;
    uint64_t cumulative_lightmap_bytes;
    uint64_t cumulative_total_bytes;
    uint64_t sequence_hash;
    uint8_t first_upload;
} BspTextureUploadFrame;

typedef struct BspTextureUploadSummary {
    uint64_t frames;
    uint64_t transient_bytes_per_frame;
    uint64_t bounded_lightmap_bytes_per_frame;
    uint64_t transient_bytes_total;
    uint64_t lightmap_bytes_total;
    uint64_t upload_bytes_total;
    uint64_t frame_bytes_min;
    uint64_t frame_bytes_max;
    uint64_t sequence_hash;
    uint64_t full_upload_frames;
    uint64_t bounded_upload_frames;
} BspTextureUploadSummary;

typedef struct BspTextureAccounting {
    BspTextureResidency residency;
    BspTextureUploadSummary upload;
    uint8_t initialized;
} BspTextureAccounting;

int bsp_texture_residency_build(const BspTextureResidencyInput *input,
                                BspTextureResidency *out);
int bsp_texture_accounting_init(BspTextureAccounting *accounting,
                                const BspTextureResidencyInput *input);
int bsp_texture_accounting_record(BspTextureAccounting *accounting,
                                  uint64_t frame,
                                  uint64_t transient_bytes,
                                  uint64_t lightmap_bytes,
                                  int first_upload,
                                  BspTextureUploadFrame *out);
int bsp_texture_accounting_finalize(
    const BspTextureAccounting *accounting, uint64_t expected_frames,
    BspTextureUploadSummary *out);

#endif
