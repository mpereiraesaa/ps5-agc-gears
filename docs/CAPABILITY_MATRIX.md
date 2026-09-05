# AGC capability and import matrix

This document separates missing public imports from missing renderer contracts.
Most remaining work does not require discovering a new NID.

## Hardware-proven core

| Area | Imports already exercised on FW 12.02 |
| --- | --- |
| AGC lifecycle | `sceAgcInit`, `sceAgcGetRegisterDefaults`, `sceAgcCreateShader`, `sceAgcLinkShaders` |
| Command construction | `sceAgcDcbSetCxRegistersIndirect`, `sceAgcDcbSetUcRegistersIndirect`, `sceAgcDcbSetShRegistersIndirect`, `sceAgcDcbDrawIndexAuto`, `sceAgcDcbSetFlip` |
| Submission/ownership | `sceAgcDriverGetWaitRenderingPacketSizeInDwords`, `sceAgcDriverWaitUntilSafeForRendering`, `sceAgcDriverSubmitDcb` |
| VideoOut | open, buffer attributes/registration, flip event/data, unregister and close |
| Memory/events | Main Direct Memory, reserve, map/BatchMap, unmap/release and equeue primitives |

## Capability progression

| Stage | New import required? | Missing contract |
| --- | --- | --- |
| Animated triangle | No | Per-frame command reset, monotonic flip IDs and double-buffer ownership |
| Plasma | No | Time/resolution user data and a stable frame loop |
| Procedural cube | Probably no | Matrix user data, perspective, culling and optionally depth |
| Non-indexed gear | No known new import | One interleaved vertex-buffer SRD and table-pointer binding |
| Indexed gear | Possibly | A validated indexed-draw builder; avoid it initially by expanding indices |
| Correct overlapping gears | No known new import | Depth allocation, layout, descriptor/register block, clear and barriers |
| Textured extensions | Useful new builder likely | Texture/sampler descriptors, resource residency and cache transitions |

## Candidate optional imports

- `sceAgcCbSetShRegisterRangeDirect`: public-reference code uses it for resource
  descriptors. It is convenient for dynamic uniforms/buffers but not required
  for Plasma because the existing SH-indirect path can update user registers.
- `sceAgcSuspendPoint`: present in a public reference after submit. It is not
  required by our proven fence/event completion path and should remain optional.
- `sceAgcDcbDrawIndex` (`q88lQ+GP5Yk`), or the bound-index trio
  `sceAgcDcbSetIndexBuffer` (`l4fM9K-Lyks`),
  `sceAgcDcbSetIndexCount` (`8N2tmT3jmC8`) and
  `sceAgcDcbDrawIndexOffset` (`B+aG9DUnTKA`): useful for compact gear meshes,
  but not a gate; a
  first implementation can expand the index list and retain
  `sceAgcDcbDrawIndexAuto`.
- `sceVideoOutGetFlipStatus` (`SbU3dwp80lQ`) and
  `sceVideoOutWaitVblank` (`j6RaAUlaLv0`): potentially useful for
  telemetry/pacing, but the existing exact flip event is sufficient for
  ownership correctness.
- `sceAgcDcbAcquireMem` (`57labkp+rSQ`), `sceAgcCbReleaseMem`
  (`wr23dPKyWc0`), `sceAgcDcbEventWrite` (`aJf+j5yntiU`),
  `sceAgcDcbWriteData` (`i1jyy49AjXU`) and `sceAgcDcbWaitRegMem`
  (`VmW0Tdpy420`) are the public synchronization candidates for later depth
  clears/transitions. They are not interchangeable: event/ACQUIRE controls
  cache or pipeline visibility, while the terminal release label proves
  completion and ownership.

## Reverse-engineering priorities

1. **No Ghidra work for Plasma.** Use the already proven imports and derive the
   time uniform from public compiler metadata.
2. Validate how LLPC maps one small constant/user-data block into SH registers;
   prefer compiler metadata and host disassembly before firmware analysis.
3. For buffered geometry, recover only the descriptor words and user-SGPR
   binding contract needed by our own shader. Cross-check public PAL/LLVM and
   `sceAgcCbSetShRegisterRangeDirect` callers.
4. Investigate depth only after Cube renders without it: defaults selector,
   `DB_Z_*`/`DB_DEPTH_*` block, layout/alignment and safe clear/barrier sequence.
5. Resolve an indexed-draw API only after non-indexed Gears works.

Firmware analysis should record symbol/NID, module hash, calling convention,
arguments, cursor advance, side effects and confidence. No proprietary bytes or
substantial decompilation enter this repository.

## Compiler evidence — 2026-09-05

Clean-room Plasma and Cube microshaders were compiled offline with public LLPC
for `gfx1013`; both produced relocation-free PAL ELFs with AMDGPU flags `0x42`.

- Plasma's four floats (`time`, inverse width/height and phase) map directly to
  pixel-stage user data 0–3. Including the internal table pointer, PAL reports
  five PS user SGPRs. No buffer descriptor or new import is needed.
