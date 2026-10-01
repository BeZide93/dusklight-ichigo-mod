# Agent instructions for Ichigo Mod

## Start here

- Read [project context](docs/project-context.md), [README](README.md), and the
  files relevant to the task. This repository is a Dusklight mod, not the host.
- Inspect the working tree, current branch, upstream base and relevant PR before
  editing. Preserve unrelated work. A task naming an existing PR belongs on that
  PR's branch; otherwise use a focused branch from current `main` for a new PR.
- Follow the user's current scope. An analysis-only request must not change code.
  Do not merge, tag, release, bump versions or update dependencies unless requested.
- Continue already-authorized work through verification and PR creation when
  requested. Ask only when a missing decision affects correctness or an action
  falls outside the authorized scope. Report blockers precisely.
- Explain results to the maintainer in German unless asked otherwise; keep code,
  repository documentation and PR descriptions consistent with the existing English.

## Binary files and conversation size

Large BMD/PNG uploads have repeatedly exhausted the conversation context.

- Read, convert, compare and hash binary files on disk. Never print or paste their
  Base64, hex dumps, binary Git patches or full byte arrays into conversation output
  or model-authored tool arguments. Avoid whole-file connector reads of binaries.
- Prefer an authenticated Git checkout and ordinary file-based commit/push.
  A connected GitHub plugin does not imply terminal Git authentication.
- An API upload is acceptable only if a verified file/stream transport keeps the
  payload outside the conversation and tool transcript. Hiding stdout or computing
  Base64 inside an orchestration call is not sufficient evidence of that guarantee.
- If the only available upload requires a multi-megabyte string in a tool call,
  stop before sending it and explain the blocker. Do not retry the same large call,
  split it across chat messages, or reduce/alter the asset just to transfer it.
- After an interrupted upload, inspect existing remote branches/commits first:
  bytes may already be uploaded even if the PR was never created. Compare the
  remote file with the intended local file by hash or byte equality.
- Report only paths, sizes, hashes, short validation summaries and commit/PR IDs.
  Keep logs bounded and never display credentials.

## Implementation constraints

- Preserve `src/model_overlays.inc` paths and persistent setting keys. Models live
  under `res/models/`; duplicating them under automatic `overlay/` would bypass
  the individual On/Off settings. All 20 current switches default to On.
- Preserve each variant's rig, materials, draw hierarchy and non-target shapes.
  Mesh numbering in an editor is not proof of BMD draw/material order. Inspect
  the actual file and validate converter output rather than assuming a lossless
  DAE/BMD roundtrip. Do not regenerate unrelated assets.
- Keep the fixed 40% eye adjustment compatible with Dawnlight: use the native
  cached translation, not an already reduced value. Respect face-overlay toggles.
- Use the pinned Dusklight API through services/hooks. Inspect the matching host
  sources in `dusklight/`; do not implement the fix only in that fetched checkout.
- Treat runtime actor/material ownership, hook ordering, cleanup and disabled-mod
  behavior as part of the change. Follow the existing service import pattern.

## Build and validation

From the repository root, for a native Linux build with the existing tests:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DICHIGO_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tests/hair_model_test.py
git diff --check
```

Configuration requires CMake 3.26+, a suitable C++ toolchain, Ninja and dependency
download access. `-DDUSKLIGHT_DIR=/path/to/checkout` can use an existing matching
host checkout. The default host version comes from `CMakeLists.txt`.

Select validation according to the change. Documentation-only changes need link,
path, factual and whitespace checks, not a full rebuild. For hair changes, use
the dedicated validator and the invariants in [hair-model.md](docs/hair-model.md).
For runtime changes, run relevant existing tests and a native build when possible.
Check `.github/workflows/build.yml` for current cross-platform requirements.

A local `build/mods/ichigo_mod.dusk` is platform-specific; the CI `mod-combined`
artifact is the distribution bundle. Offline checks do not prove in-game appearance,
animation or Dark Link crash behavior. Clearly separate tests run now, historical
reports, CI results and pending device checks. Finish with the PR link, changed
behavior, validation and any remaining limitation.
