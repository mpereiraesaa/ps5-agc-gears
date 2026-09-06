# Backend extraction provenance

This repository is a clean publication boundary. It contains no copied Sony
headers, runtime binaries, game material, captured command buffers or shader
blobs. Firmware-specific facts are represented as narrow constants and are
covered by synthetic host tests.

## Extracted platform-neutral contracts

| Public unit | Purpose | Evidence boundary |
| --- | --- | --- |
| `ps5_surface` | Plans two 1080p/2160p tiled display buffers inside one aligned 128 MiB allocation. | Constants were sanitized from the FW 12.02 laboratory integration; footprint and non-overlap are host-tested and the native backend allocates/registers the resulting surfaces. |
| `ps5_present` | Invokes an injected SetFlip builder, validates its cursor, then appends the terminal eight-DWORD release-fence packet. | Exact ordering and cursor bounds are host-tested with a synthetic builder. The public unit contains no NID or loader code. |
| `ps5_frame_completion` | Retains every submitted resource until both fence zero and the matching VideoOut token are observed. | Every success and fail-closed transition is host-tested. Event polling remains outside this pure state machine. |
| `include/ps5_agc*.h` | Declares only the AGC/driver functions currently required by the demo. | Calling conventions and arguments are sanitized FW 12.02 contracts exercised by the standalone artifact; project-owned link stubs are included. |
| `ps5_color_target` | Selects the unique MRT0 runtime-default block and derives a bound color-target descriptor. | Firmware key, compact offsets, masks and alignment are explicit, checked at runtime and covered by synthetic defaults. |
| `ps5_pipeline` | Combines MRT0, viewport, linked shaders and compiler-produced stage registers into the exact 84 CX / 12 SH / 3 UC plan. | Deterministic fixture hash and every concatenation boundary are host-tested. |
| `ps5_depth_target` | Builds the uncompressed D32, no-HTILE target and depth-test state. | Alignment, dimensions and the 22 sanitized public-register pairs are host-tested; the private hardware integration previously exercised the equivalent plan. |
| `ps5_event_adapter` | Converts injected equeue wait/decode calls into exact completion observations. | Host fixtures cover empty polls, matching tokens and fail-closed malformed/decode-error paths; native symbol binding stays outside this unit. |
| `ps5_gpu_span` | Proves a complete non-empty range lies inside a declared GPU-visible mapping. | Boundary and integer-overflow cases are host-tested. |
| `ps5_submission` | Initializes the terminal fence, composes presentation and invokes an injected submit callback. | Tests prove ordering, command length, no-submit on compose failure and resource retention across every irreversible failure. `render_resources_ready` deliberately replaces the obsolete assumption that the color target must be DMA-prefilled. |
| `shaders/gears_lit.pipe` | Independently authored GLSL and LLPC pipeline/resource mapping for lit gears and the fullscreen clear. | A host contract test pins the 24 direct parameter DWORDs, one indirect vertex-table pointer, 24-byte interleaved vertex layout, triangle-list topology and BGRA8 target. No compiled PAL ELF or ISA blob is committed. |
| `tools/build_shader.py` | Compiles the project source with public LLPC for `gfx1013`, rejects relocations, extracts exact PAL symbol extents and hashes every result. | Unit-tested parser and source invariants plus two consecutive real LLPC builds with identical ELF and stage hashes. Generated files remain ignored build products. |
| `tools/generate_agc_metadata.py` | Safely decodes the project PAL notes and derives shader resource words, draw modifier and 10+9 CX templates. | The standalone result matches all ten defines and all nineteen register pairs used by the hardware-proven integration; invalid target/pipeline/relocation contracts fail closed. |
| `native/stubs/libSceAgc*.c` | Provides link-only facades for the AGC imports declared by this project. | Both files compile with the pinned Prospero toolchain and link into x86-64 shared import modules with the required public symbols. Runtime implementations remain firmware-owned. |
| `ps5_direct_memory` | Owns reserve/allocate/map and ordered unmap/release through injected platform calls. | Host tests cover valid lifecycle, map rollback, invalid alignment and refusal to release resources without explicit GPU cleanup authority. |
| `include/ps5_platform.h` | Declares the minimal kernel, VideoOut and sysmodule surface and its opaque record layouts. | Compile-time sizes/offsets match the FW 12.02 hardware-used integration; it is explicitly a project-owned compatibility header. |
| `native/ps5log` | Streams bounded structured `ps5log/1` records using `/app0/dev.conf` and a native `sceNet` adapter. | Vendored source matches the project-owned logging client byte-for-byte, its full host suite passes, and both native objects compile with only the expected libc/`sceNet` imports unresolved. |
| `ps5_shader_header` | Builds the self-relative FW 12.02 input arena consumed by `sceAgcCreateShader` from generated public metadata. | Exact header/user-data/special/CX/SH offsets, GS/PS resource words and invalid inputs are host-tested; native compilation succeeds. |
| `native/shader_assets.S` | Embeds only the two stages produced from `shaders/gears_lit.pipe`. | Prospero assembly produces 384-byte GS and 160-byte PS spans at 256-byte alignment using repository-local build paths. |
| `ps5_agc_writer` | Adapts AGC's mutable command-buffer writer and driver wait cursor to checked project-owned calls. | Host tests cover exact 27+3 DWORD packets, capacity/cursor corruption, GPU-span rejection, depth-only DMA naming and transactional wait errors. Native symbol binding remains a thin separate layer. |
| `ps5_agc_submit` and `native/ps5_agc_native` | Validate, flush and submit a GPU-visible DCB while binding the checked writer calls to the firmware builders. | Host tests inspect the exact 0x10-byte descriptor and reject out-of-range commands; all units compile with Clang 18 for `x86_64-sie-ps5` with only the declared AGC symbols unresolved. |
| `ps5_videoout` | Owns VideoOut open, equeue/event setup, flip rate, buffer registration and ordered teardown. | Host fault injection covers every acquisition boundary and the FW 12.02 `RESOURCE_BUSY` unregister behavior; standalone hardware runs completed through VideoOut close and memory release. |
| `native/main.c` | Joins the extracted units into the complete two-buffer renderer and native lifecycle. | The exact standalone build completed 300-frame strict validation and a 10,000-frame hardware soak on FW 12.02. |
| `tools/build_native.sh` | Fetches/verifies the pinned public foundation, builds `gfx1013` shaders and metadata, compiles, links, signs and packages `PPSA99997`. | Its output is ignored, inspected after signing and identified by SHA-256 in every accepted hardware run. |
| `bsp_texture_descriptor` | Emits mip-aware linear RGBA8 image and sampler SRDs plus one base/lightmap table per BSP texture. | Exact GFX10.3 words, 48-bit address/alignment limits, smallest-to-base padded mip layout, repeat/clamp modes, trilinear/anisotropic filters, full-table sizing and forged-view failures are host-tested against public Mesa/PAL contracts. |
| `ps5_resource_pool` | Suballocates one direct-memory heap with aligned first-fit placement and generation-tagged handles. | Host tests cover fragmentation, stale handles, unsubmitted release and refusal to reclaim submitted resources without exact completion proof. |
| `ps5_transient_ring` and `ps5_transient_table` | Own two per-frame arenas and build GPU-visible descriptor tables inside the currently open slot. | Host tests cover alignment, exhaustion, abort-before-submit, sealed-token mismatch and exact-token reuse. |
| `ps5_gfx1013_descriptor` | Builds named V#, mip-aware T# and bilinear/trilinear/anisotropic S# records for structured vertices, raw constants and BSP images. | Exact DWORD tests pin the independently authored subset derived from Mesa/PAL's public GFX10.3 definitions. |
| `ps5_cache_contract` | Plans an aligned CPU flush and scoped AcquireMem before GPU reads, then requires fence plus exact VideoOut token before CPU reuse. | Host tests pin the 256-byte range, FW-observed raw engine/GCR/poll tuple, operation order and refusal of fence-only completion; raw PS5 bits are not assigned speculative public AMD names. |
| `bsp_resource_frame` and `bsp_resource_draw` | Build per-frame constants, descriptors and overlay geometry, then compose the map and overlay pipeline permutations. | Synthetic GPU mappings verify all spans, exact draw counts, packet lengths, distinct GS `0x8d`/PS `0x0d` user-data banks and no-write-on-invalid-input behavior; an isolated overlay hardware run completed 2,340 frames after the bank correction. |
| `shaders/bsp_resource.pipe`, `shaders/bsp_overlay.pipe` and generated permutation metadata | Define the constant-buffer textured map and transient visible overlay pipelines. | Source contracts pin resource mappings, constant layouts and shader-generated overlay vertices; LLPC manifests and generated application-word counts are validated together. |

