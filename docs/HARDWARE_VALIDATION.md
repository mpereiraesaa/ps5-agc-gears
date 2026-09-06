# Hardware validation

Hardware claims in this repository refer to one PS5 running firmware 12.02.
They are not compatibility claims for other firmware or consoles. Run logs and
captures live in the private parent laboratory; this public boundary records
only sanitized identifiers, hashes, outcomes and known limitations.

## Phase 3 dynamic-lightmap gate

The first ordered Phase 3 gate passed 10,000 frames on FW 12.02:

- Run: `20260906T150442704Z_PPSA99997_ps5-agc-gears_0x659df0b5b957`
- Native ELF SHA-256:
  `a922c3d2fbb028cec80e27f17b9634d1e2317987eac507bf2f2e6b232a6133c6`
- Signed fSELF SHA-256:
  `b7b52e38580e3a22612c279632ad92c2b638c8004894c5df10490e5d183968aa`
- Transcript/manifest SHA-256:
  `e163c166e3ca5a733570c803b75decdbb4251b608907aff8b8a5126ae27b5ac9` /
  `db345dc5ff7be97a22d8975639f6b4f97b3b0535f20fb0d75f4a86150d60c9d5`
- Requested/completed/connected: 10,000/10,000/10,000
- Dynamic patch: 8x8 texels, 256 bytes per bounded update
- A/B patch hashes: `2590af3457808025` / `89d8b73bb162b925`
- Final GPU framebuffer hashes: `7d766580357827ce` / `e3691c96c36c021c`
- Resident/total uploaded bytes: 64,259,072 / 173,632,448
- Image slots/pool allocations reclaimed: 2/6
- Outside-patch bytes and all guards: stable/intact
- Pad reads, presentation-budget and renderer errors: 0/0/0
- Teardown: gap-free BYE at sequence 217, exact-title closure and four healthy
  payload services

The fail-closed validator accepted the immutable manifest against the exact
private bundle identity. A Remote Play screenshot confirms the map remains
visually correct; its SHA-256 is
`32cb65a640e6c319f979aab842916bb1274da1cf6a66126bfe12b842b46a33a6`.
See `BSP_TEXTURE_PATH_PHASE3.md` for the ownership and cache contract. This
gate proves only the first ordered Phase 3 slice, not the later mip, alpha-test,
sky or final 60,000-frame gates.

## Phase 2 resource-foundation status

Phase 2 passed its complete 60,000-frame FW 12.02 hardware gate:

- Run: `20260906T130036578Z_PPSA99997_ps5-agc-gears_0x5ed84765862b`
- Native ELF SHA-256:
  `1a1f33e840d919f090accce6d4a5593044058c0f32386a2986cd2715e64b050c`
- Signed fSELF SHA-256:
  `695a5db926fc36e4d246c866e2d47cb0de8bd55904d495c1bf646f16c211e756`
- Private `c1a0` bundle SHA-256/bytes:
  `ef9661dbfaad03bcefef4e07707ebc21cc8a13812a7e63bc8e58a408ae8ab42b` /
  7,056,384
- Transcript SHA-256:
  `8a7b8ce9aa03552f92c1717ff7bb4d836b616a8b399cf938951ac21f21e4c66e`
- Server manifest SHA-256:
  `2a74e4467910b3bfd8da268582925f508f1dc7b86285f5496e1a37b5f5ce2bcb`
- Requested/completed/connected: 60,000/60,000/60,000
- Resource pipelines/heap allocations: 2/4
- BSP textures/descriptor DWORDs: 164/3,936
- GPU fences: zero before reuse
- VideoOut tokens: exact; final token `72567767493216`
- Transient slots: both reusable after exact completion
- Persistent allocations reclaimed: 4
- Color/depth guards: intact
- Pad read, present-budget and renderer errors: 0/0/0
- Final readbacks: `a00b1153259458f4` and `5a5954a5f43f9ee8`
- Teardown: gap-free BYE at sequence 1,127, followed by exact-title BigApp
  closure and four healthy payload services
