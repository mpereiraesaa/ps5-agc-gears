# Publication roadmap

- [x] Record the initial hardware-accelerated triangle proof.
- [x] Create an isolated publication staging directory and positive allowlist.
- [x] Add a fail-closed publication auditor.
- [x] Choose repository name (`ps5-agc-gears`), GPL-3.0-or-later and local
  development identity `PPSA99997`.
- [x] Confirm intended GitHub owner: `mpereiraesaa`.
- [x] Publish the repository publicly and confirm the development identity.
- [x] Derive the standalone app from a pinned public boilerplate revision.
- [x] Extract the AGC backend behind a small documented interface.
- [x] Extract and host-test the first backend core: two-surface planning,
  SetFlip/release-fence composition and exact GPU/VideoOut completion state.
- [x] Extract sanitized AGC ABI declarations and deterministic MRT0,
  pipeline-register and no-HTILE depth-state builders with host regressions.
- [x] Extract injected event polling, GPU-visible span validation and a
  fail-closed presentation/submission transaction without native test links.
- [x] Extract the direct-memory lifecycle, minimal platform ABI and AGC/driver
  link stubs; compile/link the stubs with the pinned native toolchain.
- [x] Vendor and host-test the project-owned `ps5log/1` client, compile its
  native `sceNet` backend and document the no-filesystem telemetry contract.
- [x] Extract the self-relative shader-header builder and repository-local
  shader embedding assembly; compile both with the native toolchain.
- [x] Extract the checked AGC command-writer boundary with exact direct/draw
  packet sizes, GPU-span validation and transactional render-wait adaptation.
- [x] Add a checked DCB submit descriptor/cache-flush boundary and compile its
  complete firmware-symbol binding with the pinned Prospero target.
- [x] Add the independently authored non-indexed gear mesh generator and host validation.
- [x] Add the three-draw MVP/quaternion/material scene builder and PS5-toolchain
  compatibility check.
- [x] Add and host-test two-buffer ownership tracking with exact 48-bit flip
  tokens and independent GPU/VideoOut completion.
- [x] Replace the three-draw prototype with a tested compositor that enforces
  the measured 27+3 DWORD cursor contract and 90-DWORD total.
- [x] Join scene timing and two-buffer ownership in a platform-neutral animation
  controller and pass a 10,000-frame host soak.
- [x] Add low-overhead frame telemetry with averages/maxima, deadline/error
  counters and a fixed 60-frame persistence cadence.
- [x] Classify imports and reverse-engineering needs by capability in
  `docs/CAPABILITY_MATRIX.md`.
- [x] Move the independently authored lit-Gears LLPC pipeline source without
  committing generated PAL ELF or ISA blobs.
- [x] Add and test the public `gfx1013` shader compiler/extractor with a
  reproducible manifest and no committed generated binaries.
- [x] Translate PAL metadata into sanitized AGC register templates during the
  standalone build and match the hardware-used contract exactly.
- [x] Add host unit tests and a fixture generated entirely from public sources.
- [x] Build without references to the parent laboratory.
- [x] Add CI for shell validation, host tests, generated-tree cleanliness and
  publication audit.
- [x] Replace release-time finite chunks with one persistent production
  `init / run-frame` state machine; expose drain only for host assertions and
  move soak duration to the external supervisor.
- [x] Validate a 300-frame moving double-buffer demo on FW 12.02 with exact
  GPU-fence and VideoOut-token ownership, intact guards and clean teardown.
- [x] Pipeline two frames in flight without weakening per-slot ownership; the
  FW 12.02 10K soak proved depth two and 59.94 fps sustained throughput.
- [x] Replace color DMA with a tested fullscreen-triangle render-target clear;
  pass green visual proof, final black 300-frame run and black 10K soak.
- [x] Add a standalone host `Makefile` for all reusable renderer tests and the
  publication audit.
- [x] Extract the four-draw frame command core into `gears_renderer`, consume
  it from the FW adapter and revalidate the exact refactor on hardware.
- [x] Pass 1,000-frame and 10,000-frame hardware soaks with exact requested,
  completed and verified counts, zero errors and clean teardown.
- [x] Pass a strict 60,000-frame standalone soak with two frames in flight,
  exact fences/tokens, intact guards, zero renderer errors and gap-free BYE.
- [x] Produce a deterministic release-candidate archive and `SHA256SUMS`.
- [x] Perform the fail-closed proprietary-material publication audit.
- [x] Replace copied numbered stages with an isolated Git-worktree development
  workflow that reuses the tested renderer contracts.
- [ ] Reproduce the archive from a fresh clone after this reconciliation PR
  commit and compare its checksum.
- [x] Publish the repository and acknowledge upstream projects precisely.
- [x] Publish the LLPC GFX1013 fork with pinned LLPC/LLVM revisions and a
  target-selection regression test.
- [x] Add the Phase 2 direct-memory resource pool with generation handles and
  exact-token deferred reclamation.
- [x] Add the two-slot per-frame transient ring, named GFX10.3 V#/T#/S#
  builders and explicit CPU-to-GPU cache transition.
- [x] Move camera constants, texture tables and a visible overlay into
  transient resources selected through generated pipeline permutations.
- [x] Add host contracts and a fail-closed structured-evidence validator for
  the resource-foundation path.
- [ ] Pass and archive the Phase 2 60,000-frame hardware gate on FW 12.02.
- [x] Require pull requests on `main`; require the host CI check after this PR
  establishes its final check context.
