#include "source/c/sprites/collision.h"
#include "source/c/library/bank_helpers.h"

CODE_BANK(PRG_BANK_SPRITE_COLLISION);
ZEROPAGE_DEF(unsigned char, collisionTemp);

// 64-tile collision lookup table (1 = solid obstacle, 0 = walkable floor)
const unsigned char TILE_COLLISION[64] = {
    // 0: default/floor (walkable), 1-6: ground/snow/floor (walkable), 7: doorway (walkable)
    0, 0, 0, 0, 0, 0, 0, 0,
    // 8-15: exterior walls & corners (solid)
    1, 1, 1, 1, 1, 1, 1, 1,
    // 16-23: interior room dividers & corners (solid)
    1, 1, 1, 1, 1, 1, 1, 1,
    // 24-31: rocks, trees, PC desk (solid)
    1, 1, 1, 1, 1, 1, 1, 1,
    // 32-35: rocks & pedestals (solid), 36-39: checkerboard floor (walkable)
    1, 1, 1, 1, 0, 0, 0, 0,
    // 40-45: kitchen appliances & counters (solid), 46: wall sconce (solid), 47: big window (solid)
    1, 1, 1, 1, 1, 1, 1, 1,
    // 48-50: dining table & chairs (solid), 51-54: area carpet (walkable), 55: side chair (solid)
    1, 1, 1, 0, 0, 0, 0, 1,
    // 56-63: extra floor / walkable
    0, 0, 0, 0, 0, 0, 0, 0
};

unsigned char test_collision(unsigned char tileId, unsigned char isPlayer) {
    (void)isPlayer;
    // The top 2 bits of the tile id are the palette color. We don't care about that here, so skip them.
    collisionTemp = tileId & 0x3f;
    return TILE_COLLISION[collisionTemp];
}