- Device span: approximately 1,001.120 seconds

`tools/validate_bsp_resource_evidence.py` accepted the immutable server
manifest against the exact private bundle identity. Two Remote Play captures
of the transient overlay have SHA-256 values
`41b72ff3eed6f8fee5f5558f2af0735b1b03cfdb9807f4a25dbee5e891ba64a5`
and `07dc2971b8fcfdc2570f03c80612393d110dfc9c23ad63216434fb3ee68bb1c6`;
their map region is byte-identical while the overlay green mean changes by
approximately 30.2 levels. The operator also confirmed the pulse live. The
representative full-frame capture SHA-256 is
`4554e6cc8b1d0a610016bcb96080ac46cc61246de3c46032e80ebe658ec959b6`.
Captures remain private because they include game material.

The gate reused the already proven noclip input path only for
connected/read-error continuity. It did not require another DualSense movement
or Remote Play handoff, and Chiaki used its existing registered console entry.
See `BSP_RESOURCE_FOUNDATION_PHASE2.md` for the exact resource contract.

## Continuous production-runtime evidence

The production lifecycle has no frame limit and is closed by the PS5
**Close Game** action. Run
`20260905T152643467Z_PPSA99997_ps5-agc-gears_0x183d28b1c66c` used that
continuous architecture in the immediately preceding build for 25,560
completed frames. It emitted healthy heartbeats
through frame 25,200 with two frames in flight, exact retired fences/tokens,
intact guards and zero renderer errors. Transcript SHA-256:
`f618b9843799f9c4fe4da1c859694152b8c7e49ab6c554bdb067112210651feb`.

The stream ended at operator closure without BYE, as expected for system-level
termination. It is evidence for the continuous runtime and ownership
heartbeats, but not a strict application-teardown result. A second continuous
session completed 14,160 frames and emitted healthy heartbeats through 10,800;
its transcript SHA-256 is
`40b2a92b647a131f1a116cdf24ecc9ebf69075ccbe02045ca464ab11a02a2914`.

The strict finite runs below validate ordered application teardown, but predate
the continuous production lifecycle. Together these evidence sets cover the
current renderer path and the explicit teardown path without claiming that one
artifact exercised both policies.

Commit `7087602` changes the telemetry accounting and its fully retired-slot
failure path. Its native SELF builds and passes host contracts, but that exact
artifact has not yet received a hardware run. The evidence above therefore
validates the renderer and continuous lifecycle it retains, not binary identity
with current HEAD.

## Strict standalone 10,000-frame soak

- Run: `20260905T143629462Z_PPSA99997_ps5-agc-gears_0x157f6aa15ea6`
- Artifact SHA-256:
  `f69443913b520510d7493bc51a6ae458895612ab3a903a8459ea5a37d84ca175`
- Transcript SHA-256:
  `22d80e7258370185afbca5ba21cd52084840741fd1e0ec3495bce59608ad71f9`
- Requested/completed/verified: 10,000/10,000/10,000
- Maximum frames in flight: 2
- GPU fences: zero before reuse
- VideoOut tokens: exact
- Color/depth guards: intact
- Renderer errors: 0
- Color clear: render-target draw (`color_dma=false`)
- Depth clear: DMA (`depth_dma=true`)
- Opening identity: schema, transport, filesystem policy and boot token exact
- Teardown: complete, with gap-free BYE
- Device span: approximately 166.863 seconds

This run established the corrected strict contract at the original soak size.

## Strict standalone 60,000-frame soak

- Run: `20260905T144120445Z_PPSA99997_ps5-agc-gears_0x15c32a4befaa`
- Artifact SHA-256:
  `431d7753672ae3922dac28b5d685a49d16f09622d53fce3790ab9d9254da8a9e`
