# Tiled Tileset Reference Guide: *The Winter Watchman*

This document provides a layout and usage guide for editing levels in **Tiled** using `graphics/generated/tiles.png` and `levels/overworld.tmx`.

---

## 1. Engine & Grid System

* **Metatile Dimensions:** Each tileset square is **1 metatile ($16 \times 16$ pixels)**, composed of four NES hardware $8 \times 8$ tiles.
* **Screen Dimensions:** Each room is **16 metatiles wide by 12 metatiles tall** ($256 \times 192$ px of active playable space).
* **Overworld Grid:** The level map consists of an $8 \times 8$ grid of 64 rooms ($128 \times 96$ metatiles total).
* **Tiled Layers:**
  * **`Overworld Tiles`**: **ALWAYS draw on this layer.** This is the active background graphics layer parsed by `tmx2c`.
  * **`Sprites`**: Place enemies, NPCs, and spawn markers here.
  * **`Room Grid`**: An editor-only overlay guide (25% opacity) marking the 16×12 room borders. **Do not draw on this layer** (keep it locked).

---

## 2. Palette Banks in Tiled

`graphics/generated/tiles.png` is 128×512 pixels (8 columns wide × 32 rows tall). Palette 4 remains removed (cleared to solid black):

| Rows in Tiled | Tile IDs (GID) | Subpalette | Colors | Primary Use |
|:---:|:---:|:---:|:---|:---|
| **Rows 0 – 7** | **1 – 64** | **Palette 0** | Black (`$0F`), Carpet Gray (`$00`), Wood Amber (`$18`), Slate Teal (`$0C`) | **House Interior & Furnishings** |
| **Rows 8 – 15** | **65 – 128** | **Palette 1** | Black (`$0F`), Cold Blue (`$11`), Ice Cyan (`$21`), Snow White (`$30`) | **Overworld Exterior (Snow & Ice)** |
| **Rows 16 – 23** | **129 – 192** | **Palette 2** | Black (`$0F`), Black Seam (`$0F`), Desk Gray (`$00`), Desk Gray (`$00`) | **Floor Tile & Carpet (Matches Desk Background)** |
| **Rows 24 – 31** | **193 – 256** | **Palette 3** | Black (`$0F`, `$0F`, `$0F`, `$0F`) | *Removed / Black* |

---

## 3. Metatile Directory (Palette 0)

### Row 0: Outside Snow Ground & South Entrances
| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0 – 4** | `(0, 0)..(0, 4)` | **Overworld Snow Ground** | Exterior snow & ground tiles used across the overworld. |
| **5** | `(0, 5)` | **Cellar Hatch Trapdoor** | Wooden trapdoor with twin iron pull-rings. |
| **6** | `(0, 6)` | **Ladder for Trapdoor** | Wooden ladder rungs leading down into the cellar opening. |
| **7** | `(0, 7)` | **South Exterior Door** | Closed wooden exterior residential door for the south wall. |

---

### Row 1: House Perimeter Wall Set
*All walls slope inward toward the room floor, matching classic NES Zelda perspective.*

| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0** | `(1, 0)` | **Top-Left Corner** | North-West outer corner miter. |
| **1** | `(1, 1)` | **Top Wall (Wainscot)** | North wall with ceiling trim, slate teal drywall, chair rail, and wainscotting. |
| **2** | `(1, 2)` | **Top-Right Corner** | North-East outer corner miter. |
| **3** | `(1, 3)` | **Left Wall** | West perimeter wall with baseboard and outer trim. |
| **4** | `(1, 4)` | **Right Wall** | East perimeter wall with baseboard and outer trim. |
| **5** | `(1, 5)` | **Bottom-Left Corner** | South-West outer corner miter. |
| **6** | `(1, 6)` | **Bottom Wall** | South perimeter wall with baseboard and outer trim. |
| **7** | `(1, 7)` | **Bottom-Right Corner** | South-East outer corner miter. |

---

