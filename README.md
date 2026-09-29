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

Open **Mods → Ichigo Mod → Model Overlays** to toggle each of the 23 BMD
replacements independently. All switches default to **On**. Files are grouped by
archive: **Alink** (swords and scabbards), **alSumou** (sumo body, head and hands),
**Bmdl**, **Kmdl**, **Mmdl**, and **Zmdl** (their separate body, face, head and hand
models). Identical filenames in different
archives have separate settings, saved in `config.json`.

**Restart Dusklight after changing models.** Overlays change when files are read;
models already cached in memory are not replaced live. Off removes only Ichigo's
replacement, allowing the original game asset or another mod's replacement to
load. Model combinations can have visible seams or mismatched parts.

For the normal Kmdl appearance, `al.bmd`, `al_face.bmd`, `al_head.bmd`, and
`al_hands.bmd` can each be disabled separately. This also affects other actors
that load those same resources, including Dark Link.

The **alSumou** section independently controls `bls.bmd`, `bls_head.bmd`, and
`bls_hands.bmd` under `Object/alSumou/archive/bmdr/`. Sumo keeps using the face
from the loaded clothing archive;
there is no separate sumo face replacement or face switch.

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

All four Ichigo hair variants (`al_head`, `bl_head`, `ml_head`, `zl_head`) use
reduced geometry, including the fine-surface fix. Depending on the variant, this
removes **30–64% of strip triangles** and **25–69% of submitted vertex records**.
Fine hair patches, textures, rig data and each model's individual toggle are preserved.
See [hair model details and reproduction](docs/hair-model.md).

## Hair movement

Ichigo's five hair joints use gentler procedural wind/idle motion for all head
variants, including sumo. Small strand and sideburn rotations use approximately
**30%** of the native angle; the child tip joint uses approximately **15%**.
A smooth saturation curve eases toward **9 degrees per axis** (tip: **4.5 degrees**)
instead of abruptly clamping, retaining movement without sharp bends.

The code identifies the loaded head's geometry against the heads in this mod's
own bundle, with native byte order taken into account. Changing a head switch
therefore does not change damping until the model actually changes: a cached
Ichigo head remains damped after switching Off, and a cached vanilla or different
head remains native after switching On. Identity is rechecked on head reload,
including reused memory addresses, and when a different model is encountered.
No resource files are read during rendering.

Only the native hair rotation call's arguments change. Simulation state, BMDs,
weights, textures, body/limb rotation, cap/accessory joints, authored head
animations, Wolf Link and status previews remain unchanged. Restart after
changing model switches to ensure cached models are replaced. The final
appearance and tuning still need an in-game test.

## Eye movement

Both eyes use a fixed **40% movement range** for Kmdl (Hero), Bmdl (casual),
Mmdl (Magic Armor) and Zmdl (Zora), including the shared face during sumo.
The code uses Dawnlight's final eye-material calculation hook, covering both
BTK animations and procedural idle/target glances without editing AlAnm files.
Texture scale, rotation, eyelid animation, animation timing and internal eye
interpolation remain unchanged. Wolf Link and status-window previews are excluded.
The adjustment follows the face archive actually loaded by Link, including sumo
model changes. Disabling that archive's face overlay also disables this adjustment;
the three sumo body/head/hand switches are independent. As with other model
settings, restart Dusklight after changing overlays.

When Dawnlight is also enabled, Ichigo's fixed 40% replaces its eye-range setting
for these faces rather than multiplying the two percentages. Use the original
AlAnm/BTK files: previously reduced animation files would still reduce the input
before the runtime adjustment.
