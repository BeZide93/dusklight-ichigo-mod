# Ichigo Mod project context

This is a handoff for future coding tasks, not an automatic copy of ChatGPT project
history. Read [AGENTS.md](../AGENTS.md) for working rules. Repository source and
current PR state take precedence over this dated overview.

## Identity and boundaries

- Repository: [BeZide93/dusklight-ichigo-mod](https://github.com/BeZide93/dusklight-ichigo-mod).
- Purpose: replace Link with Ichigo from Bleach in Twilight Princess running in
  [Dusklight](https://github.com/TwilitRealm/dusklight).
- This is a standalone mod. [Dawnlight](https://github.com/BeZide93/dawnlight) is a
  separate gameplay mod and a compatibility/reference project, not its build host.
- At the 2026-09-28 documentation baseline `97b21d00524d509232fe16ea9436c8c458e7050d`,
  `CMakeLists.txt` defines version 1.1.1 and defaults to Dusklight `v2.0.0`.
  Read those files again before changing versions or investigating ABI issues.
- Stable mod ID: `dev.bezide.ichigo_mod` in `mod.json.in`; package:
  `ichigo_mod.dusk`. Keep packaging and update-checker names consistent.

## Code and asset map

| Location | Responsibility |
| --- | --- |
| `src/mod.cpp` | Mod initialization, settings UI and lifecycle |
| `src/model_settings.cpp`, `src/model_overlays.inc` | Overlay switches, stable keys and exact resource paths |
| `src/eye_movement.cpp` | Fixed eye texture-translation range and Dawnlight hook ordering |
| `src/update_service.cpp` | Optional release checks and confirmed download/restart flow |
| `res/models/Object/` | BMD models registered through the runtime overlay service |
| `tests/` | Settings/update regression checks, service-import check and hair validator |
| `tools/simplify_hair.py`, `docs/hair-model.md` | Hair simplification and preservation requirements |
| `.github/workflows/build.yml`, `tools/merge_mod.py` | Platform builds and combined package |

The inherited README still starts with template instructions. The concrete project
identity is defined by the manifest, CMake target and source, not that heading.

## Model behavior to preserve

There are 20 independent BMD switches under **Mods -> Ichigo Mod -> Model Overlays**.
All default to On. Archive groups are Alink (swords/scabbards), Kmdl (Hero), Bmdl
(casual), Mmdl (Magic Armor), and Zmdl (Zora). Body, face, head and hands are separate
resources; identical basenames in different archives have independent settings.

The canonical Kmdl body is `res/models/Object/Kmdl/archive/bmwr/al.bmd`. Resolve
other filenames from `src/model_overlays.inc`; do not infer them from the variant
name (some face/hand resources retain an `al_` prefix).

Switches affect subsequent resource reads. Restart Dusklight after changing them:
cached models are not replaced live. Off removes Ichigo's claim so native assets
or another mod can supply the resource. This also affects Dark Link when it loads
the same models. Mixed parts can have visible seams.

## Hair geometry and crash investigation

The maintainer reported that reducing the hair geometry resolved a Dark Link crash.
The first reduction left visible holes; the corrected implementation preserves
small disconnected surfaces and skinning transitions across all four head variants.
See [hair-model.md](hair-model.md) for exact source commits, counts and reproduction.

Do not merge vertices across draw-matrix assignments or discard tiny surfaces only
because their local-space area is small. Preserve original vertex attributes,
rig/material/texture sections and non-hair shapes. Each variant has its own hair
shape index and packet structure; copying the whole Kmdl head to other variants
would overwrite their distinct data. File-derived geometry savings are not a
measurement of renderer buffer occupancy or a guarantee for every mod combination.

## Eye movement

Both eyes use a fixed 40% range for all four human clothing variants. The post-hook
on `daAlink_matAnm_c::calc` covers BTK-driven movement and procedural idle/target
glances. It uses the native cached translation and runs after Dawnlight's adjustment,
so the percentages do not compound. Wolf form, status-window previews and variants
whose face overlay is disabled are excluded.

Use original AlAnm/BTK files when testing. Previously reduced BTK input can still
produce a second reduction before this runtime hook. Keep eyelids, timing, texture
scale/rotation and interpolation intact. Details are in [README](../README.md).

## Updates and distribution

The optional update checker defaults to Off. It looks for a newer stable numeric
release containing `ichigo_mod.dusk`, not an Actions artifact. Installation requires
the user's Update choice and a restart. Use Dusklight's HTTP service and its own
User-Agent handling; the previous custom-header override caused request rejection.

CI builds Linux, Windows, macOS, iOS and Android variants and combines them into
`mod-combined`. A successful local Linux build does not validate all other targets.

## Dated work snapshot: 2026-09-28

- PRs [#3](https://github.com/BeZide93/dusklight-ichigo-mod/pull/3) (model toggles),
  [#4](https://github.com/BeZide93/dusklight-ichigo-mod/pull/4) (hair reduction with
  detail preservation) and [#5](https://github.com/BeZide93/dusklight-ichigo-mod/pull/5)
  (40% eyes) are merged in the baseline.
- [PR #6](https://github.com/BeZide93/dusklight-ichigo-mod/pull/6), branch
  `test/al-bmd-roundtrip`, was open when this overview was written. It replaces only
  the Kmdl `al.bmd` with a rebuilt test asset. Do not treat it as merged or copy it
  into another task without checking its current state.
- The PR #6 file was verified byte-for-byte against the prepared roundtrip ZIP:
  2,558,304 bytes; SHA-256
  `0e66da195225781593cb90799065827bca6ffd709c986b24963be204157a3e58`.
  Its earlier report records matching geometry/UVs/weights/skeleton/material data
  after converter corrections, but CMPR recompression differences in three textures.
  In-game loading, rendering and animations remain a separate validation step.

Before continuing a task, check the live branch, PR discussion, CI and user test
results. Keep this overview concise and update affected facts when behavior changes;
do not copy private chat logs, account information or binary payloads into it.
