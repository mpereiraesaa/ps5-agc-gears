# BSP resource foundation — Phase 2

Phase 2 turns the validated `c1a0` textured viewer into a resource-managed
renderer. It preserves the Phase 1 map, noclip, texture and lightmap paths while
changing how GPU-visible memory, per-frame data, descriptors and pipeline
selection are owned.

## Runtime ownership model

The resource build uses one aligned direct-memory heap for four allocations:

| Allocation | Lifetime | Retirement rule |
| --- | --- | --- |
| BSP bundle and immutable scene data | Whole run | Deferred to the last completed frame token |
| Shader headers, code and pipeline registers | Whole run | Deferred to the last completed frame token |
| D32 depth target and guard | Whole run | Deferred to the last completed frame token |
| Two-slot transient arena | Per frame inside whole-run allocation | A slot is reused only after its exact fence and VideoOut token complete |

`ps5_resource_pool` is a bounds-checked, aligned first-fit suballocator with
generation-tagged handles. Releasing a submitted allocation changes it to
`RETIRING`; reclamation is rejected unless the caller supplies the same nonzero
token and explicit completion proof. The older bump allocator remains only for
the independently reproducible Phase 1 host contract. The Phase 2 native build
does not use it.

`ps5_transient_ring` divides the transient allocation into two independently
owned slots matching the two display buffers. A slot moves through
`EMPTY -> OPEN -> SEALED(token) -> EMPTY`. An open, unsubmitted slot may be
aborted. A sealed slot cannot be reset or reused on a fence alone; it requires
the exact VideoOut token observed by the frame-completion state machine as well.

## Per-frame resource packet

Every frame rebuilds these objects in its open transient slot:

- one 128-byte map constant buffer and one 128-byte clear constant buffer;
- one V# descriptor table for each constant buffer;
- map and clear vertex V# tables;
- all 164 base/lightmap T#/S# tables (3,936 DWORDs for the private reference
  bundle);
- one 128-byte pulsing-color overlay constant buffer and its V# table, plus six
  transient indices; four screen-space positions are generated from
  `gl_VertexIndex`.

The map constants contain a 4x4 camera matrix, a control vector and three debug
vectors. The descriptor is a public-source GFX10.3 raw constant V#; vertex V#,
linear RGBA8 T# and sampler S# construction also lives behind named builders.
Every resource and table is checked against the complete GPU mapping before a
command is emitted.

`bsp_resource.pipe` consumes the constant-buffer table, vertex table and
base/lightmap table. `bsp_overlay.pipe` consumes a separate transient constant
table and draws a small pulsing green quad. `pipeline_permutations.json` is the
source of truth for both pipeline identities and their application SGPR word
counts; generated metadata is rejected when it disagrees with the manifests.
The overlay pipeline explicitly disables depth testing immediately before its
screen-space draw instead of inheriting the map's depth state.

AGC's direct application-user-data offsets are stage-banked. Geometry-stage
tables are written at compact offset `0x8d`; pixel-stage tables remain at
`0x0d`. A host regression counts updates to each bank independently. Hardware
fault isolation also exercised clear-only, map-only and overlay-only command
streams before identifying an earlier `0x0d` geometry write: the corrected
overlay-only artifact completed 2,340 frames with exact retirement and zero
renderer errors. That diagnostic run was deliberately closed externally and
is supporting fault-isolation evidence, not the acceptance soak. A later
visual isolation pass showed that the transient structured-vertex fetch was
zero-filled in this pipeline configuration, so the final overlay uses the
already proven constant-buffer V# path and shader-generated vertices.

## Cache and synchronization contract

CPU-written transient bytes are flushed over an aligned 256-byte range, then
an eight-DWORD `AcquireMem` packet is emitted before any resource draw. The
call uses the firmware-observed raw tuple engine `1`, GCR control `0x00009000`
and poll interval `0x190`. Those values are deliberately not renamed after
public AMD fields: the FW 12.02 builder and an observed scoped-range call prove
their packet encoding, but not a complete semantic name for each PS5 bit.
Submission ends with the existing release-fence packet and exact VideoOut flip
token.

GPU-to-CPU completion is accepted only when both conditions hold:

1. the matching slot fence reads zero;
2. VideoOut reports the exact 48-bit token submitted for that frame.

Only then does the native adapter publish the completed token used to reopen
that transient slot. After the final drain, both slots are reopened and aborted
as an explicit reuse proof. The four persistent allocations are then marked
retiring and reclaimed against the last completed token.

## Build and host gates

The private resource artifact is built with:

```sh
PS5LOG_DEV_CONF=/private/path/dev.conf \
make bsp-resource-native-release \
  BSP_INPUT=/private/path/c1a0.bsp \
  AMDLLPC=/path/to/amdllpc \
  LLVM_READELF=/path/to/llvm-readelf
```

`BSP_RESOURCE_FOUNDATION=1` fails closed unless both the textured and noclip
paths are enabled. `make test` covers allocation fragmentation, stale handles,
premature reclamation, ring exhaustion/token mismatch, descriptor words,
constant/table layouts, transient overlay composition, cache-range alignment,
AcquireMem size and generated pipeline metadata. `make audit` keeps all private
maps, bundles, logs, configuration and generated binaries outside the public
boundary.

## Hardware acceptance

The 60,000-frame run is unattended. Phase 1 already proves physical DualSense
movement, so Phase 2 requires a connected sample on every frame and zero pad
read errors but does not repeat movement or Remote Play handoff. Chiaki remains
on its existing registered console entry and is not paired or reinitialized.

The run is accepted only if `validate_bsp_resource_evidence.py` proves:

- exact title, application, boot and private bundle identity;
- gap-free structured `ps5log/1` with no raw, oversized or `ERROR` records;
- two resource pipelines and four heap allocations;
- per-frame constant, texture-table and overlay cardinality at frames 0 and
  59,999, with complete GPU spans and the observed AcquireMem tuple;
- matching seal, zero-fence retirement and exact VideoOut token bookends;
- matching submitted-frame token and slot bookends between seal and retirement;
- 60,000 completed and connected frames with no renderer, pad or present-budget
  errors;
- visible pixels, intact guards, both transient slots reusable and all four
  persistent allocations reclaimed;
- a clean `bsp-resource-soak-complete` BYE.

Run
`20260906T130036578Z_PPSA99997_ps5-agc-gears_0x5ed84765862b` satisfies this
contract on FW 12.02. Its 1,127-record manifest proves 60,000 completed and
connected frames, 164 textures, 3,936 descriptor DWORDs, two reusable
transient slots, exact final token `72567767493216`, intact guards, zero errors,
four reclaimed persistent allocations and a gap-free
`bsp-resource-soak-complete` BYE. The transcript SHA-256 is
`8a7b8ce9aa03552f92c1717ff7bb4d836b616a8b399cf938951ac21f21e4c66e`.

Two private Remote Play captures independently show the green overlay at
different pulse intensities while the map region remains byte-identical. The
operator confirmed the animation live. Exact artifact, bundle, manifest and
capture hashes are recorded in `HARDWARE_VALIDATION.md`; no private map,
compiled binary, telemetry transcript or game capture crosses the repository
boundary.
