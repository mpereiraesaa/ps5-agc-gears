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

## Gate 2: deterministic mip chains and sampler variants

The second gate is complete on FW 12.02. Bundle ABI version 3 stores a complete
RGBA8 mip chain for every base texture. The baker derives every level from the
decoded level-zero image with an integer 2x2 box filter and round-to-nearest,
stopping at 1x1. It follows Mesa AddrLib's GFX10 linear layout: levels are
packed from smallest to base, each row is padded to 256 bytes, and the image
base remains 256-byte aligned. The C consumer recomputes and validates every
level's dimensions, pitch, offset and aggregate span before exposing it.

The GFX10.3 T# builder now emits `BASE_LEVEL`, `LAST_LEVEL` and `MAX_MIP` from
the validated chain. Separate S# variants encode trilinear filtering and 4:1
anisotropic filtering; exact DWORDs and invalid combinations are host-tested
against Mesa and PAL's public register definitions. The hardware gate uses
trilinear on even frames and anisotropic 4:1 on odd frames. Its final two
frames deliberately share the same dynamic-lightmap pattern, so their distinct
GPU framebuffer hashes isolate the sampler change rather than the lightmap.

Acceptance requires all of the following:

- deterministic ABI-v3 output across two independent private-map bakes;
- 164 validated chains, 5–9 levels, 7,842,816 bytes and smallest-to-base order;
- exact sampled S# words for frames 0, 1, 9,998 and 9,999;
- identical final lightmap-slot hashes with paired pattern 1, but distinct
  trilinear and anisotropic GPU framebuffer readbacks;
- exact fence plus VideoOut-token retirement, intact guards, 10,000 completed
  and connected frames, zero errors and six reclaimed allocations;
- a gap-free `bsp-texture-path-mip-soak-complete` BYE.

`validate_texture_path_mip_evidence.py` accepted run
`20260906T153443296Z_PPSA99997_ps5-agc-gears_0x67412ae3fe5e`: 10,000 frames,
224 structured records, trilinear/anisotropic framebuffer hashes
`53961843c05e93bf` / `404a458403011f6d`, 76,383,232 resident bytes and
173,632,448 uploaded bytes. The transcript and manifest SHA-256 values are
`6d06e1fbd22cb2ad91e0f2e4904ae752675f281dc8387f97da4f1d772ba965db`
and
`8daa88b924288d72b454274e37c0efe569157870826f20ab2c718a0b40456044`.
The private bundle is 10,121,728 bytes with SHA-256
`05a2f8ecc0b21df1e0ad3f5a159f7f20ae847c8ef252245959561c42f6e3fe52`.
The native ELF/fSELF SHA-256 values are
`bec19b3e50f762e86713cf386238d697e52389a371cc69418a66e7c78ed7b50f`
and
`7adfb55bfb287867f9d5d419262b9d1144330360867ab20d571b46344f0ea7ff`.

The private Remote Play capture SHA-256 is
`e15f89725691d8fcc02e0482f75d5b1d596b7b6e7c603865c7d832499d35125b`.
It was taken from the already-open registered Chiaki session after the soak;
no launch, pairing or controller movement gate was repeated. The exact title
was then closed and all four console services passed two health checks.

## Gate 3: `{` alpha-test permutation

The third gate is complete on FW 12.02. Transparent GoldSrc textures are no
longer handled by a discard in the general BSP shader. Bundle inspection and a
platform-neutral planner classify texture names beginning with `{`, count their
draws and select the nearest alpha-tested face centroid from the initial camera.
The private reference bundle contains one such texture and 42 alpha-tested
draws; the other 3,569 map draws remain opaque.

`bsp_resource` is now strictly opaque. The separate `bsp_alpha_test` pipeline
uses the same base-times-lightmap resource ABI, depth test and depth writes, but
discards base texels below alpha 0.5. Generated metadata proves the compiled
pipelines differ in `DB_SHADER_CONTROL` only by the pixel-kill bit: opaque
`0x00000810`, alpha-test `0x00000850`. The native command stream first submits
the opaque draw class, switches pipeline, then submits the alpha-tested class;
the overlay remains the final third permutation.

The last two frames share the same camera, anisotropic sampler and paired
dynamic-lightmap pattern. Frame 9,998 deliberately draws the alpha class with
the opaque control pipeline, while frame 9,999 uses the alpha-test pipeline.
Their distinct GPU framebuffer readbacks therefore isolate the alpha-test
permutation. Acceptance requires:

- exactly one `{` texture, 42 alpha-tested draws and 3,569 opaque draws;
- a three-entry runtime permutation table and the exact compiled kill-bit delta;
- distinct opaque-control/alpha-test GPU readbacks with identical lightmap slots;
- exact fence plus VideoOut-token retirement, intact guards, 10,000 completed
  and connected frames, zero errors and six reclaimed allocations;
- a gap-free `bsp-texture-path-alpha-soak-complete` BYE.

`validate_texture_path_alpha_evidence.py` accepted run
`20260906T160249452Z_PPSA99997_ps5-agc-gears_0x68c9c058f710`: 10,000 frames,
224 structured records and opaque-control/alpha-test framebuffer hashes
`716cc2cc82d8015a` / `221c46ffdbcf3e1c`. The transcript and manifest SHA-256
values are
`1ab9e23155eb53abf75994401f9a130ad0b41c141f448f65907c76ebbe3408ce`
and
`b8080f7a2e3b68fde8b4b4fb0e88460f7a235c13caf0b13d143163c1c142b665`.
The native ELF/fSELF SHA-256 values are
`129f0dbb7074849b3f7fd661e3d8ce2ddfc5caeaa19dbe7718c23c6c49b6c04e`
and
`57b3a39fde9b57bb96ec97ed9af5ee0fee2b706ae58c6a79eb112f412c2f1b16`.

The private Remote Play capture SHA-256 is
`6c050673f69e58f1113dcbf99ecd9cf6aef3dec6c881a285dc396cb1b6f147f6`.
It shows the alpha-tested grate/door with the scene visible through the cutout,
instead of the earlier opaque repeated placeholder. It was taken from the
already-open registered Chiaki session; Chiaki was neither launched nor paired.
The exact title was then closed and all four console services passed two stable
health checks.

## Remaining ordered gates

Gates 1–3 do not claim completion of Phase 3. The remaining order is:

1. a separate sky pass;
2. consolidated resident/upload telemetry and reproducible host/hardware gates;
3. the final 60,000-frame structured soak and pull request.

Entities, PVS, water, sprites/models, platform/audio and engine integration
remain outside this phase.
