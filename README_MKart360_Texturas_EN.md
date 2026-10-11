# MKart360 — Texture Preparation, Editing, and ROM Rebuilding Guide

This guide documents the workflow used to prepare a Mario Kart 64 ROM, extract its textures, edit each asset category while respecting the game's original structure, and rebuild the ROM.

The goal is not simply to enlarge every image. Each texture has a format, a purpose, and limits imposed by the original ROM. Dimensions, transparency, palettes, useful image areas, margins, and the way the game loads each resource must be respected.

## Current project status

According to the current project tracking, **3,983 of 4,061 textures are working** within the rules and limitations of the ROM structure. **78 textures still have no confirmed solution**.

These numbers describe the current project state; they do not guarantee compatibility with every ROM or revision. Source ROMs can differ in offsets, formats, compression, palettes, and allocations. Keep an untouched copy of the original ROM and test every rebuild.

## 1. Requirements

- A legally obtained Mario Kart 64 ROM to use as the base.
- The complete MKart360 project extracted to a local folder.
- Windows and PowerShell.
- Python 3 and Pillow for reading and writing PNG files.
- Visual Studio 2010 and the Xbox 360 XDK/SDK tools required by the port. Rebuilding textures/the ROM is separate from compiling the executable.

> **Important:** naming a ROM `baserom.br.z64` does not make it a Brazilian ROM. The name is only a workflow convention. If the ROM has a different name or region, provide the correct path and region. Do not mix manifests extracted from one ROM with another without validating offsets and metadata.

## 2. Initial setup

Open PowerShell in the `mk64-master` folder.

### 2.1 Install Python

Run once:

```powershell
winget install Python.Python.3
```

Reopen PowerShell and check:

```powershell
py --version
```

A Python 3 version should be displayed.

### 2.2 Install Pillow

```powershell
py -m pip install --upgrade pip
py -m pip install pillow
```

### 2.3 Prepare the Windows asset tools

```powershell
powershell -ExecutionPolicy Bypass -File ".\xbox360\setup_windows_asset_tools.ps1" -SkipTorch
```

This prepares the project's Windows asset tools. If the script reports a missing dependency, resolve it before continuing; do not ignore setup errors.

## 3. Place and identify the ROM

Copy the ROM into:

```text
MKart360-Texture-Rebuilder\mk64-master\
```

For the Brazilian base used in this workflow, the conventional filename is `baserom.br.z64`. An American ROM may be named `baserom.us.z64`. For other ROMs, pass the path with `--rom` and specify `--region` when needed.

Keep a backup outside the working folder. Never use the only copy of the original ROM as the output of a rebuild.

## 4. Prepare ROM assets

From `mk64-master`, run:

```powershell
py .\PUBLIC_PREPARE_MK64_ASSETS.py
```

The script prepares and checks derived files required by the project. Run it after placing the selected ROM. If preparation fails, stop and fix the cause; do not extract or rebuild using incomplete files or assets generated for a different ROM.

## 5. Extract textures

Run the default extraction:

```powershell
py .\EXTRACT_MK64_TEXTURES.py
```

To specify the ROM and region explicitly:

```powershell
py .\EXTRACT_MK64_TEXTURES.py --rom .\baserom.br.z64 --region br
```

Example for an American ROM:

```powershell
py .\EXTRACT_MK64_TEXTURES.py --rom .\baserom.us.z64 --region us
```

The extractor uses available YAML, JSON, and source metadata to identify symbols, dimensions, formats, and related information. Region selection affects which metadata is found. If metadata for the selected region is unavailable, the program may fall back to US YAML files. This is a metadata fallback, not a ROM region conversion.

Useful extractor options:

- `--force`: overwrite existing PNGs. Use only when intentionally re-extracting, because manual edits may be replaced.
- `--no-karts`: skip driver/kart sprites.
- `--no-lakitu`: skip Lakitu frames.
- `--no-generated`: skip textures identified in the port's generated asset banks.
- `--keep-black`: keep opaque black in cases normally handled with automatic transparency.
- `--out`: choose a different output folder.
- `--dims`: provide a manual dimensions JSON when necessary.

