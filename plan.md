# Implementation Plan: Fix NPC Dialogue, HUD Restoration & Character Movement

## Issues Reported
1. **HUD Missing After Dialogue**: After talking to the Wife NPC, the HUD disappears.
2. **Character Frozen / Stuck**: After talking, the player character cannot move in any direction.
3. **Cannot Talk Again**: Pressing A after dialogue does not allow talking to the NPC a second time.

---

## Root Causes Identified

1. **HUD Restoration Missing on Dialogue Exit (`main.c` / `game_text.c`)**:
   - `draw_game_text()` clears the HUD's 3 dialogue rows with blank tiles.
   - On exit, `draw_game_text()` clears `currentText` and hides Sprite 0, but does not redraw the HUD border, "PLAYER" text header, or attributes.
   - `main.c` switches `gameState = GAME_STATE_RUNNING` without calling `draw_hud()`, leaving the top HUD area blank/broken.

2. **Directionless Velocity Cancellation in NPC Collision (`player.c`)**:
   - In `handle_player_sprite_collision()`, `SPRITE_TYPE_NPC` unconditionally executes `playerXVelocity = 0; playerYVelocity = 0;`.
   - When the player steps adjacent to an NPC, their bounding box overlaps the NPC. When attempting to walk away, `nextPlayerPosition` is still detected as overlapping, setting velocity to 0 in all directions every frame. The player is permanently trapped.

3. **Duplicate Input & Dialogue State Pipeline (`main.c` & `player.c`)**:
   - Dialogue trigger logic was duplicated in both `prepare_player_movement()` and `handle_player_sprite_collision()`.
   - When `prepare_player_movement()` triggers text, `main.c` continued running physics ticks for that frame.
   - Controller state was not synchronized on dialogue exit, causing input edge-detection (`PAD_A`) desync.

---

## Step-by-Step Implementation Steps

### Step 1: Cleanly Restore HUD & Input on Dialogue Exit in `main.c`
In `source/c/main.c`:
- In `case GAME_STATE_SHOWING_TEXT`:
  1. Complete `banked_call(PRG_BANK_GAME_TEXT, draw_game_text);`.
  2. Call `ppu_off(); banked_call(PRG_BANK_HUD, draw_hud); ppu_on_all();` to restore the HUD frame, background tiles, and "PLAYER" label.
  3. Call `banked_call(PRG_BANK_HUD, update_hud);` to restore hearts and key count.
  4. Flush controller state (`lastControllerState = pad_poll(0); controllerState = lastControllerState;`) so button presses cleanly reset.
  5. Set `gameState = GAME_STATE_RUNNING;`.
- In `case GAME_STATE_RUNNING`:
  - If `gameState` transitions out of `GAME_STATE_RUNNING` (e.g. to `GAME_STATE_SHOWING_TEXT` or `GAME_STATE_PAUSED`), `break;` immediately instead of running remaining physics and sprite updates for that frame.

### Step 2: Fix NPC Collision to Allow Moving Away in `player.c`
In `source/c/sprites/player.c`:
- In `handle_player_sprite_collision()` under `case SPRITE_TYPE_NPC`:
  - Retrieve the NPC's coordinates from `currentMapSpriteData`.
  - Only zero velocity in the direction moving **into** the NPC:
    - If moving right (`playerXVelocity > 0`) and player is to the left: stop X.
    - If moving left (`playerXVelocity < 0`) and player is to the right: stop X.
    - If moving down (`playerYVelocity > 0`) and player is above: stop Y.
    - If moving up (`playerYVelocity < 0`) and player is below: stop Y.
  - Never zero velocity when moving **away** from the NPC.
  - Remove duplicate `PAD_A` dialogue trigger from collision handling (proximity check in `prepare_player_movement()` already handles this cleanly).

### Step 3: Verify Re-triggering Dialogue
- With movement unblocked and input state flushed on text exit, player can press A to re-read dialogue at any time.

### Step 4: Create `docs/reference.md`
- Consolidate all architectural knowledge:
  - Sprite metadata & `spriteDefinitions` architecture
  - Hardware Sprite 0 raster timing & mid-frame CHR bank switching
  - Palette organization and color assignments
  - Dialogue scripting and Tiled map workflow
  - Mac tools for editing NES CHR tiles and sprites

### Step 5: Build, Test & Commit
- Run `./build.sh build-only` to verify clean compilation.
- Commit changes with conventional commit message.
