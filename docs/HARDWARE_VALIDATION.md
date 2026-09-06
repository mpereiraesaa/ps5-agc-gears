# Hardware validation

Hardware claims in this repository refer to one PS5 running firmware 12.02.
They are not compatibility claims for other firmware or consoles. Run logs and
captures live in the private parent laboratory; this public boundary records
only sanitized identifiers, hashes, outcomes and known limitations.

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