See all options supported by the current version:

```powershell
py .\EXTRACT_MK64_TEXTURES.py --help
```

Keep the manifests and original extracted PNGs. Manifests connect images to ROM symbols, formats, and data; do not delete them or replace them with files from another extraction.

## 6. Editing rules by asset category

Exact names and folders depend on what the extractor finds. Use the paths actually created by the extractor. Do not rename files or folders for convenience: the packer/repacker may depend on original manifests and symbols.

### General rules before editing

1. Back up the extracted folder before batch edits.
2. Check the original dimensions, transparency, and appearance.
3. Identify the resource using its path, manifest, and format; do not rely on the PNG filename alone.
4. For indexed images (CI4/CI8), check the associated palette.
5. Preserve dimensions, framing, transparency, and useful content unless that category's workflow is known to support a change.
6. Save as PNG and keep the expected filename and path.
7. Test a small selection in-game before applying changes in bulk.

### 6.1 Common textures, objects, and items

These include shared game textures, items, effects, HUD elements, and resources used in multiple places.

- **Dimensions:** keep the original dimensions by default. Increasing a PNG's resolution does not automatically make the native ROM accept a larger image.
- **Padding:** do not add it by default. Preserve or add margins only when the original mapping or resource definition requires them.
- **Palette:** for CI4/CI8, palette indices and the expected color mapping are part of the asset; they are not ordinary RGB values.
- **Transparency:** preserve alpha and transparent pixels. Do not fill transparent areas with black or another color unless opaque black is confirmed for that resource.
- **Testing:** check every place where a shared texture appears, since one change may affect several screens or tracks.

### 6.2 Karts and in-race character sprites

Driver/kart sprites are arranged in frames and may use specific palettes and sequences.

- Keep frame order, names, and count unchanged.
- Preserve each frame's dimensions and the character/kart framing.
- Do not crop wheels, shadows, hands, heads, or parts close to an edge.
- Do not add arbitrary padding: even a few pixels can make animation jitter or shift a sprite.
- For CI4/CI8, preserve the palette associated with the character/frame; do not substitute a generic palette or reorder indices.
- For HD redraws, keep content inside the original useful area and avoid halos along transparent edges.
- Test multiple characters, turns, collisions, animations, and wheels—not just a stationary driver.

### 6.3 Lakitu and animated frames

Lakitu assets may contain multiple frames used in different situations.

- Preserve frame order and filenames.
- Do not combine frames into a single atlas unless the manifest and loader explicitly expect one.
- Preserve frame area and alignment; extra margins can change animation placement or its visual center.
- Respect the original palette for indexed formats.
- Keep transparency clean, without colored fringes over the scenery.
- Test different states and gestures, not just one frame.

### 6.4 Character selection, portraits, and menus

This category includes portraits, selection images, backgrounds, and menu graphics.

- Keep each file at the path and filename referenced by its JSON/manifest.
- For coordinate-positioned portraits and elements, do not add external margins without checking how the game positions them. Existing empty space may be part of the alignment.
- Do not stretch characters to fill the image; preserve proportions, visual center, and space around the head and shoulders.
- CI4/CI8 images need the original or a verified compatible palette. RGBA16/RGBA32 have no indexed palette, but the expected format must still be respected.
- Test entering and leaving character selection and returning from other screens. Distortion after navigating back may indicate a loading/state/association issue, not just a bad PNG.

### 6.5 Backgrounds and menu screens

Backgrounds may use specific dimensions, crops, tiles, or transparency handling.

- Do not assume a background can be replaced with one larger image.
- Preserve composition and proportions. If it consists of strips, tiles, or segments, edit each part described by the manifest.
- Do not automatically add margins; empty space may be part of the layout or crop.
- Check the manifest and format. Indexed backgrounds must retain compatible palettes; converting them to RGB does not make the game interpret them the same way.
- Test supported screen modes and resolutions, checking for repetition, seams, and incorrect cropping.

### 6.6 TKMK00 — compressed menu images

TKMK00 is a special compressed format used by some Mario Kart 64 images. It is not an ordinary PNG or MIO0 resource.

