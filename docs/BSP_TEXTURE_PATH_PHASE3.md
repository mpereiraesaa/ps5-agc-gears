# BSP texture path — Phase 3

Phase 3 builds on the exact-token resource foundation. It is intentionally
ordered: each hardware gate must pass before the next texture feature changes
the bundle ABI, descriptors or pipeline permutations.

## Gate 1: bounded dynamic lightmap

The first gate is complete on FW 12.02. It keeps two full lightmap images,
one for each display/resource slot. Each image is reused only after that slot's
GPU fence reaches zero and VideoOut reports the exact submitted token. The
per-frame patch payload is allocated from the matching open transient slot.

The patch is not chosen from private map-specific constants. At startup the
runtime intersects the bundle's initial camera ray with the nearest opaque,
lit triangle and centers an 8x8-texel patch on the interpolated lightmap UV.
Selection fails closed if no suitable surface is hit. For the private reference
bundle this chooses face 3100 and a 256-byte patch; those values are telemetry,
not source configuration.

Each frame writes one of two deterministic luminance values into transient
staging, copies only the patch rows into the retired slot's lightmap image,
flushes the CPU-written span and emits a second scoped `AcquireMem` using the
Phase 2 observed tuple. Descriptor tables are still rebuilt in the transient
slot and point at that slot's lightmap image. The first use of each image flushes
and acquires the complete initialized image; later frames upload 256 patch bytes
and acquire the aligned span covering those rows. The fixed overlay color keeps
the two terminal framebuffer hashes attributable to the alternating lightmap.

Each image has 256-byte prefix and suffix guards. A hash of every byte outside
the patch is captured at initialization and recomputed after the final drain.
Acceptance therefore requires all of the following:

- deterministic, distinct A/B patch hashes at frames 0, 1, 9,998 and 9,999;
- two distinct final GPU framebuffer readbacks;
- stable bytes everywhere outside the patch and intact image/color/depth guards;
- exact fence plus VideoOut-token retirement before slot reuse;
- 10,000 completed and connected frames, no pad reads or renderer errors and no
  presentation intervals over 17 ms;
- all six resource-pool allocations reclaimed against the final exact token;
- a gap-free `bsp-texture-path-lightmap-soak-complete` BYE.

`validate_texture_path_lightmap_evidence.py` validates the immutable server
manifest and transcript against the exact private bundle identity. The accepted
run is
`20260906T150442704Z_PPSA99997_ps5-agc-gears_0x659df0b5b957`: 10,000 frames,
217 structured records, 64,259,072 resident allocation bytes, 173,632,448 total
CPU-to-GPU upload bytes, six reclaimed allocations, intact guards and zero
errors. Its transcript SHA-256 is
`e163c166e3ca5a733570c803b75decdbb4251b608907aff8b8a5126ae27b5ac9`.
The native ELF/fSELF SHA-256 values are
`a922c3d2fbb028cec80e27f17b9634d1e2317987eac507bf2f2e6b232a6133c6`
and
`b7b52e38580e3a22612c279632ad92c2b638c8004894c5df10490e5d183968aa`.

The Remote Play screenshot and post-soak static recording remain private. Their
SHA-256 values are
`32cb65a640e6c319f979aab842916bb1274da1cf6a66126bfe12b842b46a33a6`
and
`329a934820519db511a7488375a20bbf4d4e3734abc6ea6f393ff9b88f1fbe32`.
The run reused the existing registered Chiaki console entry and the previously
validated input path; it did not pair Chiaki or require another DualSense
movement gate.

## Remaining ordered gates

Gate 1 does not claim completion of Phase 3. The remaining order is:

1. deterministic bundle mip chains plus mip-aware T# descriptors and separate
   trilinear/anisotropic S# variants;
2. an alpha-test pipeline permutation for `{` textures;
3. a separate sky pass;
4. consolidated resident/upload telemetry and reproducible host/hardware gates;
5. the final 60,000-frame structured soak and pull request.

Entities, PVS, water, sprites/models, platform/audio and engine integration
remain outside this phase.
