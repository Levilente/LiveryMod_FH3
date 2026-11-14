# Changelog

## 1.3.0
- A decal whose PNG was taken out of `fh3-mods\\liverymod` no longer leaves a hole in saved liveries: the mod remembers every decal it has seen (`fh3-mods\\LiveryMod-known.txt`) and keeps the removed ones' ids in the game, not in the menu, showing a "missing sticker" picture.

## 1.2.2
- Decal ids stay at or under 32767: the game keeps a livery decal's id in 16 bits, and a decal with a bigger id showed in the menu but did nothing when picked. A decal that had such an id gets a new one (a saved livery that used it needs the decal put on again); every other decal keeps its id.

## 1.2.1
- Starts much faster with many decals: the PNGs are converted on several threads at once (a few hundred decals took about a minute before). Together with platform 1.2.1 this keeps the mods that load after this one (Nitro System II, Universal Wheel Spacers) from registering late.

## 1.2.0
- The platform's update check knows this repository (MODS > Updates, start-up notice). Needs FH3 Mod Platform 1.2.0 or newer.
- Every decal gets a 1024 x 1024 texture (`Size` in `LiveryMod.ini`: 256, 512 or 1024, default 1024); smaller pictures are scaled up to it.

## 1.1.0
- Bigger decals: 512 / 1024 textures, so logos stay sharp when zoomed in.

## 1.0.0
- First version: every PNG in fh3-mods\\liverymod becomes a decal under Apply a Manufacturer Decal.
