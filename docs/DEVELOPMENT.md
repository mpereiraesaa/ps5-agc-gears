# Development workflow

## Stable product tree

`main` is the reviewable demo, not a scratch area. Reusable renderer and
platform contracts live in `src/` and `include/`; the PS5 adapter lives in
`native/`. Every change to those contracts must keep `make all` green.

Historical `stage-*` copies are not part of this repository. They duplicate
code, hide drift and make it difficult to know which implementation was
tested.

## Isolated experiments

Create each risky renderer experiment in a sibling Git worktree. This gives it
its own build directory and working tree while sharing the repository history:

```sh
git switch main
git worktree add ../ps5-agc-gears-exp-<topic> -b exp/<topic>
cd ../ps5-agc-gears-exp-<topic>
make all
```

The experiment imports the existing `src/`, `include/` and `native/` code from
its branch; it must not copy those directories into a numbered stage. Commit
small changes on `exp/<topic>`, validate them independently, and merge only the
reviewed commits into `main`.

When finished, first confirm that no uncommitted work remains, then remove the
worktree from the primary checkout:

```sh
git worktree list
git worktree remove ../ps5-agc-gears-exp-<topic>
git branch -d exp/<topic>
```

## Required gates

Before merging or publishing:

```sh
make all
git diff --check
git status --short
```

Native changes additionally require a clean `make native-release`, artifact
hashing and one hardware launch with matching TCP telemetry. Longer soaks are
required when synchronization, ownership, memory layout or command emission
changes. Generated `.deps/`, `build/`, `dist/` and `release/` content remains
ignored and disposable.

The private laboratory may retain source notes and hardware evidence, but its
generated stage directories and title packages are never inputs to this repo.
