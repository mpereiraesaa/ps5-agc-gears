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
frames but has no frame limit, chunk, sleep or automatic timeout. The user
presses **Options** to request a normal application exit.

The Options path stops producing frames, drains both in-flight slots by GPU
fence and exact VideoOut token, checks the guards, closes Pad/UserService,
VideoOut, direct memory and AGC, emits BYE and calls `_exit(0)`. Returning from
`main` is intentionally forbidden: on FW 12.02 the title CRT's `exit/atexit`
path can remove BigApp and still report a game/app failure. The PS5
system menu's **Close Game** command externally terminates the process and
cannot execute this application-owned teardown; it is not the release
validation path. Automated soaks use the same artifact and finish by injecting
Options after reaching their target. Host tests exercise bounded sequences by
calling the same state machine's `drain` operation directly; no second runner
or frame-limit API exists.

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
