# Architecture target

The standalone demo will have four explicit layers:

1. **Native application shell:** title lifecycle, logging and exact teardown.
2. **Platform services:** direct memory, VideoOut and event handling.
3. **AGC renderer:** initialization, resource ownership, command composition,
   submission, fence and double-buffer presentation.
4. **Shader toolchain:** public `gfx1013` compilation and metadata translation.

## Required invariants

- All GPU-referenced storage remains alive until its completion fence.
- A zero submit return is not success without observable effect and completion.
- CX, UC and SH indirect packets use firmware builders and their measured
  five-DWORD cursor contract; they are not manually synthesized.
- Shader symbol offsets and sizes come from the PAL ELF symbol table.
- Cleanup is ordered and fail-closed when ownership is ambiguous.
- The application never reads or modifies another process.

## Extraction boundary

The initial implementation currently lives in the private laboratory under
`apps/agc-native-sce`, `probes/ps5-agc-phase0` and `research/gpu/tools`. Files
must not be copied wholesale. Each reusable unit will be rewritten or migrated
with its provenance, test and public dependency recorded.
