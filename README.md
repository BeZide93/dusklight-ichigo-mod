# Ichigo Kurosaki — Bleach × Twilight Princess
Bring a Soul Reaper to Hyrule! This mod replaces Link with Ichigo Kurosaki from Bleach, adapted for Dusklight using model parts and assets from LINE BLEACH – Paradise Lost and Jump Force.

The mod includes replacements for Link’s Hero’s Tunic, Ordon clothing, Magic Armor, Zora Armor and sumo appearances, alongside matching body parts and replacement sword and scabbard models.

Installation

Place ichigo_mod.dusk in Dusklight’s mods folder and enable Ichigo Mod in the mod manager.

Individual replacements can be configured under Mods → Ichigo Mod → Model Overlays. Restart Dusklight after changing these settings.

Other mods replacing the same assets may conflict. Mixing Ichigo and vanilla model parts may cause visible seams.

**Credits & Asset Sources**

**Mod adaptation**
- **BeZide93 / BeZide** — Model assembly, adaptation and rigging adjustments for Twilight Princess, optimization, animation tuning and Dusklight integration.

**Sketchfab model sources**
- **Marianozi** (`white_guy_official23`) — [Ichigo Kurosaki (Sinegami)](https://sketchfab.com/3d-models/ichigo-kurosaki-sinegami-395fe371606e4742b3ec5fe74641a5bc). Listed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **Cyrone™** — [Ichigo Kurosaki](https://sketchfab.com/3d-models/ichigo-kurosaki-4d874ca9990d4ea6a48cfe85f60a3f6d), sourced from **Bleach: Paradise Lost**. Listed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **Karosio** — [Zangetsu (Ichigo Kurosaki’s Zanpakuto)](https://sketchfab.com/3d-models/zangetsu-ichigo-kurosakis-zanpakuto-9a01e07404cb42049853e0fd8dcf65a9), modeled in Blender and textured in Substance Painter. Licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).

These source assets have been modified and converted for use in Twilight Princess through Dusklight. The adapted Zangetsu asset and BeZide’s modifications to it are distributed under **CC BY-NC-SA 4.0**. This asset-specific license statement does not relicense unrelated mod code or other assets.

**Original game assets and rights holders**
- **LINE Corporation / YD Online** — Original game assets from **LINE BLEACH – Paradise Lost**.
- **Spike Chunsoft / Bandai Namco Entertainment** — Original game assets from **Jump Force**.
- **Tite Kubo / SHUEISHA** — Bleach and its original characters.
- **TV TOKYO, dentsu and Pierrot** — Bleach anime rights holders.
- **Nintendo** — The Legend of Zelda: Twilight Princess and its original game assets.

**Special thanks**
- **TwilitRealm and the Dusklight contributors** — Dusklight and its modding framework.

This is an unofficial, non-commercial fan modification, distributed free of charge. Credit for original assets remains with their respective creators and rights holders. Sketchfab contributor credits acknowledge the linked sources and do not imply ownership of the underlying commercial game assets. No endorsement by the original creators, publishers or rights holders is implied.



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

## Magic Armor movement sounds

While Ichigo Mod is enabled, Magic Armor uses the normal movement sounds shared
with Hero's Tunic, without its additional light/heavy armor rattle. This applies
both with and without rupees. Footsteps still follow the ground surface, and
Iron Boots, equipment sounds and the armor's power-up/power-down cues keep their
normal behavior. Armor protection, weight and rupee consumption are unchanged.

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
