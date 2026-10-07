# The Winter Watchman - Master Brainstorm & Design Document

## 1. Overview & Source Material Lore
The game is based on *The Winter Watchman* video lore. The protagonist is an urban winter vigilante who patrols suburban sidewalks and driveways at night to prevent people from slipping on ice.

### Character Visual Identity
* **Headgear**: Leather ushanka aviator hat with fur ear flaps; tinted ski goggles with dual bright white LED lights mounted on the sides (or catcher's face mask).
* **Costume**: Dark athletic compression top with a white racing stripe down the arm, blue jeans, tool belt with holster pouches, and heavy winter snow boots.
* **Tone**: Low-budget, DIY superhero dealing with suburban winter hazards and makeshift tools.

---

## 2. Campaign & Level Progression

### Level 1: The Watchman's House & The Neighborhood Street
* **Setting**: Begins inside the protagonist's home on Screen 0.
* **NPCs**:
  * **Partner/Wife**: Expresses concern about the costume, the cold, and disturbing the neighbors.
  * **Kid/Roommate**: Drops a hint that an old ice chopper is hidden down in the storm cellar.
* **The Secret Cellar**:
  * A heavy table in the corner can be pushed aside, revealing a trapdoor.
  * The trapdoor leads to Screen 63 (the cellar), where an Ice Chopper is stored.
* **Outside (Screen 1)**:
  * Street covered in snowdrifts and ice patches.
  * Roaming ice enemies.
  * Player must find a key to unlock the gate to Level 2.
  * If the player did not find the cellar pickaxe, an alternate tool must be scavenged outside or enemies evaded.

### Level 2: The Salt Shaker & Hunting Enemies
* **Setting**: An icy commercial strip or driveway complex with faster, more aggressive threats.
* **New Mechanics**:
  * **Chasing Enemies (`SPRITE_MOVEMENT_CHASE_PLAYER`)**: Enemies track the player's coordinates directly.
  * **Salt Shaker**: Consumable rear-drop weapon. Dropped salt stays on the ground for a short duration (e.g. 60–90 frames). Pursuing enemies that cross it take damage and recoil.

### Level 3: The Hair Dryer & The Extension Cord
* **Setting**: Heavily iced area where doors and key items are entombed in thick ice barriers.
* **New Mechanics**:
  * **Hair Dryer**: Blasts hot air forward to damage enemies and melt ice blocks.
  * **The Extension Cord Constraint**: The player must plug into an outdoor wall outlet. The hair dryer only operates while within a maximum distance (`MAX_CORD_LENGTH`) of the outlet.

### Level 4: The Boss Encounter
* **Concept**: A high-health winter adversary (e.g., "Mr. Freeze", a rogue Snowplow, or an Ice Golem).
* **Mechanics**:
  * Multi-hit health pool (10–20 HP).
  * Attack phases: Charging rushes, summoning mini-hazards, vulnerability windows requiring specific tool interactions (salt stun, heat melt).

---

## 3. Core Gameplay Systems & Mechanics

### A. Melee Combat (Ice Chopper / Pickaxe)
* **Input**: Dedicated attack button (Button B or A).
* **Attack Timing**: Triggers a swing animation (`weaponPosition = 14`), decrementing by 2 each frame until retracted.
* **Hitbox**: Bounding box calculated in front of the player based on `playerDirection`.
* **Enemy Reaction**: Decrements enemy health by 1, triggers invulnerability flash, and applies knockback.

### B. Durability & Handle-Drop System
* **Durability Counter**: `pickaxeDurability` initialized upon pickup (e.g., 20 hits).
* **Wear**: Each successful strike on an enemy or breakable block decrements durability by 1.
* **Breaking Point**: When durability reaches 0:
  * Audio: Loud snapping/breaking sound effect.
  * State: `hasIceChopper = 0` (player cannot swing).
  * Visual: The broken handle drops directly at the player's feet as a temporary sprite, signaling that the tool is destroyed.

### C. Mystery Breakable Blocks (Ice Chunks)
* **Object**: Stationary map sprites (`SPRITE_TYPE_ICE_BLOCK`).
* **Interaction**: Striking the block with a melee tool shatters it and rolls a random outcome:
  * **Empty (40%)**: Block shatters with no item.
  * **Enemy Ambush (30%)**: A frozen enemy is freed and immediately engages the player.
  * **Positive Drop (30%)**: Spawns an item on the ground:
    * *Heart*: Restores player health.
    * *Tool Kit / Whetstone*: Restores durability to current tool.
    * *Replacement Tool Head*: Fully restores weapon.

### D. Enemy Chasing AI
* Instead of random movement, aggressive enemies compare `(sprX, sprY)` to `(playerXPosition, playerYPosition)`:
  * Aligns movement direction along the axis of greater distance.
  * Runs collision checks against map obstacles using the standard movement handler.

---

## 4. Technical Architecture Reference

### Screen Grid Layout
* Total Map Size: $128 \times 96$ tiles ($8 \times 8$ screens, 64 screens total).
* Screen Dimensions: $16 \times 12$ tiles per screen (playable area below HUD).
  * `Screen 0` (Row 0, Col 0): House Interior (Spawn point).
  * `Screen 1` (Row 0, Col 1): Neighborhood Street.
  * `Screen 63` (Row 7, Col 7): The Storm Cellar (Basement).

### Sprite & OAM Allocation
* Player: OAM slots `0x10`–`0x1F` (4 hardware sprites, 16×16 meta-sprite).
* Player Weapon: OAM slots `0x20`–`0x27` (2 hardware sprites, 16×8 or 8×16 meta-sprite).
* Map Sprites: OAM slots `0x40`–`0xBF` (up to 8 active map sprites).

### Transitions & Warps
* Screen transitions between house, cellar, and outdoor screens utilize `do_fade_screen_transition()`.
* State persistence tracked per screen via `currentMapSpritePersistance[64]`.

---

## 5. Asset & Gameplay Punchlist (Execution Order)

1. **House Environment Tiles (Backgrounds)**
   * Extract and import the $16 \times 16$ wood floor, rug, and $32 \times 32$ table from the interior sheet into `graphics/tiles.chr`.
   * Add interior wall and corner border tiles so the room can be enclosed with collisions.
   * Lay out the starting house room on Screen 0 in Tiled.

2. **The Pickaxe / Ice Chopper (Weapons)**
   * Create a 4-tile orthogonal ice chopper (horizontal $16 \times 8$ and vertical $8 \times 16$) in `graphics/sprites.chr`.
   * Register `SPRITE_TYPE_ICE_CHOPPER` in `sprite_definitions.h` and `sprite_definitions.c`.
   * Wire up pickup collision and the swing action.

3. **Enemies (Hazards)**
   * Extract chosen enemy sprites from the `elemental` sheet (e.g., frost demon and ice slimes) into `graphics/sprites.chr`.
   * Configure enemy stats, damage, and chasing movement.

4. **The Watchman Character (Player Sprites)**
   * Design the custom $16 \times 16$ player sprite (ushanka with fur flaps, LED goggles, dark top, jeans, boots).
   * Insert walking animation frames (Down, Up, Side) into `graphics/sprites.chr`.

---

## 6. Cover Art & Branding Ideas
* **Platform / Moniker Branding**: "famicom/samicom"
* **Hero Artwork**: Create a full-size sprite of Winter Watchman for cover art — use [lospec.com](https://lospec.com) for inspiration and palette references.


