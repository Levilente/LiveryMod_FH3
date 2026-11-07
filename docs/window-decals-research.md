# Window decals (livery editor tabs) - research notes

Goal: four new livery editor tabs (front windshield, rear windshield, left windows, right windows) that take decals, visible from outside and inside.
All offsets are RVAs in `forza_durango_release_final.exe` (add the game base). Confirmed in game with `tools/probe/livery_windows_probe.cpp` unless marked (static).

## Editor tabs
- 6 sections, ids: Front 0, Back 1, Top 2, Left 3, Right 4, Wing 5. Screen order: Front(0), Left(3), Top(2), Right(4), Back(1), Wing(5).
- Tab table: static array at `0x4E2A460`, 6 entries of 0x18 bytes `{int section, TextObj* name, extra}`, count in qword at `0x4571DE0`.
  Built lazily inside the page-init function `0x14DD290` (guard check at `0x14DD5F7`), then walked by the loop at `0x14DD931..0x14DD9CA`:
  `IsSectionValid(editor, id)` (`0x3823C0`), then `MakeTab(page, out, id, text**, extra)` (`0x14C7CC0`), then `container->vtbl[+0x40]`.
  Only that loop reads the table and the count (static xref scan). Names come from string ids `IDS_Section_*` (globals `0x4E57CCC..0x4E57CE0`).
- Wing is special-cased in `IsSectionValid` (`cmp ebx,5` ANDs with `[editor+0x6B4]`).

## Section records (from LiveryMasks\Masks.xml, per car)
- `SectionRecord(obj, id)` (`0x1C8CC70`) returns `obj + 0x420 + id*0xA8` with NO bounds check. The array holds exactly 6 records and is followed directly
  by the masks path string at `obj+0x810` / `obj+0x838`, so it cannot be grown in place; ids >= 6 need a hook on the accessor returning our own records.
- Parser: `0x1C94150` (loops `cmp r15d,6` at `0x1C94519`). Record layout seen in the log: name ("Front"...), valid byte at +0x50 (via IsSectionValid), 4 corner floats, axes/scale/rotation.

## Glass
- Window glass is a separate mesh per window (`scene/exterior/Windows/glassF/glassR/glassLF/glassLR/glassRF/glassRR/glassC_a.modelbin`, interior copies in `scene/Interior/InteriorWindows/`).
- Materials are shared library materials with their own shaders: `car_window`, `car_window_alpha`, `car_windsheild` (exterior), `gls_windsheild_interior` etc. (interior); `gls_edge` uses `gls_ch1glossopacityemissive` (a textured, alpha-capable shader).
  Glass meshes have one TEXCOORD set which is not the livery atlas. The window shaders have no livery sampler: this is the main open problem.

## Plan / open questions
1. Tabs: patch the loop's table pointer to our own 10-entry table and set the count after the lazy init (hook_call on the init-guard footer `0x14DD929`). Safe: nothing else reads the table.
2. Records for ids 6..9: hook `SectionRecord`, return per-object records from our own storage (valid=1, projection params of our choice).
3. Unknown: how layers store/serialise the section id, how the render manager picks the mask/projection (`CDeferredLiveryRenderManager`, `SetActiveEditorSection`), where `ELiverySection` is used as array index/bitmask.
4. Unknown (hardest): getting the rendered window layer onto the glass, outside and inside. Candidate: an overlay mesh per window using an alpha-capable textured material (like `gls_ch1glossopacityemissive`) fed with a per-car window livery texture.
