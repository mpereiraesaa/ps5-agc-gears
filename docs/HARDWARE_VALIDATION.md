# Hardware validation

Hardware claims in this repository refer to one PS5 running firmware 12.02.
They are not compatibility claims for other firmware or consoles. Run logs and
captures live in the private parent laboratory; this public boundary records
only sanitized identifiers, hashes, outcomes and known limitations.

## Phase 3 resident/upload-accounting gate

The fifth ordered Phase 3 gate passed 10,000 frames on FW 12.02:

- Run: `20260906T180203834Z_PPSA99997_ps5-agc-gears_0x6f4b7cbd361b`
- Native ELF SHA-256:
  `8bf3622af44ace9f013054119f2e80887c48031df30050106402a9daf1a13308`
- Signed fSELF SHA-256:
  `eb3d0a18e5abe0e4646386e74f884a6a594051fd81ad870175f562953ece6792`
- Private bundle SHA-256/bytes:
  `7536b8a28be3b815f93b35f035f9f957952e379722657194e1ab15172f9604e1` /
  8,741,888
- Transcript/manifest SHA-256:
  `8f64357391ac9ca00392e6483694f8035d83ceaa72dc9465699f8733b16aa20d` /
  `205d5f418d38ee8c165d79b4c8a658853df714ba310f64846d2a4cf6ff14dfbf`
- Requested/completed: 10,000/10,000; controller dependency: none
- Pool-resident/texture-payload bytes: 68,731,904 / 12,251,392
- Total transient/lightmap/upload bytes:
  127,000,000 / 6,671,872 / 133,671,872
- Upload frames: 2 full plus 9,998 bounded; sequence digest:
  `9ee815de96b54c11`
- Mip chains and opaque/alpha/sky draws: 122 and 2,915/137/158
- Final alternating-lightmap GPU readbacks:
  `068f5c03c04419eb` / `6a7e900866458fb0` (distinct)
- Fence and VideoOut tokens: exact; guards intact; renderer errors: 0
- Teardown: gap-free BYE at sequence 226, six allocations reclaimed,
  exact-title closure and two stable healthy four-service checks

The strict validator recomputed every byte total, linked the terminal resource
readbacks to the two deterministic lightmap patterns and accepted the immutable
manifest against the exact bundle identity. A private 1280x720 direct-stream
capture shows the complete textured scene and overlay; its SHA-256 is
`525063dbd05d35b7543a60414c3fe9811d52b303d15b4baa7ae24c2c5f0b47b6`.
Chiaki used the registered console directly from its CLI helper, without its
client window, pairing or re-registration.

An earlier attempt reached all 10,000 clean frames but was correctly rejected
with `parked-retain`: an inherited success condition required a connected
controller even though input code was unchanged. The accepted artifact makes
this boundary explicit with `input_gate=not-repeated` and
`input_dependency=none`; pad-read failures are still fatal and connection
counts remain observable. This gate proves consolidated texture accounting,
not the pending final 60,000-frame closure.

## Phase 3 sky-pass gate

The fourth ordered Phase 3 gate passed 10,000 frames on FW 12.02:

- Run: `20260906T165427904Z_PPSA99997_ps5-agc-gears_0x6b9b27deac05`
- Native ELF SHA-256:
  `d3669c1d4b1be1c6dc14a55478ba951bf39364fae6bac96ea07db78e53dba670`
- Signed fSELF SHA-256:
  `037d34a8eb379b8b6f821ceb663427055d1e98fa77882176622ba356f49ab1f6`
- Private bundle SHA-256/bytes:
  `7536b8a28be3b815f93b35f035f9f957952e379722657194e1ab15172f9604e1` /
  8,741,888
- Transcript/manifest SHA-256:
  `30ede3404d9588b3fbf4be74e9787739143255391eeee4b727f4157f7361136c` /
  `d96c761554bc35cb93006e1147b6b04784f392244062c50f4aff296c0814dc88`
- Requested/completed/connected: 10,000/10,000/10,000
- Sky textures/draws: 1 / 158; compiled runtime pipelines: 4
- Final skip-control/sky-pass GPU readbacks:
  `8c9d51a9222a6ce4` / `67cfb3455c938c2e` (distinct)
- Paired final lightmap slots: equal, pattern 1
- Fence and VideoOut tokens: exact; guards intact; renderer errors: 0
- Teardown: gap-free BYE at sequence 224, six allocations reclaimed,
  exact-title closure and healthy four-service state

