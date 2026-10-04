# Implementation Plan: Ice Chopper (Weapon Pickup & Combat)

This plan outlines the 4 stages to implement the **Ice Chopper** melee weapon from *The Winter Watchman* lore into the NES game.

---

## Stage 1: Sprite Graphic Creation

### 1.1 Visual Assets Needed
* **Ground Pickup Item**:
  * An 8×8 (or 16×16) tile representing the ice chopper lying on the ground.
  * Wooden/metal handle with a sharp chisel pick head.
* **In-Hand / Swing Sprites**:
  * Two 8×8 tiles (combined into 16×8 horizontal or 8×16 vertical meta-sprites) extending from the player during a chop:
    * **Horizontal Chop (Left / Right)**: Blade extending forward horizontally.
    * **Vertical Chop (Up / Down)**: Blade extending forward vertically.

### 1.2 Pipeline & Tools
* **Target File**: `graphics/sprites.chr` (4KB 2bpp NES pattern table).
* **Open Space**: Tiles `0xc0`–`0xd7` (and many higher tiles) are currently unused in `sprites.chr`.
* **Editing Approaches**:
  * **Option A (Python CHR Tool)**: Write a pixel-map converter script to inject 8×8 pixel arrays directly into `graphics/sprites.chr`.
  * **Option B (PNG $\leftrightarrow$ CHR Pipeline)**: Use a tool to export/import tiles from `graphics/generated/sprites.png` for editing in Mac graphics software (Aseprite, Photoshop, Piskel, etc.).

---

## Stage 2: Engine Registration & Map Placement

### 2.1 Engine Registration
1. **Define Constant**:
   * File: `source/c/sprites/sprite_definitions.h`
   * Add: `#define SPRITE_TYPE_ICE_CHOPPER 0x08`
2. **Add Sprite Definition**:
   * File: `source/c/sprites/sprite_definitions.c`
   * Add 8-byte entry to `spriteDefinitions[]`:
     * Byte 0: `SPRITE_TYPE_ICE_CHOPPER`
     * Byte 1: Tile ID in `sprites.chr`
     * Byte 2: `SPRITE_SIZE_8PX_8PX | SPRITE_PALETTE_1`
     * Byte 3: `SPRITE_ANIMATION_NONE`
     * Byte 4: `SPRITE_MOVEMENT_NONE`
     * Bytes 5–7: `0x00, 0x00, 0x00` (unused)

### 2.2 Map Placement
* **Tool**: Tiled (`levels/overworld.tmx`).
* **Layer**: `Sprites` object layer.
* **Location**: Place a sprite object at the desired tile coordinates on Level 1.
* **Build**: Run `./build.sh` to compile `.tmx` into C data structures via `tmx2c`.

---

## Stage 3: Collision, Collection & Inventory

### 3.1 Inventory State
* **Variable**: Define global `unsigned char hasIceChopper;` in `source/c/globals.h` and `source/c/globals.c` (initialized to `0`).

### 3.2 Collision & Despawn Logic
* File: `source/c/sprites/player.c` inside `handle_player_sprite_collision()`:
  * When `lastPlayerSpriteCollisionId` matches an ice chopper sprite:
  * Set `hasIceChopper = 1;`
  * Despawn from screen:
    ```c
    currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
    ```
  * Play pickup chime: `sfx_play(SFX_KEY, SFX_CHANNEL_3);`
  * Mark persistence bitmask so the item does not respawn upon screen re-entry:
    ```c
    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
    ```

---

## Stage 4: Combat Mechanics & Enemy Damage

### 4.1 Input Handling & Swing Timer
* File: `source/c/sprites/player.c` inside `prepare_player_movement()`:
  * Check controller state:
    ```c
    if (hasIceChopper && weaponPosition == 0 && (controllerState & PAD_B) && !(lastControllerState & PAD_B)) {
        weaponPosition = PLAYER_WEAPON_POSITION_FULLY_EXTENDED; // 14
        sfx_play(SFX_SWORD, SFX_CHANNEL_1);
    } else if (weaponPosition != 0) {
        weaponPosition -= 2;
    }
    ```

### 4.2 Drawing the Weapon Sprite
* File: `source/c/sprites/player.c` inside `update_player_sprite()`:
  * If `weaponPosition > 0`, allocate hardware OAM slots `0x20`–`0x27` (`PLAYER_WEAPON_OAM_LOCATION`).
  * Draw the chopper offset from `playerXPosition` / `playerYPosition` according to `playerDirection` (`UP`, `DOWN`, `LEFT`, `RIGHT`).
  * If `weaponPosition == 0`, hide the weapon sprites offscreen (`SPRITE_OFFSCREEN`).

### 4.3 Hitbox Detection
* File: `source/c/sprites/map_sprites.c` inside `update_map_sprites()`:
  * If `weaponPosition > 4`, compute bounding box `(playerWeaponX, playerWeaponY, width, height)`.
  * Loop through active screen enemies; if weapon box overlaps enemy box:
    ```c
    lastPlayerWeaponCollisionId = i;
    ```

### 4.4 Damage, Knockback & Invulnerability
* File: `source/c/sprites/player.c` inside `handle_player_sprite_collision()`:
  * If `lastPlayerWeaponCollisionId != NO_SPRITE_HIT`:
    * Decrement enemy health:
      ```c
      if (--currentMapSpriteData[enemyIndex + MAP_SPRITE_DATA_POS_HEALTH] == 0) {
          currentMapSpriteData[enemyIndex + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
          currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerWeaponCollisionId];
      } else {
          currentMapSpriteData[enemyIndex + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = SPRITE_INVULNERABILITY_TIME;
          currentMapSpriteData[enemyIndex + MAP_SPRITE_DATA_POS_CURRENT_DIRECTION] = playerDirection;
      }
      ```
  * In `map_sprites.c`: Flash sprite every other frame and double knockback speed while `INVULN_COUNTDOWN > 0`.
