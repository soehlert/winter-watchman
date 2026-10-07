# Winter Watchman - NES Architecture & Development Reference

This document serves as a quick-reference guide for architectural decisions, hardware quirks, graphics tools, and workflows in the Winter Watchman codebase.

---

## 1. NES Hardware Architecture & Raster Splits

### Sprite 0 Hit & Mid-Frame CHR Bank Switching
```c
// Quick break for nmi to make sure we don't have glitches
ppu_wait_nmi();
wait_for_sprite0_hit();
set_chr_bank_0(CHR_BANK_TILES);
```
**What does this mean?**
* **The Problem:** The NES Picture Processing Unit (PPU) can only map one 4KB background tile bank into memory at a time. The game needs ASCII font letters for dialogue/menus (`CHR_BANK_MENU` / Bank 0) AND pixel art tiles for the room (`CHR_BANK_TILES` / Bank 1).
* **`ppu_wait_nmi()`**: Pauses execution until Vertical Blank (VBlank) starts at scanline 241. During VBlank, the PPU NMI interrupt handler swaps the background bank to `CHR_BANK_MENU` (Bank 0) so the top of the screen renders the font in the HUD area.
* **`wait_for_sprite0_hit()`**: The CPU waits in an assembly loop watching the PPU status register (`$2002`). Sprite 0 is positioned at scanline 40 (bottom of the HUD). When the cathode-ray beam draws a non-transparent pixel of Sprite 0 over a non-transparent background pixel, the hardware flag flips!
* **`set_chr_bank_0(CHR_BANK_TILES)`**: The instant Sprite 0 hits, the CPU writes to MMC1 mapper registers to swap Bank 0 to Bank 1 (room tiles) for the rest of the scanlines (scanlines 41–240). This allows seamless font at the top and room tiles below with zero glitches.

### Sprite 0 Placement: Test Code vs. Production Code
```c
// Draw sprite0 onto the screen so we can test it.
oam_spr(249, HUD_PIXEL_HEIGHT - NES_SPRITE_HEIGHT - 0, HUD_SPRITE_ZERO_TILE_ID, 0x00, 0);
```
* **Is this test code?** **NO.** This is essential production hardware code.
* The starter kit author used confusing phrasing ("so we can test it"), but they meant: *place Sprite 0 so the PPU hardware can detect the pixel collision hit*.
* If this line were removed, Sprite 0 would never collide, `wait_for_sprite0_hit()` would hang forever, and the NES would freeze completely.

### Defensive Programming in `draw_game_text()`
```c
// Once we have drawn the text set this back to null, so problems are easier to find.
currentText = NULL;
```
* **Is this debug code?** No, it is standard defensive C programming.
* When dialogue finishes, setting the pointer to `NULL` prevents stale pointer re-execution. If code ever mistakenly triggers text without supplying a valid string, `if (currentText == NULL)` safely traps it with an error rather than reading garbage memory from cartridge ROM.

---

## 2. HUD & Dialogue Positioning

### Why is dialogue in the HUD area instead of a bottom pop-up bubble?
* **NES Raster Constraints:** To render text at the bottom of the screen (scanlines 180–240), the PPU would have to render map tiles first, and then switch to the font bank mid-frame at scanline 180.
* Without hardware scanline IRQ counters (like the MMC3 mapper), an MMC1 game would have to burn CPU cycles spinning on raster beams for almost the entire frame.
* NES Starter Kit places both HUD and text in scanlines 0–48 (rows 24–29 of Nametable A, wrapped to the top via `scroll(0, 192)`).
* To prevent the HUD from disappearing after dialogue, `draw_hud()` and `update_hud()` must be called upon exiting `GAME_STATE_SHOWING_TEXT`.

---

## 3. Sprite System & `spriteDefinitions`

