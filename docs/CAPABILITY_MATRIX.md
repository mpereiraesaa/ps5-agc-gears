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