The strict validator accepted the immutable manifest and exact private bundle
identity. A private capture shows textured geometry beneath the dedicated sky
pass; its SHA-256 is
`199a6b2c8d1d1c901934db52516989dec797f13e20d809fa79d14bb956033be3`.
The input implementation was unchanged; the operator moved the existing camera
to a sky-visible view. Chiaki reused the registered console through the direct
CLI stream path without pairing or re-registration. This gate proves the
separate sky draw class and pass ordering, not the pending consolidated
accounting or final 60,000-frame gates.

## Phase 3 alpha-test gate

The third ordered Phase 3 gate passed 10,000 frames on FW 12.02:

- Run: `20260906T160249452Z_PPSA99997_ps5-agc-gears_0x68c9c058f710`
- Native ELF SHA-256:
  `129f0dbb7074849b3f7fd661e3d8ce2ddfc5caeaa19dbe7718c23c6c49b6c04e`
- Signed fSELF SHA-256:
  `57b3a39fde9b57bb96ec97ed9af5ee0fee2b706ae58c6a79eb112f412c2f1b16`
- Private bundle SHA-256/bytes:
  `05a2f8ecc0b21df1e0ad3f5a159f7f20ae847c8ef252245959561c42f6e3fe52` /
  10,121,728
- Transcript/manifest SHA-256:
  `1ab9e23155eb53abf75994401f9a130ad0b41c141f448f65907c76ebbe3408ce` /
  `b8080f7a2e3b68fde8b4b4fb0e88460f7a235c13caf0b13d143163c1c142b665`
- Requested/completed/connected: 10,000/10,000/10,000
- Alpha textures/draws and opaque draws: 1 / 42 / 3,569
- Compiled opaque/alpha `DB_SHADER_CONTROL`: `0x00000810` / `0x00000850`
- Final opaque-control/alpha-test GPU readbacks:
  `716cc2cc82d8015a` / `221c46ffdbcf3e1c` (distinct)
- Paired final lightmap slots: equal, pattern 1
- Resident/total uploaded bytes: 76,383,232 / 173,632,448
- Fence and VideoOut tokens: exact; guards intact; renderer errors: 0
- Teardown: gap-free BYE at sequence 224, six allocations reclaimed,
  exact-title closure and two stable healthy four-service checks

The strict validator accepted the immutable manifest and exact private bundle
identity. A private capture from the already-running registered Chiaki session
shows the cutout surface with geometry behind it; its SHA-256 is
`6c050673f69e58f1113dcbf99ecd9cf6aef3dec6c881a285dc396cb1b6f147f6`.
Neither Chiaki nor the DualSense movement gate was restarted. This gate proves
the separate `{` alpha-test draw class and compiled shader permutation, not the
pending sky or final 60,000-frame gates.

## Phase 3 mip/sampler gate

The second ordered Phase 3 gate passed 10,000 frames on FW 12.02:

- Run: `20260906T153443296Z_PPSA99997_ps5-agc-gears_0x67412ae3fe5e`
- Native ELF SHA-256:
  `bec19b3e50f762e86713cf386238d697e52389a371cc69418a66e7c78ed7b50f`
- Signed fSELF SHA-256:
  `7adfb55bfb287867f9d5d419262b9d1144330360867ab20d571b46344f0ea7ff`
- Private bundle SHA-256/bytes:
  `05a2f8ecc0b21df1e0ad3f5a159f7f20ae847c8ef252245959561c42f6e3fe52` /
  10,121,728
- Transcript/manifest SHA-256:
  `6d06e1fbd22cb2ad91e0f2e4904ae752675f281dc8387f97da4f1d772ba965db` /
  `8daa88b924288d72b454274e37c0efe569157870826f20ab2c718a0b40456044`
- Requested/completed/connected: 10,000/10,000/10,000
- Texture chains/levels/bytes: 164 / 5–9 / 7,842,816
- Layout/filter pair: smallest-to-base linear / trilinear and anisotropic 4:1
- Paired final lightmap slots: equal, pattern 1
- Final trilinear/anisotropic GPU readbacks:
  `53961843c05e93bf` / `404a458403011f6d` (distinct)
- Resident/total uploaded bytes: 76,383,232 / 173,632,448
- Fence and VideoOut tokens: exact; guards intact; renderer errors: 0
- Teardown: gap-free BYE at sequence 224, six allocations reclaimed,
  exact-title closure and two healthy four-service checks

The strict validator accepted the immutable manifest and exact private bundle
identity. A capture from the already-running registered Chiaki session shows a
correct textured corridor; its SHA-256 is
`e15f89725691d8fcc02e0482f75d5b1d596b7b6e7c603865c7d832499d35125b`.
The capture remained private and neither Chiaki nor the DualSense input gate was
restarted. This gate proves deterministic mips and the two sampler variants,
not the pending alpha-test, sky or final 60,000-frame gates.

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
