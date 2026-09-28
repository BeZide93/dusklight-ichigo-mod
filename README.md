# Dusklight Mod Template

A standalone template for [Dusklight](https://github.com/TwilitRealm/dusklight) mods.

See the [Dusklight modding documentation](https://github.com/TwilitRealm/dusklight/blob/main/docs/modding.md)
for the full mod API: services, hooking game functions, asset overlays, and more.

## Optional update checker

Open **Mods → Ichigo Mod → Check for Updates** to enable release checks. It is
off by default and saved independently of Dawnlight. When enabled, it checks once
on mod load and immediately when switched on; **Check Now** repeats the check.

The checker uses this repository's latest stable GitHub release, not Actions
artifacts. A newer `vMAJOR.MINOR.PATCH` release must include the combined
`ichigo_mod.dusk` asset. Choosing **Update** downloads it and replaces the installed
`ichigo_mod.dusk`; **Later** leaves the installed version untouched. Restart
Dusklight after installation. Renamed packages must be updated manually.

Downloads use Dusklight's HTTP service on all platforms. Turning the option off
cancels pending requests. Failed or incomplete downloads do not replace the mod.

## Model settings

Open **Mods → Ichigo Mod → Model Overlays** to toggle each of the 20 BMD
replacements independently. All switches default to **On**. Files are grouped by
archive: **Alink** (swords and scabbards), **Bmdl**, **Kmdl**, **Mmdl**, and **Zmdl**
(their separate body, face, head and hand models). Identical filenames in different
archives have separate settings, saved in `config.json`.

**Restart Dusklight after changing models.** Overlays change when files are read;
models already cached in memory are not replaced live. Off removes only Ichigo's
replacement, allowing the original game asset or another mod's replacement to
load. Model combinations can have visible seams or mismatched parts.

For the normal Kmdl appearance, `al.bmd`, `al_face.bmd`, `al_head.bmd`, and
`al_hands.bmd` can each be disabled separately. This also affects other actors
that load those same resources, including Dark Link.

The BMD files are bundled under `res/models/` and registered through Dusklight's
runtime overlay service. They must not also be packaged under `overlay/`, which
would keep disabled replacements active. `src/model_overlays.inc` lists every
file and its stable setting key; the model settings test checks asset coverage.

## Quick start

1. Click "Use this template" to create a new repository for your mod.
2. Edit `mod.json.in`: set your mod's `id` (reverse-DNS style, e.g. `com.example.my_mod`),
   `name`, `author`, and `description`.
3. Rename the target in `CMakeLists.txt` (`add_mod(my_mod ...)`) (this names the `.dusk` file).
4. Write your mod in `src/mod.cpp`.
5. Build locally:
   ```sh
   cmake -B build
   cmake --build build
   ```

The result is `build/mods/<name>.dusk`. Copy it into the game's mods folder to try it:

- Windows: `%APPDATA%\TwilitRealm\Dusklight\mods`
- Linux: `~/.local/share/TwilitRealm/Dusklight/mods`
- macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`

During development, rebuild, copy and click **Reload** in the in-game mod manager to pick up changes.

> [!IMPORTANT]
> A mod built locally will only be valid for your own platform, and shouldn't be distributed.
> The repository will build a [cross-platform bundle](#github-actions) for distribution. See below.

## Updating to a new Dusklight version

Change the `DUSKLIGHT_VERSION` line in `CMakeLists.txt` to the new release tag (or commit hash) and reconfigure. The
pinned version is fetched into `dusklight/` automatically. Use the `dusklight/` checkout to browse game code, headers
and mod services.

> [!IMPORTANT]
> The Dusklight checkout is for **reference only**. Mods use
> [services](https://github.com/TwilitRealm/dusklight/blob/main/docs/modding.md#built-in-services) and
> [hooks](https://github.com/TwilitRealm/dusklight/blob/main/docs/modding.md#hooking-game-functions) to interact with
> game code.

## GitHub Actions

The included GitHub Actions workflow builds the mod for the following platforms:
- Windows (AMD64 & ARM64)
- macOS (Apple Silicon & Intel)
- iOS (Apple Silicon)
- Linux (x86_64 & aarch64)
- Android (aarch64)

It then merges the per-platform builds into a single `.dusk` supporting all platforms. (Artifact `mod-combined`) 

Pushing a tag to the repository creates a GitHub release with the combined bundle.

## For Dusklight developers

Point the build at an existing checkout instead of fetching one:

```sh
cmake -B build -DDUSKLIGHT_DIR=~/path/to/dusklight
```

## Optimized hair model

The normal Ichigo hair model uses about **30% fewer strip triangles** and
**28% fewer submitted vertex records**. Fine hair patches, textures and rig data are preserved.
See [hair model details and reproduction](docs/hair-model.md).
