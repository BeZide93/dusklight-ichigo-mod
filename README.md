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