- Transcript SHA-256:
  `51954da00511cf81b27f3aa99c2be5b28ca7b3e55622333dfc68964f58d53360`
- Requested/completed/verified: 60,000/60,000/60,000
- Maximum frames in flight: 2
- GPU fences: zero before reuse
- VideoOut tokens: exact
- Color/depth guards: intact
- Renderer errors: 0
- Color clear: render-target draw (`color_dma=false`)
- Depth clear: DMA (`depth_dma=true`)
- Opening identity: schema, transport, filesystem policy and boot token exact
- Teardown: complete, with gap-free BYE at sequence 1,011
- Device span: approximately 1,001.025 seconds

This is the strict finite reference. It is six times longer than the original
10,000-frame gate and used one uninterrupted process, allocation set and
VideoOut lifecycle. It predates the continuous runtime now at HEAD.

## Classified incomplete finite run

Run `20260905T150602724Z_PPSA99997_ps5-agc-gears_0x171c47e22690` requested
60,000 finite frames but its transcript ended after 32,040 completed frames,
without BYE or a terminal error marker. The preceding records report zero
renderer errors and stable timing. It is classified as an unexplained external
termination/truncated transcript—not a completed soak and not evidence of a GPU
fault. Transcript SHA-256:
`44af0e01addf3fa287111059af0137daa91b8e25b5d6a0ddbec9a25f4c2d1736`.

## Historical 10,000-frame soak

- Run: `20260905T140957943Z_PPSA99997_ps5-agc-gears_0x140cddf522be`
- Artifact SHA-256:
  `584780499d5f6b9e63a01df03c30d9a932cd05ea76d52b24936c9ff4f89ce77a`
- Requested/completed/verified: 10,000/10,000/10,000
- Maximum frames in flight: 2
- GPU fences: zero before reuse
- VideoOut tokens: exact
- Color/depth guards: intact
- Renderer errors: 0
- Color clear: render-target draw (`color_dma=false`)
- Depth clear: DMA (`depth_dma=true`)
- Teardown: complete, with clean BYE
- Runtime: approximately 176.864 seconds

This run predates the correction that emitted the mandatory
`LOG_BOOT_MONOTONIC_NS` opening record. Its GPU, ownership and teardown markers
are internally complete, but it is not described as strict telemetry-contract
evidence.

## Strict 300-frame validation

- Run: `20260905T141618842Z_PPSA99997_ps5-agc-gears_0x14658d16f31f`
- Artifact SHA-256:
  `038280d88975f0395037cbe08fc9e82f8560ee2411a44b42249fc7545c5ba5af`
- Requested/completed/verified: 300/300/300
- Maximum frames in flight: 2
- GPU fences: zero before reuse
- VideoOut tokens: exact
- Color/depth guards: intact
- Renderer errors: 0
- Color clear: render-target draw (`color_dma=false`)
- Depth clear: DMA (`depth_dma=true`)
- Opening identity: schema, transport, filesystem policy and boot token exact
- Teardown: VideoOut close, direct-memory release and clean BYE

This shorter run first established the corrected opening contract. It is
retained as regression history; the later strict 10,000-frame run supersedes it.

## Timing interpretation

The historical deadline counter measured a frame from preparation until
retirement. With two frames in flight that interval spans pipeline depth and
therefore reports approximately frames minus one. It is not a dropped-frame
metric. HEAD replaces it with average/maximum intervals between consecutive
retirements and a 17 ms over-budget count. Historical command composition was
about 2.2 microseconds, GPU wait about 1.1 milliseconds and VideoOut wait about
15.56 milliseconds.

## Acceptance rule for later soaks

A later run supersedes the 60,000-frame strict reference only when its artifact hash is
known and its manifest proves matching title/app/boot identity, gap-free
`ps5log/1`, the exact requested frame count, two frames in flight, exact
fences/tokens, intact guards, zero renderer errors and ordered teardown through
BYE. A launch return code, elapsed timeout or visual observation alone is not a
soak result.
