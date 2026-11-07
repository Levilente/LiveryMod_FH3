# Livery Decals for Forza Horizon 3

An [FH3 Mod Platform](https://github.com/Levilente/FH3_ModPlatform) mod. Drop PNG pictures into `fh3-mods\liverymod` and they show up as
new decals in the livery editor, under **Apply a Manufacturer Decal**, after the game's own ones. Nothing is replaced: take a PNG out
and its decal is gone from the menu on the next start.

## Install

1. Install the FH3 Mod Platform (**1.2.1 or newer**).
2. Copy `LiveryMod.dll` into `fh3-mods` next to `azimuth.exe`.
3. Start the game once: the mod makes `fh3-mods\liverymod`. Put your PNGs in it and restart the game.

## Pictures

- Any PNG, any size. It is fitted into a square texture keeping its proportions, centred, with a transparent border (the way the game's own
  wide logos sit in theirs). Every decal's texture is 1024 x 1024 pixels (`Size` in the ini); a smaller picture is scaled up to fit.
  Transparency is kept.
- A decal's id comes from its file name. Keep the name and the decal keeps its id, so liveries that use it keep working; rename the file
  and it becomes a different decal. Don't remove a PNG that a saved livery still uses.
- `fh3-mods\LiveryMod.ini`: `Size=256` gives the game's own size (use it if a big decal ever misbehaves); `Log=1` writes which PNG became which decal to `platform.log`; `Premultiplied=1` stores colours multiplied by
  alpha (try it if edges look dark or light in the game).

## Build

`cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<platform>/cmake/mingw-w64-x86_64.cmake -DFH3_PLATFORM_DIR=<platform>`, with llvm-mingw.
`tools/swatch_tool.cpp` builds on any machine and turns a PNG into the `.swatchbin` / `.modelbin` the mod hands to the game.

MIT licensed; `third_party/stb_image.h` is public domain (Sean Barrett).
