# The Winter Watchman - Game Design & Weapons Guide

## 1. Lore & Character Visual Identity
Based on *The Winter Watchman* video lore and vigilante gear:
* **The Vigilante Suit**:
  * Headgear: Leather ushanka aviator hat with fur ear flaps, combined with ski goggles featuring bright LED side-mounted headlights (or full catcher's face mask).
  * Attire: Dark compression base layer with arm racing stripe, blue jeans, tool belt with pouches/holsters, and heavy winter boots.
  * Identity: Secret vigilante patrolling icy driveways and sidewalks at night to protect innocent victims from slipping.
* **The Vigilante Arsenal**:
  1. **Long-Handled Ice Chopper / Pickaxe**: Heavy melee tool used for chipping through hard-packed ice and knocking back enemies.
  2. **Salt Shaker / Salt Pellets**: Considered "a little too passive" by the hero, but ideal as a deployed rear trap against pursuing enemies.
  3. **High-Powered Hair Dryer**: The iconic weapon that blasts hot air to melt thick ice barriers, limited by an outdoor extension cord that can snag on landscaping.

---

## 2. Level & Weapon Progression

### Level 1: The Ice Chopper (Melee Fundamentals)
* **Objective**: Explore the icy streets, locate the dropped Ice Chopper, fight through basic ice slimes/enemies, find the key, and unlock the exit door.
* **Weapon**: **Ice Chopper / Pickaxe**
  * Type: Forward melee swing (Button B or A).
  * Range: 14 pixels in front of the player.
  * Effect: 1 damage + enemy knockback and damage flash.

### Level 2: The Salt Shaker (Trap & Chasing Enemies)
* **Objective**: Face faster, more aggressive enemies that actively hunt the player down.
* **New Mechanics**:
  * **Enemy Chasing AI (`SPRITE_MOVEMENT_CHASE_PLAYER`)**: Instead of random wandering, aggressive enemies compare their position to the player and pursue.
  * **Weapon: Salt Shaker (Consumable Hazard)**:
    * Single-use or limited charges (`saltCount`).
    * Tossed/dropped directly behind the player.
    * Spawns a salt hazard sprite that stays on the ground for a short duration (e.g. 60–120 frames / a few steps) before dissolving.
    * Chasing enemies that step onto the salt take damage and recoil, letting the player escape or defeat them.

### Level 3: The Hair Dryer (Melting & The Extension Cord)
* **Objective**: The exit door and key are trapped behind solid, impassable ice barriers that cannot be broken with normal melee.
* **New Mechanics**:
  * **Weapon: Hair Dryer**: Blasts a cone/beam of heat that damages enemies and melts ice blocks.
  * **Ice Melting**: When the heat blast hits an ice barrier (either a map sprite or a specific background tile), the ice melts away into clear passable pavement.
  * **The Extension Cord Mechanic**:
    * Inspired by the lore (*"sometimes the extension cord gets caught in the landscaping"*).
    * Must plug into an outdoor wall outlet first.
    * The hair dryer only has power when the player is within maximum cord distance (`MAX_CORD_LENGTH`, e.g., within 80–100 pixels of the outlet).
    * Wandering past the cord limit pulls the plug or locks the hair dryer until the player returns closer.

### Level 4 / Climax: The Boss Encounter
* **Concept**: A tough multi-hit winter boss (e.g. "Mr. Freeze", a rogue Snowplow, or an Abominable Ice Demon).
* **Boss Mechanics**:
  * High health bar (e.g., 10–20 HP).
  * Phase 1: Charges across the screen towards the player (lure into salt traps to stun!).
  * Phase 2: Throws ice projectile hazards or summons mini-slimes.
  * Vulnerability: Can only be damaged during specific vulnerable states, or takes massive damage when melted with the hair dryer.

---

## 3. Technical Architecture Breakdown

### A. Item Pickup System (Ground Item $\rightarrow$ Inventory)
1. **Sprite Definitions (`sprite_definitions.h` & `sprite_definitions.c`)**:
   * Define unique type constant (e.g., `SPRITE_TYPE_PICKAXE 0x08`, `SPRITE_TYPE_SALT 0x09`, `SPRITE_TYPE_HAIRDRYER 0x0A`).
   * Add 8-byte entry in `spriteDefinitions[]` with CHR tile ID, palette, size (8x8 or 16x16), and `SPRITE_MOVEMENT_NONE`.
2. **Placement on Map**:
   * Open `levels/overworld.tmx` in Tiled.
   * Place the item on the `Sprites` object layer.
3. **Collision & Collection (`player.c` - `handle_player_sprite_collision()`)**:
   * On collision, increment player inventory flag (e.g. `hasPickaxe = 1;` or `saltCount++;`).
   * Play SFX: `sfx_play(SFX_KEY, SFX_CHANNEL_3)`.
   * Despawn from screen: `currentMapSpriteData[index + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN`.
   * Set persistence bit: `currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId]`.

---

### B. Melee Weapon Attack System (Ice Chopper / Sword)
1. **Attack Trigger (`player.c` - `prepare_player_movement()`)**:
   * Poll controller: check for `PAD_B` press when weapon is owned and not already swinging.
   * Initialize attack timer: `weaponPosition = 14;`.
   * Tick down `weaponPosition -= 2;` each frame until 0.
2. **Rendering Weapon Sprite (`player.c` - `update_player_sprite()`)**:
   * Allocate OAM slots `0x20`–`0x27` (free hardware sprite slots immediately after player).
   * Position weapon offset in front of player based on `playerDirection` (`UP`, `DOWN`, `LEFT`, `RIGHT`).
3. **Hitbox Calculation (`map_sprites.c` - `update_map_sprites()`)**:
   * Compute bounding box `(playerWeaponX, playerWeaponY, width, height)`.
   * Test overlap against all active enemies on screen.
   * If collision detected, store enemy index in `lastPlayerWeaponCollisionId`.
4. **Enemy Reaction & Damage (`player.c` - `handle_player_sprite_collision()`)**:
   * Decrement enemy health: `--currentMapSpriteData[enemyIndex + MAP_SPRITE_DATA_POS_HEALTH]`.
   * If health is 0: despawn enemy and save to persistence bitmask.
   * If health > 0: set `MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN` for flashing and knockback.

---

### C. Salt Trap Mechanism (Dropping Hazard & Timer)
1. **Dropping the Salt**:
   * When button pressed and `saltCount > 0`:
   * Spawn a new map sprite in the first available slot behind the player's current tile.
   * Set its type to `SPRITE_TYPE_SALT_HAZARD` and initialize lifetime countdown: `currentMapSpriteData[index + MAP_SPRITE_DATA_POS_DIRECTION_TIME] = 120;` (e.g. 2 seconds).
   * Decrement `saltCount--`.
2. **Lifetime Countdown**:
   * Each frame in `map_sprites.c`, decrement timer. When it hits 0, despawn sprite (`SPRITE_TYPE_OFFSCREEN`).
3. **Enemy Collision**:
   * In `map_sprites.c`, check collision between enemies and `SPRITE_TYPE_SALT_HAZARD`.
   * If enemy steps on salt: enemy takes damage, gets stunned/knocked back, and salt dissolves.

---

### D. Enemy Chasing AI (`SPRITE_MOVEMENT_CHASE_PLAYER`)
In `source/c/sprites/map_sprites.c` under sprite movement:
```c
case SPRITE_MOVEMENT_CHASE_PLAYER:
    // Every few frames, adjust direction towards the player
    if (ABS(sprX - playerXPosition) > ABS(sprY - playerYPosition)) {
        if (sprX > playerXPosition) {
            currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_CURRENT_DIRECTION] = SPRITE_DIRECTION_LEFT;
        } else {
            currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_CURRENT_DIRECTION] = SPRITE_DIRECTION_RIGHT;
        }
    } else {
        if (sprY > playerYPosition) {
            currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_CURRENT_DIRECTION] = SPRITE_DIRECTION_UP;
        } else {
            currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_CURRENT_DIRECTION] = SPRITE_DIRECTION_DOWN;
        }
    }
    do_sprite_movement_with_collision();
    break;
```

---

### E. Hair Dryer & Extension Cord Mechanics
1. **Extension Cord Distance**:
   * Store outlet position `(outletX, outletY)` on the map.
   * Calculate Manhattan distance:
     ```c
     cordDistance = ABS(playerXPosition - outletX) + ABS(playerYPosition - outletY);
     ```
   * If `cordDistance > MAX_CORD_LENGTH`, hair dryer button click fails (SFX click / fizzle).
2. **Melting Ice Barriers**:
   * The hair dryer blast creates an active weapon hitbox 2–3 tiles forward.
   * When colliding with an ice barrier sprite: sprite disappears with a sizzle SFX.
   * Or if colliding with an ice background tile: call tile replacement logic to swap the ice tile to a clean sidewalk tile.