- Cube's 4×4 MVP maps directly to 16 pre-raster user-data words. Interleaved
  position and normal inputs cause PAL to request the compiler-defined vertex
  buffer table pointer (`0x1000000f`); the stage reports 20 user SGPRs.

Public GFX10 PAL source and the resulting `gfx1013` ISA close the vertex side
further:

- both attributes share binding zero and one interleaved 24-byte vertex;
- the shader loads exactly one 16-byte SRD, then fetches position and normal
  using formats encoded in the shader instructions;
- the SRD words are address-low, address-high plus stride, record count, and
  the public GFX10 control word `0x11014fac`;
- the SRD table is 16-byte aligned and its 32-bit GPU pointer occupies the
  compiler-selected `VertexBufferTable` user register.

Thus non-indexed Cube/Gears needs neither two descriptors nor a general uniform
subsystem. The remaining hardware contract on the critical path is depth;
indexed drawing and explicit VideoOut pacing remain optional refinements.

## Public depth register map

The 16-entry depth-target block has now been cross-checked exhaustively against
AMD's public GFX10 PAL register table. Each compact ID is exactly the register's
context-space offset from `0xa000`:

| ID | Public GFX10 register | ID | Public GFX10 register |
|---:|---|---:|---|
| `02` | `DB_DEPTH_VIEW` | `10` | `DB_Z_INFO` |
| `05` | `DB_HTILE_DATA_BASE` | `11` | `DB_STENCIL_INFO` |
| `07` | `DB_DEPTH_SIZE_XY` | `12` | `DB_Z_READ_BASE` |
| `0a` | `DB_STENCIL_CLEAR` | `13` | `DB_STENCIL_READ_BASE` |
| `0b` | `DB_DEPTH_CLEAR` | `14` | `DB_Z_WRITE_BASE` |
| `1a` | `DB_Z_READ_BASE_HI` | `15` | `DB_STENCIL_WRITE_BASE` |
| `1b` | `DB_STENCIL_READ_BASE_HI` | `1c` | `DB_Z_WRITE_BASE_HI` |
| `1d` | `DB_STENCIL_WRITE_BASE_HI` | `1e` | `DB_HTILE_DATA_BASE_HI` |

This identifies all five split 48-bit addresses, the two clear values, surface
dimensions and format/control words. It does not by itself prove that writing
this block initializes all depth state: public PAL also manages render-control,
override, cache-policy, polygon-offset and HTILE-surface registers around the
view. The first implementation will therefore use the uncompressed/no-HTILE
path only after its allocation/swizzle and surrounding defaults are validated.

Public Mesa now accepts GFX1013 external revisions `0x82..0x85` and routes
them through generic GFX10 AddrLib. This proves the fixed depth swizzle family
is `ADDR_SW_64KB_Z_X`: GFX1013 does not enable RB+ there, so the variable-block
depth mode is unavailable. It does **not** prove a complete byte layout.
AddrLib also requires the device's real `GB_ADDR_CONFIG` (pipe count,
interleave and compression topology), so no Navi10-derived or emulator-default
value is treated as PS5 fact. A narrow exception is topology-independent: a
single-mip, single-sample D32 `64KB_Z_X` main plane uses 128×128-pixel blocks.
At 1920×1080 its 64-KiB-aligned footprint is `0x870000` bytes, and a uniform
`1.0f` initialization is simply `0x3f800000` repeated across that allocation;
internal swizzle permutations cannot change a uniform field. Per-coordinate
CPU addressing and HTILE remain blocked on the real topology.

The public no-HTILE state also requires `DB_RENDER_CONTROL` to disable depth
and stencil compression and `DB_STENCIL_INFO` to disable tiled stencil. These
are not optional consequences of the 16-entry target block. A depth-enabled,
write-enabled `LESS_EQUAL` state is a separate `DB_DEPTH_CONTROL` value. The
offline register plan is reproducible, but clear visibility is intentionally
still a gate: the observed post-draw completion ACQUIRE covers its private
label, not the depth allocation. The implementation must validate an explicit
CPU-fill, DMA-plus-range-ACQUIRE, or graphics slow-clear path before claiming
reusable per-frame depth.

## Frame ownership contract

No additional pacing import is required for the first animation loop. Two
independent conditions govern reuse:

1. the terminal GPU fence completes before command, shader or vertex memory is
   reset;
2. the exact VideoOut flip event returns the submitted `flipArg` before its
   display buffer is rendered into again.

Use a monotonic positive 48-bit token per frame and retain one owner token per
backbuffer. `sceVideoOutWaitVblank` can regulate CPU production and
`sceVideoOutGetFlipStatus` can provide telemetry/fallback, but neither proves
GPU completion and neither replaces the terminal fence. Animation time should
come from a monotonic clock rather than assuming one event equals one fixed
duration.
