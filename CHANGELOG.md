# Changelog

## 1.2.1
- Starts much faster with many decals: the PNGs are converted on several threads at once (a few hundred decals took about a minute before). Together with platform 1.2.1 this keeps the mods that load after this one (Nitro System II, Universal Wheel Spacers) from registering late.

## 1.2.0
- The platform's update check knows this repository (MODS > Updates, start-up notice). Needs FH3 Mod Platform 1.2.0 or newer.
- Every decal gets a 1024 x 1024 texture (`Size` in `LiveryMod.ini`: 256, 512 or 1024, default 1024); smaller pictures are scaled up to it.

## 1.1.0
- Bigger decals: 512 / 1024 textures, so logos stay sharp when zoomed in.

## 1.0.0
- First version: every PNG in fh3-mods\\liverymod becomes a decal under Apply a Manufacturer Decal.