### Row 2: Interior Room Divider System
| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0** | `(2, 0)` | **Divider (Horizontal)** | Horizontal room partition. |
| **1** | `(2, 1)` | **Divider (Vertical)** | Vertical room partition. |
| **2** | `(2, 2)` | **Divider Corner TL** | 90° corner connecting South and East dividers. |
| **3** | `(2, 3)` | **Divider Corner TR** | 90° corner connecting South and West dividers. |
| **4** | `(2, 4)` | **Divider Corner BL** | 90° corner connecting North and East dividers. |
| **5** | `(2, 5)` | **Divider Corner BR** | 90° corner connecting North and West dividers. |
| **6** | `(2, 6)` | **Doorway Up/Down** | Open walk-through passage (North ↔ South). |
| **7** | `(2, 7)` | **Doorway Side/Side** | Open walk-through passage (West ↔ East). |

---

### Row 3: Kitchen Stove, PC Desk, Microwave & Window
*(Replaces placeholder dungeon brick walls / mountains)*

| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0** | `(3, 0)` | **Top Wall Window** | Top wall wainscot tile with inset glass window. |
| **1** | `(3, 1)` | **Microwave on Counter** | Compact kitchen microwave appliance on countertop. |
| **2 – 3** | `(3, 2)` & `(3, 3)` | **Kitchen Stove & Oven** | 32×16 stovetop range with burners, dials, and oven door. |
| **4 – 7** | `(3, 4)..(3, 7)` | **Modern PC Workstation Desk** | 32×32 retro PC desk: CRT monitor, keyboard, mouse, and wood drawers (TL, TR, BL, BR). |

---

### Row 4: Overworld Rocks, Dirt & Floor Tile
| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0 – 3** | `(4, 0)..(4, 3)` | **Overworld Boulders / Rocks** | 32×32 rock formation used across the overworld. |
| **4** | `(4, 4)` | **Dirt Waves** | Exterior terrain accent. |
| **5** | `(4, 5)` | **Ceramic Kitchen Floor Tile** | 4-tile ceramic grid with clean grout lines (checkered in Pal 0, desk gray in Pal 3). |
| **6 – 7** | `(4, 6)` & `(4, 7)` | **Dirt Edges** | Exterior ground trim. |

---

### Row 5: Refrigerator, Kitchen Sink, Wall Sconce & Big Window
| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0 – 3** | `(5, 0)..(5, 3)` | **32×32 Refrigerator** | Large refrigerator with freezer door, main door, handles, and feet (TL, TR, BL, BR). |
| **4 – 5** | `(5, 4)` & `(5, 5)` | **Kitchen Island Sink** | 32×16 island counter with clean countertop, faucet, sink basin, and cabinet doors. |
| **6** | `(5, 6)` | **Bottom Wall (Sconce)** | South exterior wall piece with an ornate brass wall sconce and glowing candle flame. |
| **7** | `(5, 7)` | **Bottom Wall (Big Window)** | South exterior wall piece with a large 4-pane wood-cased window and sturdy window sill. |

---

### Row 6: Dining Table, Chairs & Area Carpet
| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0 – 1** | `(6, 0)` & `(6, 1)` | **32×16 Dining Table** | Sturdy wood dining table with clean tabletop, legs, and floor gray bg (Left & Right halves). |
| **2** | `(6, 2)` | **Dining Chair (Front)** | Wood chair facing south with black outline for high contrast against gray floors. |
| **3 – 6** | `(6, 3)..(6, 6)` | **32×32 Area Carpet** | Geometric diamond weave rug with dark border (TL, TR, BL, BR). |
| **7** | `(6, 7)` | **Dining Chair (Side)** | Wood chair facing east/profile with black outline for high contrast against gray floors. |

---

### Row 7: HUD, Engine Icons & Free Slot
| Col | Coordinates | Tile Name | Description |
|:---:|:---:|:---|:---|
| **0** | `(7, 0)` | **Locked Door** | Dungeon barred keyhole door. |
| **1** | `(7, 1)` | **Open / Free Slot** | Unused slot. |
| **2 – 7** | `(7, 2)..(7, 7)` | **HUD Icons & Font** | Minimap borders, key, hearts, "Player" text label, numbers 0–9. |