- Preserve dimensions, aspect ratio, and content within the expected rectangle.
- Do not add padding without evidence in the manifest; TMEM tiles, crops, and internal margins may be significant.
- Do not automatically apply CI4/CI8 palette rules. TKMK00 has its own decoding and reconstruction process.
- Do not rename files, manually alter manifests, or copy metadata from another ROM.
- Consider a rebuild valid only after the TKMK00 tool validates the encoding and the asset is tested in-game. If the encoded block cannot fit the original allocation, do not force the write.

### 6.7 Tracks, scenery, and Rainbow Road

Track textures may be shared, tiled, or loaded from different banks. Rainbow Road may also have effects/animation frames that use a different path from ordinary textures.

- Keep files in the directories recognized by the manifest and packer. Do not add extra folder levels or move frames elsewhere without updating and validating the workflow.
- Preserve UV mapping, orientation, repetition, and edges that meet neighboring tiles.
- Do not add padding by default: it can create seams in repeating tiles or shift content mapped to fixed coordinates.
- Follow the manifest's format. CI4/CI8 require compatible palettes; RGBA16/RGBA32 encode color directly, but input format and dimensions still matter.
- For Rainbow Road, verify track textures separately from neon/effect frames. PNGs existing on disk does not prove that the loader uses them.
- Test the track in motion from different angles and distances, looking for seams, flicker, original textures appearing over HD ones, or unchanged assets.

### 6.8 Fonts, letters, and numbers

Individual characters may be small I4, IA4, CI, or other native-format textures. Pixel dimensions and placement are part of the glyph design.

- Do not draw outside the original rectangle.
- Do not add external padding on your own; a few pixels can change spacing or make a character disappear or touch its neighbor.
- Respect the format and how intensity/alpha are interpreted. In intensity formats, pixel values may represent brightness rather than RGB color.
- If several characters may be affected, do not fix only one letter. Check related letters, numbers, and symbols, then test complete words across relevant screens.
- If the ROM rebuild cannot safely rewrite a compressed block or might alter neighboring bytes, preserve the original and document the limitation.

### 6.9 Frames, animations, and sequences

- Preserve filenames, indices, and frame order.
- Do not replace a sequence with a sprite sheet unless the manifest explicitly supports it.
- Keep dimensions and alignment consistent between frames.
- Verify that the frames are actually connected to the loader used by the game; a correct JSON file alone does not guarantee that the port loads them.

## 7. Palette and padding quick reference

Always confirm the actual format in the manifest.

| Resource format | Palette | Padding | Safe rule |
|---|---|---|---|
| `CI4` / `CI8` (indexed) | Required | Do not add without evidence | Preserve indices and associated TLUT |
| `I4` / `I8` (intensity) | Not an RGB palette; intensity matters | Do not add without evidence | Preserve intensity, mask, and transparency |
| `IA4` / `IA8` (intensity + alpha) | Not an RGB palette; intensity and alpha matter | Do not add without evidence | Preserve transparency and contrast |
| `RGBA16` / `RGBA32` | No indexed palette | Do not add without evidence | Preserve format, alpha, and expected dimensions |
| `MIO0` | Depends on decoded data format | Depends on texture and mapping | Rebuild only if it fits the original allocation |
| `TKMK00` | Special workflow; do not assume CI4/CI8 | Do not add without evidence | Use the TKMK00 tool and validate the block |
| Sprites/frames/tiles | Depends on actual format | Alignment usually must remain exact | Do not change order, position, or dimensions without validation |

**Important:** obtain the format from manifests/metadata or project source. Do not infer it from appearance alone.

## 8. Rebuild the edited ROM

`ROM_REPACK_TEXTURES.py` uses the manifests and extracted PNGs. For safety, validate changes first without writing to the ROM. The write mode creates a separate output file.

### 8.1 Validate without writing

```powershell
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all
```

This is a validation/preview step; it should not replace the original ROM. Adjust `--rom` for a different base ROM. You can also test one type at a time:

```powershell
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type mio0
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type tkmk00
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type lakitu
```

### 8.2 Write a new ROM

Only after reviewing validation results, run:

```powershell
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all --patch --output .\baserom_MOD.z64
```