### Why does Main Character have `SPRITE_ANIMATION_NONE, SPRITE_MOVEMENT_NONE`?
```c
SPRITE_TYPE_NPC, 0x00, SPRITE_SIZE_16PX_16PX | SPRITE_PALETTE_0, SPRITE_ANIMATION_NONE, SPRITE_MOVEMENT_NONE, 0x00, 14, 0x00, // Main Character / Winter Watchman (ID 0, tile 0x00, Palette 0)
```
* The active player is **not** driven by `spriteDefinitions[]`. The player is driven by custom physics and 4 hardware OAM sprites in [`source/c/sprites/player.c`](file:///Users/soehlert/projects/personal/winter-watchman/source/c/sprites/player.c).
* `spriteDefinitions[]` is only for map entities (enemies, items, doors, NPCs).
* ID 0 exists in `spriteDefinitions` solely so the automated tool `sprite_def2img` exports the Winter Watchman's sprite tile into `graphics/generated/sprites.png` for Tiled to show him in the map editor palette.

---

## 4. NEXXT Guide & CHR Editing on macOS

NEXXT is the gold-standard NES tile and sprite studio. Here is how to use it seamlessly with the Winter Watchman project:

### Running NEXXT
* Launch **NEXXT.exe** via your bottle in **Whisky** (or Porting Kit).

### Opening & Saving Project Assets
* **Sprites:** `Patterns` -> `Open CHR...` -> select `/Users/soehlert/projects/personal/winter-watchman/graphics/sprites.chr`.
* **House/Room Tiles:** `Patterns` -> `Open CHR...` -> select `/Users/soehlert/projects/personal/winter-watchman/graphics/tiles.chr`.
* **Saving:** Press **`Ctrl + S`** (or `Patterns` -> `Save CHR`).
* **Deploying to Game:** Run `./build.sh` in the project root. Your updated pixel art compiles immediately into the ROM!

### Understanding the 16x16 Metasprite Layout
The NES hardware only natively understands 8x8 pixel tiles. All characters (Winter Watchman, Wife, Kid) are 16x16 **metasprites** composed of four 8x8 tiles in a 2x2 grid:
```
+---------------+---------------+
| Top-Left      | Top-Right     |
| Tile N        | Tile N + 1    |
+---------------+---------------+
| Bottom-Left   | Bottom-Right  |
| Tile N + 16   | Tile N + 17   |
+---------------+---------------+
```
* **Tile 0x00:** Top-Left of Winter Watchman (facing down)
* **Tile 0x01:** Top-Right of Winter Watchman
* **Tile 0x10 (Row 1, Col 0):** Bottom-Left of Winter Watchman
* **Tile 0x11 (Row 1, Col 1):** Bottom-Right of Winter Watchman
* **Tile 0x02–0x03 / 0x12–0x13:** Walk animation frame 2 (facing down)
* **Tile 0x04 / 0x14:** Facing UP
* **Tile 0x20 / 0x30:** Facing RIGHT
* **Tile 0x24 / 0x34:** Facing LEFT
* **Tile 0x08 / 0x18:** Wife NPC (Metasprite ID 1)
* **Tile 0x0A / 0x1A:** Kid NPC (Metasprite ID 2)

### Setting Colors in NEXXT (Palette Matching)
In the top palette bar of NEXXT, click the 4 color swatches for Subpalette 0, 1, or 2 to match the game's actual hardware palettes:
* **Color 0 (Transparent / Background):** Always set to Black (`$0F`).
* **Winter Watchman (Sprite Palette 0):**
  - Color 1: Fur / Warm Tan (`$27`)
  - Color 2: Parka shading / Beard (`$0F` Black or `$00` Dark Gray)
  - Color 3: Goggle Headlights (`$30` Crisp White)
* **Wife NPC (Sprite Palette 1):**
  - Color 1: Black Hair (`$0F`)
  - Color 2: Peach Skin (`$37`)
  - Color 3: Red Top (`$16`)
* **Kid NPC (Sprite Palette 2):**
  - Color 1: Dark Brown Hair (`$08`)
  - Color 2: Peach Skin (`$37`)
  - Color 3: Bright Green Hoodie (`$2A`)

### Key NEXXT Shortcuts & Tips
* **`1` / `2` / `3` / `4`**: Quick-select color index 0, 1, 2, or 3.
* **Left Click**: Draw pixel with selected color.
* **Right Click**: Draw with Color 0 (erase) or pick color.
* **`Ctrl + Z`**: Undo.
* **`H`**: Flip selected tile horizontally.
* **`V`**: Flip selected tile vertically.
* **`2x2` Button (Metasprite mode)**: On the toolbar, click the `2x2` button so your selection box grabs a full 16x16 metasprite at once instead of a single 8x8 tile.

---

### Alternative: Local Offline Browser Editor (`tools/chr_editor.html`)
If you ever want to make a quick 5-second pixel adjustment without opening NEXXT/Wine:
* Double-click `tools/chr_editor.html` in Finder (or `open tools/chr_editor.html`).
* Loads in Safari, supports 16x16 and 8x8 modes, with built-in presets for the Watchman, Wife, Kid, and House tiles.


---

## 5. Palette Architecture Summary

| Palette Slot | Purpose | Colors in `main_bg.pal` / `main_sprite.pal` |
| :--- | :--- | :--- |
| **BG Subpalette 0** | House Interior (Floors/Walls/Wood) | Black ($0F), Carpet Gray ($00), Warm Wood Brown ($17), Deep Indigo ($02) |
| **BG Subpalette 1** | Outdoors Snow & Area Rug | Black ($0F), Cold Blue ($11), Ice Cyan ($21), Pure Snow White ($30) |
| **BG Subpalette 2** | Desk Gray Accent | Black ($0F), Dark Gray ($00), Dark Gray ($00), Dark Gray ($00) |
| **BG Subpalette 3** | **HUD & Dialogue Text** | Black ($0F), Dark Gray Border ($00), Red Hearts ($16), Crisp White Text ($30) |
| **Sprite Subpalette 0** | **Winter Watchman** | Black ($0F), Fur/Skin Tan ($27), LED Headlight White ($30) |
| **Sprite Subpalette 1** | **Wife NPC** | Black Hair ($0F), Peach Skin ($37), Red Top ($16) |
| **Sprite Subpalette 2** | **Kid NPC** | Dark Brown ($08), Peach Skin ($37), Bright Green Hoodie ($2A) |
| **Sprite Subpalette 3** | Reserved / Enemies | Black ($0F), Blue ($12), Gray ($10), White ($30) |
