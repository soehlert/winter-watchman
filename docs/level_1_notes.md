# Level 1 Design Notes: The Watchman's House & The Cellar Secret

## 1. Overview & Narrative Flow
Level 1 begins inside the protagonist's home on a cold winter night before he steps out onto the dangerous, icy neighborhood sidewalks. 

The player has two paths to complete the level:
* **The Secret Shortcut (Recommended)**: Talk to the family, solve the pushable table puzzle to open the storm cellar trapdoor, grab the Ice Chopper early, and step outside fully armed.
* **The Hard Way**: Head straight outside into the freezing street unarmed, dodging aggressive ice hazards until finding a tool in the neighborhood.

---

## 2. Setting & Map Layout

### Screen 0: The House Interior
* **Visuals**: Cozy wooden floorboards, rug, furniture, and a front doorway leading outside.
* **Front Door**: Walking south through the front door transitions to **Screen 1** (the snowy driveway/street).
* **Pantry / Storage Corner**: A heavy wooden table (or crate) sits in the corner, concealing the cellar trapdoor.
* **NPCs**:
  1. **The Wife / Partner** (Lore connection: *"Actually it's mostly her, she asks me every day..."*):
     * Dialogue: *"You're going out there again in that costume? At least stay off the black ice, and try not to wake Mr. Wolkowski this time."*
  2. **The Kid / Roommate** (The Hint):
     * Dialogue: *"Dad says grandpa's old ice chopper is down in the storm cellar, but there's a heavy table pushed right over the floor hatch!"*

---

### The Secret: Pushable Table & Trapdoor
* **Mechanic**:
  * The table sits on top of a hidden trapdoor/stair tile.
  * When the player walks into the table and holds the direction for several frames (or pushes it), the table slides one tile aside with a wooden dragging sound.
  * The trapdoor/stairs tile is revealed underneath.
* **Warping to the Cellar**:
  * Stepping onto the revealed trapdoor triggers a fade transition:
    ```c
    playerOverworldPosition = 63; // Cellar screen index
    do_fade_screen_transition();
    ```

---

### Screen 63: The Storm Cellar (Basement)
* **Visuals**: Dark stone walls, earthen/concrete floor, cobwebs, and storage shelves.
* **The Reward**:
  * In the center of the room sits the **Ice Chopper** pickup sprite (`SPRITE_TYPE_ICE_CHOPPER`).
  * Walking over it triggers the pickup chime, despawns the sprite, and sets `hasIceChopper = 1`.
* **The Exit**:
  * A wooden ladder on the back wall. Stepping on it warps the player back up to the house (`playerOverworldPosition = 0`).

---

### Screen 1: The Snowy Street / Driveway
* **Visuals**: Snowdrifts, icy pavement, snow-covered cars, mailboxes, and outdoor landscaping.
* **Hazards**: Wandering and chasing ice slimes/hazards.
* **Objective**:
  * Use the Ice Chopper to defend against ice hazards.
  * Locate the Key dropped in the snowbank.
  * Unlock the neighborhood gate/door leading to Level 2.

---

## 3. Technical Implementation Details

### A. Screen Index Mapping
* The overworld is an $8 \times 8$ grid (64 screens total, indices `0` to `63`).
  * `Screen 0`: The House Interior (Player start position).
  * `Screen 1`: First outdoor street screen (east of Screen 0).
  * `Screen 63`: The Cellar (isolated off-grid screen at bottom-right of map).

### B. Pushable Table / Box Mechanic
Two technical approaches:
1. **Sprite-based Pushable Object (Standard)**:
   * Define `SPRITE_TYPE_PUSHABLE_BLOCK` in `sprite_definitions.h`.
   * When player collides with it from a given direction, increment a push timer. After ~16 frames, move the sprite 16 pixels away and change underlying map tile to revealed trapdoor.
2. **Tile-Swap Interaction**:
   * Inspect player facing direction. When pressing A (or walking into it), swap the table tile with the open hatch tile in the background buffer (`currentMap[]`).

### C. Ladder & Trapdoor Warps
* Uses the built-in `do_fade_screen_transition()` in `source/c/map/map.c`.
* Set cooldown timer `warpCooldownTime` to prevent immediate re-triggering upon arrival.