These units replace laboratory-prefixed prototypes with project-owned names
and interfaces. Their behavior was exercised by the private native integration
before extraction, but the files here are independently reviewable and do not
depend on the parent laboratory.

## Hardware evidence versus repository evidence

The 300-frame, 10,000-frame and Phase 2 60,000-frame FW 12.02 runs cited in the
README used title `PPSA99997` built entirely by this repository's native build
path and the pinned public foundation. The strict 300-frame run proves the
complete opening telemetry identity, exact frame ownership and clean teardown
contract. The earlier 10,000-frame run proves prolonged GPU/ownership behavior
but predates the final `LOG_BOOT_MONOTONIC_NS` opening-record correction; this
limitation is preserved rather than silently reclassifying that transcript.

The Phase 2 run
`20260906T130036578Z_PPSA99997_ps5-agc-gears_0x5ed84765862b` proves the
resource-pool, per-frame transient, descriptor-table, constant-buffer,
pipeline-permutation and cache-transition path for 60,000 frames. Its public
claim is limited to sanitized identities, hashes and outcomes. The private
`c1a0` BSP, compiled artifact, transcript and Remote Play captures remain
outside this repository. Exact hashes and validator results are listed in
`HARDWARE_VALIDATION.md`.

## Rules for later extraction

- Rewrite public interfaces around the smallest required contract.
- Preserve fail-closed ownership after any irreversible submit/present step.
- Use only independently authored shader source and public compiler tooling.
- Add a synthetic regression for each constant, layout and cursor advance.
- Record hardware validation separately from host-only evidence.
- Run `make audit` before any commit, archive or publication.

## Native foundation pin

Native packaging uses the public boilerplate fork at commit
`37dd53602bdead63936f718004555ba10154be48`. This is the hardware-used tree:
upstream `722f2227a8bb6fa2229120546995b6562552c752` plus the public RELRO
load-segment congruence correction. A build script must verify this exact
revision or fetch it into a project-local ignored dependency directory; it does
not silently consume whichever checkout happens to exist in a parent lab.
