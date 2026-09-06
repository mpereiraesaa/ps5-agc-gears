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

## Gate 4: separate sky pass

The fourth gate is complete on FW 12.02. The baker marks texture name `sky`
with an explicit bundle flag. Sky faces remain in the validated draw table but
are excluded from bounded dynamic-lightmap selection. A platform-neutral plan
partitions the runtime draws into opaque, alpha-tested and sky classes and
selects a deterministic nearest-centroid camera standoff without embedding
private map coordinates in source.

The dedicated `bsp_sky` pipeline samples only the base texture: it is unlit,
blend-disabled and depth-writing. It is compiled separately from
`bsp_resource`; their pixel-shader payload sizes are 204 and 260 bytes. The
native command stream draws opaque and alpha-tested classes first, then switches
to the sky pipeline before the overlay. Frame 9,998 deliberately omits the sky
class while frame 9,999 restores it. Both use the same anisotropic sampler and
paired dynamic-lightmap pattern, so the distinct GPU framebuffer readbacks
isolate the sky pass.

Acceptance requires:

- exactly one sky texture, 158 sky draws and four compiled runtime pipelines;
- skip-control/sky-pass draw counts of 0/158 at frames 9,998/9,999;
- distinct paired GPU framebuffer readbacks with stable lightmap slots;
- exact fence plus VideoOut-token retirement, intact guards, 10,000 completed
  and connected frames, zero errors and six reclaimed allocations;
- a gap-free `bsp-texture-path-sky-soak-complete` BYE.

`validate_texture_path_sky_evidence.py` accepted run
`20260906T165427904Z_PPSA99997_ps5-agc-gears_0x6b9b27deac05`: 10,000 frames,
224 structured records and skip-control/sky-pass framebuffer hashes
`8c9d51a9222a6ce4` / `67cfb3455c938c2e`. The transcript and manifest SHA-256
values are
`30ede3404d9588b3fbf4be74e9787739143255391eeee4b727f4157f7361136c`
and
`d96c761554bc35cb93006e1147b6b04784f392244062c50f4aff296c0814dc88`.
The private bundle is 8,741,888 bytes with SHA-256
`7536b8a28be3b815f93b35f035f9f957952e379722657194e1ab15172f9604e1`.
The native ELF/fSELF SHA-256 values are
`d3669c1d4b1be1c6dc14a55478ba951bf39364fae6bac96ea07db78e53dba670`
and
`037d34a8eb379b8b6f821ceb663427055d1e98fa77882176622ba356f49ab1f6`.

The private Remote Play capture SHA-256 is
`199a6b2c8d1d1c901934db52516989dec797f13e20d809fa79d14bb956033be3`.
The already-proven input path was unchanged; the operator moved the existing
camera so sky-visible geometry was on screen. Chiaki used its existing
registered PS5 entry through the direct stream CLI helper, with no pairing or
re-registration. The title was then closed by exact identity and all four
console services remained healthy.

## Gate 5: exact resident and upload accounting

The fifth gate is complete on FW 12.02. A platform-neutral checked-`uint64_t`
ledger replaces the earlier ad-hoc resident/upload counters. It partitions the
six fence-retired pool allocations and separately accounts for base mip-chain
payload, the source lightmap and both live dynamic-lightmap images without
double-counting them as additional allocations.

Every frame enters the ledger after both CPU-to-GPU flush plans have been
formed and before submit. The ledger rejects skipped or repeated frame numbers,
a changed transient footprint, incorrect full-versus-bounded lightmap uploads,
an inconsistent bounded patch size and integer overflow. An order-sensitive
FNV-64 digest covers the exact frame number and upload components. Four sampled
records are only bookends; the final summary closes all 10,000 ledger entries.

Acceptance requires:

- an exact six-allocation resident partition and three-class texture-payload
  partition bounded by pool capacity;
- exact mip-chain byte identity and positive opaque, alpha and sky draw classes
  using all four pipelines;
- 127,000,000 transient plus 6,671,872 lightmap bytes, totalling 133,671,872;
- exactly two 2,056,192-byte initial lightmap uploads followed by 9,998
  256-byte bounded uploads;
- a gap-free sequence digest matching the final sample, summary and completion
  record;
- distinct GPU-visible alternating-lightmap buffers, stable surrounding bytes,
  intact guards, exact fence/VideoOut retirement, zero errors and six reclaimed
  allocations;
- a gap-free `bsp-texture-path-accounting-soak-complete` BYE.

`validate_texture_path_accounting_evidence.py` accepted run
`20260906T180203834Z_PPSA99997_ps5-agc-gears_0x6f4b7cbd361b`: 10,000 frames,
226 structured records, 68,731,904 pool-resident bytes, 12,251,392 texture
payload bytes, 133,671,872 uploaded bytes and sequence digest
`9ee815de96b54c11`. The transcript and manifest SHA-256 values are
`8f64357391ac9ca00392e6483694f8035d83ceaa72dc9465699f8733b16aa20d`
and
`205d5f418d38ee8c165d79b4c8a658853df714ba310f64846d2a4cf6ff14dfbf`.
The native ELF/fSELF SHA-256 values are
`8bf3622af44ace9f013054119f2e80887c48031df30050106402a9daf1a13308`
and
`eb3d0a18e5abe0e4646386e74f884a6a594051fd81ad870175f562953ece6792`.

The private Remote Play capture SHA-256 is
`525063dbd05d35b7543a60414c3fe9811d52b303d15b4baa7ae24c2c5f0b47b6`.
The direct Chiaki CLI path reused the registered entry without opening the
client window or pairing. Since input code was unchanged, this texture-only
gate explicitly reports `input_dependency=none`; connection remains telemetry
and pad-read errors remain fatal, but no DualSense movement gate is repeated.

## Remaining ordered gates

Gates 1–5 do not claim completion of Phase 3. The remaining order is:

1. the final 60,000-frame structured soak;
2. final public audit, branch publication and pull request.

Entities, PVS, water, sprites/models, platform/audio and engine integration
remain outside this phase.
