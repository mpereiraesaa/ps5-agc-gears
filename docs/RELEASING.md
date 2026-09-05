# Release process

## One production runtime

The default native build is the user-facing release profile:

```sh
make native-release AMDLLPC=/path/to/amdllpc \
  LLVM_READELF=/path/to/llvm-readelf
```

The renderer initializes one persistent state machine and calls `run_frame`
continuously. Frame indices, telemetry and flip tokens are 64-bit and never
restart at an artificial boundary. It emits a heartbeat every 3,600 completed
frames but has no voluntary frame limit, chunk, sleep or automatic exit. The
user closes the title with the PS5 **Close Game** action.

An external Close Game terminates the process through the system lifecycle, so
the TCP transcript normally ends without application BYE. That is expected for
operator closure. Automated soaks use this same artifact: the PC supervisor
observes continuous frame/ownership/guard telemetry, records its target and
closes the exact title when the target is reached. Host tests exercise bounded
sequences by calling the same state machine's `drain` operation directly; no
second runner or frame-limit API exists.

## Candidate archive

After a release-mode native build:

```sh
bash tools/package_release.sh v0.1.0-rc1
```

The deterministic archive contains the title folder, license, notices and
per-file checksums. It intentionally excludes `dev.conf`, logs, captures,
compiler output and development paths. Before distribution, run `make all`,
verify the archive hash from a clean checkout and attach it only alongside the
matching source revision.

## Current pre-publication gates

- Host C and Python contracts pass with warnings as errors.
- Publication audit passes from a generated working tree.
- Native foundation and shader target are pinned.
- Signed SELF integrity is inspected during every native build.
- The exact release artifact must receive a hardware launch/telemetry check
  before tagging; soak duration is controlled externally.
- Repository visibility and final distributable identity remain explicit owner
  decisions; no script publishes or pushes automatically.

Development experiments follow `DEVELOPMENT.md`: a sibling Git worktree and
short-lived `exp/<topic>` branch replace historical copied `stage-*` trees.