The expected output is `baserom_MOD.z64`, while `baserom.br.z64` remains unchanged. Adjust `--rom` if the base ROM has another name.

The repacker is designed not to relocate data or change pointers. It should write selected replacements only when they fit the original compressed allocations and pass round-trip validation. If a texture does not fit, conflicts, or fails validation, do not work around the error by enlarging the block or overwriting neighboring data.

See the current options with:

```powershell
py .\ROM_REPACK_TEXTURES.py --help
```

## 9. Test without losing working assets

1. Keep the original ROM and a clean extraction.
2. Make a small change in one category.
3. Run validation without `--patch` first.
4. If validation succeeds, create a separate output ROM.
5. Test it in the emulator or target environment.
6. Check the asset in its actual in-game context, not only in a PNG viewer.
7. If something breaks, return to the last known-good ROM and PNGs; do not mix experiments into the working base.
8. Approve the change before moving to the next category.

If an image does not change in-game, it may be linked to another symbol, loaded from another bank, use a different palette, be part of an atlas, or not be connected to the path being edited. Before changing loader code, verify the symbol, manifest, hash, and resource actually loaded.

## 10. Original ROM limitations

The main limitation is the original ROM and code structure. Many textures are stored in compressed blocks with fixed reserved space. An edited image can become larger after compression and no longer fit, even when it looks correct.

Other complications include shared palettes, tiled textures, animation frames, intensity formats, special compression, and metadata differences between ROM revisions. Therefore:

- There is no universal padding rule for every image.
- Not every texture can be increased in resolution inside the native ROM.
- Not every exported PNG can necessarily be written back by the repacker.
- Do not reuse offsets/manifests from another ROM without checking compatibility.
- A texture is not considered fixed until it is confirmed in-game.
- If a replacement exceeds its original allocation, a larger loader/allocation change may be needed; forcing the write is unsafe.

This is why, out of the **4,061 textures** currently tracked, **3,983 are working** and **78 remain without a confirmed solution**. Those cases require individual investigation without damaging textures that already work.

## 11. Compile the Xbox 360 port

ROM rebuilding and compiling the port are separate tasks. To compile the Xbox 360 project, install Visual Studio 2010 and the compatible Xbox 360 XDK/SDK tools required by the project configuration.

Open `MK64.sln` in the compatible environment and build the Xbox 360 configuration/platform specified by the project. XDK paths and versions can vary. If compilation fails, use the actual compiler error to fix the dependency or configuration instead of replacing code files indiscriminately.

The rebuilt ROM does not automatically replace `MK64.xex`. For port testing, follow the file layout expected by the build in use; do not assume an external texture pack is required if the chosen workflow writes changes directly into the ROM.

## 12. Workflow summary

From the `mk64-master` folder:

```powershell
py -m pip install pillow
powershell -ExecutionPolicy Bypass -File ".\xbox360\setup_windows_asset_tools.ps1" -SkipTorch
py .\PUBLIC_PREPARE_MK64_ASSETS.py
py .\EXTRACT_MK64_TEXTURES.py --rom .\baserom.br.z64 --region br
# Edit PNGs while respecting category, manifest, format, and palette.
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all
py .\ROM_REPACK_TEXTURES.py --rom .\baserom.br.z64 --extracted .\extracted_textures --type all --patch --output .\baserom_MOD.z64
```

For another ROM, adjust `--rom` and `--region`. The filename is only a convention; metadata compatibility and the actual ROM structure are what matter.

This project involves careful editing and rebuilding. Preserve categories that already work and investigate exceptions individually instead of applying broad changes that might break the rest.

## Credits

- **[n64decomp/mk64](https://github.com/n64decomp/mk64)** — the original Mario Kart 64 decompilation, which provides the foundation for this project.
- **[sirdankz/MKart360](https://github.com/sirdankz/MKart360)** — the Xbox 360 port, including its native build, console-to-console multiplayer, and rendering fixes. This repository is a fork of that project.
- **[ffeliph999](https://github.com/ffeliph999)** — to fix issues with the big screen and missing trees, as well as for graphical improvements and performance optimizations.
