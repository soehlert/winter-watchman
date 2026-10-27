#include "source/c/neslib.h"
#include "source/c/sprites/player.h"
#include "source/c/library/bank_helpers.h"
#include "source/c/globals.h"
#include "source/c/map/map.h"
#include "source/c/configuration/game_states.h"
#include "source/c/configuration/system_constants.h"
#include "source/c/sprites/collision.h"
#include "source/c/sprites/sprite_definitions.h"
#include "source/c/sprites/map_sprites.h"
#include "source/c/menus/error.h"
#include "source/c/graphics/hud.h"
#include "source/c/graphics/game_text.h"
#include "source/c/sprites/map_sprites.h"

CODE_BANK(PRG_BANK_PLAYER_SPRITE);

// Some useful global variables
ZEROPAGE_DEF(int, playerXPosition);
ZEROPAGE_DEF(int, playerYPosition);
ZEROPAGE_DEF(int, playerXVelocity);
ZEROPAGE_DEF(int, playerYVelocity);
ZEROPAGE_DEF(int, nextPlayerXPosition);
ZEROPAGE_DEF(int, nextPlayerYPosition);
ZEROPAGE_DEF(unsigned char, playerControlsLockTime);
ZEROPAGE_DEF(unsigned char, playerInvulnerabilityTime);
ZEROPAGE_DEF(unsigned char, playerDirection);
ZEROPAGE_DEF(unsigned char, pickaxePosition);

// Huge pile of temporary variables
#define rawXPosition tempChar1
#define rawYPosition tempChar2
#define rawTileId tempChar3
#define collisionTempX tempChar4
#define collisionTempY tempChar5
#define collisionTempXRight tempChar6
#define collisionTempYBottom tempChar7

#define tempSpriteCollisionX tempInt1
#define tempSpriteCollisionY tempInt2

#define collisionTempXInt tempInt3
#define collisionTempYInt tempInt4

const unsigned char* wifeDialogue = 
    "Please be careful out there.  "
    "Whatever you do, stay away    "
    "from Mr. Wachowski's driveway."

    "He swore he'd call the cops.  "
    "if you went in his driveway!  "
    "You can't get arrested again! "

    "Your tools do come in handy.  "
    "Now if you can just remember  "
    "where you left any of them.   "

    "You'll want to have those.    "
    "Search the house before you   "
    "head out into the blizzard.   "
    ;

const unsigned char* kidDialogue = 
    "Stay safe out there, Dad!     "
    "The snow is piling up fast,   "
    "so don't stay out too late!   "

    "Hurry back home so we can     "
    "eat pizza like you promised!  "
    "I want pepperoni please!      "

    "Be careful in the deep snow.  "
    "Good luck, Winter Watchman!   "
    "                              "
    ;

const unsigned char* introductionText = 
    "At night the streets grow     " 
    "treacherous. Each step is     "
    "fraught with danger.          "

    "When I was only a child I     "
    "became one of the streets'    "
    "victims.                      "
    
    "Spiraling downward with no    "
    "foundation to hold me up.     "
    "                              "

    "I need to take matters        "
    "into my own hands. I need     "
    "to keep everyone else safe.   "
    ;

const unsigned char* movedText = 
    "You may not think of          "
    "yourself as a hero, but a lot "
    "of other people do!"
    ;

void trigger_intro_text(void) {
    trigger_game_text(introductionText);
}

// NOTE: This uses tempChar1 through tempChar3; the caller must not use these.
void update_player_sprite(void) {
    // Calculate the position of the player itself, then use these variables to build it up with 4 8x8 NES sprites.
    rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
    rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
    if (hasPlowTruck) {
        if (playerDirection == SPRITE_DIRECTION_DOWN) {
            rawTileId = 0x68;
        } else if (playerDirection == SPRITE_DIRECTION_UP) {
            rawTileId = 0x6a;
        } else if (playerDirection == SPRITE_DIRECTION_RIGHT) {
            rawTileId = 0x6c;
        } else {
            rawTileId = 0x6e;
        }
    } else {
        rawTileId = PLAYER_SPRITE_TILE_ID + playerDirection;

        if (playerXVelocity != 0 || playerYVelocity != 0) {
            // Does some math with the current NES frame to add either 2 or 0 to the tile id, animating the sprite.
            rawTileId += ((frameCount >> SPRITE_ANIMATION_SPEED_DIVISOR) & 0x01) << 1;
        }
    }

    // Clamp the player's sprite X Position to 0 to make sure we don't loop over, even if technically we have.
    if (rawXPosition > (SCREEN_EDGE_RIGHT + 4)) {
        rawXPosition = 0;
    }
    
    if (playerInvulnerabilityTime && frameCount & PLAYER_INVULNERABILITY_BLINK_MASK) {
        // If the player is invulnerable, we hide their sprite about half the time to do a flicker animation.
        oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, rawTileId, 0x00, PLAYER_SPRITE_INDEX);
        oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, rawTileId + 1, 0x00, PLAYER_SPRITE_INDEX+4);
        oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, rawTileId + 16, 0x00, PLAYER_SPRITE_INDEX+8);
        oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, rawTileId + 17, 0x00, PLAYER_SPRITE_INDEX+12);

    } else {
        oam_spr(rawXPosition, rawYPosition, rawTileId, 0x00, PLAYER_SPRITE_INDEX);
        oam_spr(rawXPosition + NES_SPRITE_WIDTH, rawYPosition, rawTileId + 1, 0x00, PLAYER_SPRITE_INDEX+4);
        oam_spr(rawXPosition, rawYPosition + NES_SPRITE_HEIGHT, rawTileId + 16, 0x00, PLAYER_SPRITE_INDEX+8);
        oam_spr(rawXPosition + NES_SPRITE_WIDTH, rawYPosition + NES_SPRITE_HEIGHT, rawTileId + 17, 0x00, PLAYER_SPRITE_INDEX+12);
    }

    if (pickaxePosition) {
        switch (playerDirection) {
            case SPRITE_DIRECTION_RIGHT:
                oam_spr(rawXPosition + pickaxePosition, rawYPosition + 6, PLAYER_PICKAXE_TILE_ID_H1, 0x00, PLAYER_WEAPON_OAM_LOCATION);
                oam_spr(rawXPosition + pickaxePosition + NES_SPRITE_WIDTH, rawYPosition + 6, PLAYER_PICKAXE_TILE_ID_H2, 0x00, PLAYER_WEAPON_OAM_LOCATION + 0x04);
                break;
            case SPRITE_DIRECTION_LEFT:
                oam_spr(rawXPosition - pickaxePosition, rawYPosition + 6, PLAYER_PICKAXE_TILE_ID_H1, OAM_FLIP_H, PLAYER_WEAPON_OAM_LOCATION);
                oam_spr(rawXPosition - pickaxePosition - NES_SPRITE_WIDTH, rawYPosition + 6, PLAYER_PICKAXE_TILE_ID_H2, OAM_FLIP_H, PLAYER_WEAPON_OAM_LOCATION + 0x04);
                break;
            case SPRITE_DIRECTION_DOWN:
                oam_spr(rawXPosition + 6, rawYPosition + pickaxePosition, PLAYER_PICKAXE_TILE_ID_V1, OAM_FLIP_V, PLAYER_WEAPON_OAM_LOCATION);
                oam_spr(rawXPosition + 6, rawYPosition + pickaxePosition + NES_SPRITE_HEIGHT, PLAYER_PICKAXE_TILE_ID_V2, OAM_FLIP_V, PLAYER_WEAPON_OAM_LOCATION + 0x04);
                break;
            case SPRITE_DIRECTION_UP:
                oam_spr(rawXPosition + 2, rawYPosition - pickaxePosition, PLAYER_PICKAXE_TILE_ID_V1, 0x00, PLAYER_WEAPON_OAM_LOCATION);
                oam_spr(rawXPosition + 2, rawYPosition - pickaxePosition - NES_SPRITE_HEIGHT, PLAYER_PICKAXE_TILE_ID_V2, 0x00, PLAYER_WEAPON_OAM_LOCATION + 0x04);
                break;
        }
    } else {
        oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, PLAYER_PICKAXE_TILE_ID_H1, 0x00, PLAYER_WEAPON_OAM_LOCATION);
        oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, PLAYER_PICKAXE_TILE_ID_H2, 0x00, PLAYER_WEAPON_OAM_LOCATION + 0x04);
    }

    if (saltTimer) {
        --saltTimer;
        // Flicker during the last ~0.5 seconds before the salt dissolves
        if (saltTimer > 30 || (frameCount & 0x02)) {
            oam_spr(saltX, saltY, PLAYER_SALT_TILE_ID, 0x01, PLAYER_SALT_OAM_LOCATION);
        } else {
            oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, 0, 0, PLAYER_SALT_OAM_LOCATION);
        }
    } else {
        oam_spr(SPRITE_OFFSCREEN, SPRITE_OFFSCREEN, 0, 0, PLAYER_SALT_OAM_LOCATION);
    }

}

void prepare_player_movement(void) {
    // Using a variable, so we can change the velocity based on pressing a button, having a special item,
    // or whatever you like!
    int maxVelocity = PLAYER_MAX_VELOCITY;
    lastControllerState = controllerState;
    controllerState = pad_poll(0);

    // If Start is pressed now, and was not pressed before
    if (controllerState & PAD_START && !(lastControllerState & PAD_START)) {
        gameState = GAME_STATE_PAUSED;
        return;
    }

    // Swing the pickaxe when B is pressed (if the player has it and isn't already mid-swing)
    if (hasPickaxe && !hasPlowTruck && pickaxePosition == 0 && (controllerState & PAD_B) && !(lastControllerState & PAD_B)) {
        pickaxePosition = PLAYER_PICKAXE_POSITION_FULLY_EXTENDED;
    } else if (pickaxePosition != 0) {
        pickaxePosition -= 2;
    }

    // If A is pressed now, and was not pressed before
    if (controllerState & PAD_A && !(lastControllerState & PAD_A)) {

        // 1. Reveal the hatch if hidden on Screen 0:
        if (!isCellarHatchRevealed && playerOverworldPosition == 0) {
            rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
            rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
            if (rawXPosition >= 152 && rawXPosition <= 184 && rawYPosition >= 184 && rawYPosition <= 208) {
                isCellarHatchRevealed = 1;
                currentMap[154] = 56; // Set hatch collision and tile in RAM

                // Send the 4 tiles of the open wooden hatch into PPU VRAM Nametable A
                screenBuffer[0] = MSB(NAMETABLE_A + 0x0254) | NT_UPD_HORZ;
                screenBuffer[1] = LSB(NAMETABLE_A + 0x0254);
                screenBuffer[2] = 2;
                screenBuffer[3] = 224; // Top-left hatch tile
                screenBuffer[4] = 225; // Top-right hatch tile
                screenBuffer[5] = MSB(NAMETABLE_A + 0x0274) | NT_UPD_HORZ;
                screenBuffer[6] = LSB(NAMETABLE_A + 0x0274);
                screenBuffer[7] = 2;
                screenBuffer[8] = 240; // Bottom-left hatch tile
                screenBuffer[9] = 241; // Bottom-right hatch tile
                screenBuffer[10] = NT_UPD_EOF;
                set_vram_update(screenBuffer);
                ppu_wait_nmi();
                set_vram_update(NULL);

                sfx_play(SFX_KEY, SFX_CHANNEL_3); // Chime
                trigger_game_text("You pull back the rug and     "
                                  "discover a hidden cellar      "
                                  "trapdoor!                     ");
                playerXVelocity = 0;
                playerYVelocity = 0;
                return;
            }
        }

        // 2. If the hatch is already revealed, pressing A descends into Screen 62:
        if (isCellarHatchRevealed && playerOverworldPosition == 0) {
            rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
            rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
            if (rawXPosition >= 152 && rawXPosition <= 184 && rawYPosition >= 184 && rawYPosition <= 208) {
                playerOverworldPosition = 62; // Warp to Screen 62
                playerXPosition = (48 << PLAYER_POSITION_SHIFT);
                playerYPosition = (80 << PLAYER_POSITION_SHIFT);
                playerXVelocity = 0;
                playerYVelocity = 0;
                banked_call(PRG_BANK_MAP_LOGIC, do_fade_screen_transition);
                return;
            }
        }

        // 3. If in the cellar on Screen 62, pressing A near the ladder climbs back to House:
        if (playerOverworldPosition == 62) {
            rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
            rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
            // Ladder is at Column 1 (X: 16-31, Y: 64-79)
            if (rawXPosition >= 12 && rawXPosition <= 40 && rawYPosition >= 60 && rawYPosition <= 100) {
                playerOverworldPosition = 0; // Warp back to House
                playerXPosition = (168 << PLAYER_POSITION_SHIFT);
                playerYPosition = (200 << PLAYER_POSITION_SHIFT);
                playerXVelocity = 0;
                playerYVelocity = 0;
                banked_call(PRG_BANK_MAP_LOGIC, do_fade_screen_transition);
                return;
            }
        }

        // 4. Check if player is talking to an NPC:
        for (i = 0; i < MAP_MAX_SPRITES; ++i) {
            currentMapSpriteIndex = i << MAP_SPRITE_DATA_SHIFT;
            if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_TYPE] == SPRITE_TYPE_NPC) {
                tempSpriteCollisionX = ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_X]) + ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_X + 1]) << 8)) >> PLAYER_POSITION_SHIFT;
                tempSpriteCollisionY = ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_Y]) + ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_Y + 1]) << 8)) >> PLAYER_POSITION_SHIFT;
                collisionTempXInt = (playerXPosition >> PLAYER_POSITION_SHIFT) - tempSpriteCollisionX;
                collisionTempYInt = (playerYPosition >> PLAYER_POSITION_SHIFT) - tempSpriteCollisionY;

                if (ABS(collisionTempXInt) <= 24 && ABS(collisionTempYInt) <= 24) {
                    if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] == 1) {
                        trigger_game_text(wifeDialogue);
                    } else if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] == 2) {
                        trigger_game_text(kidDialogue);
                    } else if (playerOverworldPosition == 0) {
                        trigger_game_text(introductionText);
                    } else {
                        trigger_game_text(movedText);
                    }
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    return;
                }
            }
        }

        // 5. Drop Salt behind the player if they have the Salt Shaker and no salt is currently active:
        if (hasSalt && saltTimer == 0) {
            rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
            rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
            switch (playerDirection) {
                case SPRITE_DIRECTION_RIGHT:
                    saltX = rawXPosition - 10;
                    saltY = rawYPosition + 6;
                    break;
                case SPRITE_DIRECTION_LEFT:
                    saltX = rawXPosition + 18;
                    saltY = rawYPosition + 6;
                    break;
                case SPRITE_DIRECTION_DOWN:
                    saltX = rawXPosition + 4;
                    saltY = rawYPosition - 8;
                    break;
                case SPRITE_DIRECTION_UP:
                default:
                    saltX = rawXPosition + 4;
                    saltY = rawYPosition + 18;
                    break;
            }
            saltTimer = SALT_DURATION;
            sfx_play(SFX_HURT, SFX_CHANNEL_2);
            if (playerOverworldPosition >= 16 && --saltCount == 0) {
                hasSalt = 0;
            }
        }
    }

    if (playerControlsLockTime) {
        // If your controls are locked, just tick down the timer until they stop being locked. Don't read player input.
        playerControlsLockTime--;
    } else {
        if (controllerState & PAD_RIGHT && playerXVelocity >= (0 - PLAYER_VELOCITY_NUDGE)) {
            // If you're moving right, and you're not at max, speed up.
            if (playerXVelocity < maxVelocity) {
                playerXVelocity += PLAYER_VELOCITY_ACCEL;
            // If you're over max somehow, we'll slow you down a little.
            } else if (playerXVelocity > maxVelocity) {
                playerXVelocity -= PLAYER_VELOCITY_ACCEL;
            }
        } else if (controllerState & PAD_LEFT && playerXVelocity <= (0 + PLAYER_VELOCITY_NUDGE)) {
            // If you're moving left, and you're not at max, speed up.
            if (ABS(playerXVelocity) < maxVelocity) {
                playerXVelocity -= PLAYER_VELOCITY_ACCEL;
            // If you're over max, slow you down a little...
            } else if (ABS(playerXVelocity) > maxVelocity) { 
                playerXVelocity += PLAYER_VELOCITY_ACCEL;
            }
        } else if (playerXVelocity != 0) {
            // Not pressing anything? Let's slow you back down...
            if (playerXVelocity > 0) {
                playerXVelocity -= PLAYER_VELOCITY_ACCEL;
            } else {
                playerXVelocity += PLAYER_VELOCITY_ACCEL;
            }
        }

        if (controllerState & PAD_UP && playerYVelocity <= (0 + PLAYER_VELOCITY_NUDGE)) {
            if (ABS(playerYVelocity) < maxVelocity) {
                playerYVelocity -= PLAYER_VELOCITY_ACCEL;
            } else if (ABS(playerYVelocity) > maxVelocity) {
                playerYVelocity += PLAYER_VELOCITY_ACCEL;
            }
        } else if (controllerState & PAD_DOWN && playerYVelocity >= (0 - PLAYER_VELOCITY_NUDGE)) {
            if (playerYVelocity < maxVelocity) {
                playerYVelocity += PLAYER_VELOCITY_ACCEL;
            } else if (playerYVelocity > maxVelocity) {
                playerYVelocity -= PLAYER_VELOCITY_ACCEL;
            }
        } else { 
            if (playerYVelocity > 0) {
                playerYVelocity -= PLAYER_VELOCITY_ACCEL;
            } else if (playerYVelocity < 0) {
                playerYVelocity += PLAYER_VELOCITY_ACCEL;
            }
        }
        
        // Now, slowly adjust to the grid as possible, if the player isn't pressing a direction. 
        #if PLAYER_MOVEMENT_STYLE == MOVEMENT_STYLE_GRID
            if (playerYVelocity != 0 && playerXVelocity == 0 && (controllerState & (PAD_LEFT | PAD_RIGHT)) == 0) {
                if ((char)((playerXPosition + PLAYER_POSITION_TILE_MASK_MIDDLE) & PLAYER_POSITION_TILE_MASK) > (char)(PLAYER_POSITION_TILE_MASK_MIDDLE)) {
                    playerXVelocity = 0-PLAYER_VELOCITY_NUDGE;
                } else if ((char)((playerXPosition + PLAYER_POSITION_TILE_MASK_MIDDLE) & PLAYER_POSITION_TILE_MASK) < (char)(PLAYER_POSITION_TILE_MASK_MIDDLE)) {
                    playerXVelocity = PLAYER_VELOCITY_NUDGE;
                } // If equal, do nothing. That's our goal.
            }

            if (playerXVelocity != 0 && playerYVelocity == 0 && (controllerState & (PAD_UP | PAD_DOWN)) == 0) {
                if ((char)((playerYPosition + PLAYER_POSITION_TILE_MASK_MIDDLE + PLAYER_POSITION_TILE_MASK_EXTRA_NUDGE) & PLAYER_POSITION_TILE_MASK) > (char)(PLAYER_POSITION_TILE_MASK_MIDDLE)) {
                    playerYVelocity = 0-PLAYER_VELOCITY_NUDGE;
                } else if ((char)((playerYPosition + PLAYER_POSITION_TILE_MASK_MIDDLE + PLAYER_POSITION_TILE_MASK_EXTRA_NUDGE) & PLAYER_POSITION_TILE_MASK) < (char)(PLAYER_POSITION_TILE_MASK_MIDDLE)) {
                    playerYVelocity = PLAYER_VELOCITY_NUDGE;
                } // If equal, do nothing. That's our goal.
            }
        #endif

    }

    // While we're at it, tick down the invulnerability timer if needed
    if (playerInvulnerabilityTime) {
        playerInvulnerabilityTime--;
    }

    nextPlayerXPosition = playerXPosition + playerXVelocity;
    nextPlayerYPosition = playerYPosition + playerYVelocity;

}

void do_player_movement(void) {

    // This will knock out the player's speed if they hit anything.
    test_player_tile_collision();
    // If the new player position hit any sprites, we'll find that out and knock it out here.
    handle_player_sprite_collision();

    playerXPosition += playerXVelocity;
    playerYPosition += playerYVelocity;

    rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
    rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
    // The X Position has to wrap to allow us to snap to the grid. This makes us shift when you wrap around to 255, down to 240-ish
    if (rawXPosition > (SCREEN_EDGE_RIGHT+4) && rawXPosition < SCREEN_EDGE_LEFT_WRAPPED) {
        // We use sprite direction to determine which direction to scroll in, so be sure this is set properly.
        if (playerInvulnerabilityTime || pickaxePosition) {
            playerXPosition -= playerXVelocity;
            rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
        } else {
            playerDirection = SPRITE_DIRECTION_LEFT;
            gameState = GAME_STATE_SCREEN_SCROLL;
            saltTimer = 0;
            bossSpearTimer = 0;
            snowballTimer = 0;
            playerOverworldPosition--;
        }
    } else if (rawXPosition > SCREEN_EDGE_RIGHT && rawXPosition < (SCREEN_EDGE_RIGHT+4)) {
        if (playerInvulnerabilityTime || pickaxePosition) {
            playerXPosition -= playerXVelocity;
            rawXPosition = (playerXPosition >> PLAYER_POSITION_SHIFT);
        } else {
            playerDirection = SPRITE_DIRECTION_RIGHT;
            gameState = GAME_STATE_SCREEN_SCROLL;
            saltTimer = 0;
            bossSpearTimer = 0;
            snowballTimer = 0;
            playerOverworldPosition++;
        }
    } else if (rawYPosition > SCREEN_EDGE_BOTTOM) {
        if (playerInvulnerabilityTime || pickaxePosition) {
            playerYPosition -= playerYVelocity;
            rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
        } else {
            playerDirection = SPRITE_DIRECTION_DOWN;
            gameState = GAME_STATE_SCREEN_SCROLL;
            saltTimer = 0;
            bossSpearTimer = 0;
            snowballTimer = 0;
            playerOverworldPosition += 8;
        }
    } else if (rawYPosition < SCREEN_EDGE_TOP) {
        if (playerInvulnerabilityTime || pickaxePosition) {
            playerYPosition -= playerYVelocity;
            rawYPosition = (playerYPosition >> PLAYER_POSITION_SHIFT);
        } else {
            playerDirection = SPRITE_DIRECTION_UP;
            gameState = GAME_STATE_SCREEN_SCROLL;
            saltTimer = 0;
            bossSpearTimer = 0;
            snowballTimer = 0;
            playerOverworldPosition -= 8;
        }
    }
}

void test_player_tile_collision(void) {

    if (playerYVelocity != 0) {
        collisionTempYInt = playerYPosition + PLAYER_Y_OFFSET_EXTENDED + playerYVelocity;
        collisionTempXInt = playerXPosition + PLAYER_X_OFFSET_EXTENDED;

        collisionTempY = ((collisionTempYInt) >> PLAYER_POSITION_SHIFT) - HUD_PIXEL_HEIGHT;
        collisionTempX = ((collisionTempXInt) >> PLAYER_POSITION_SHIFT);

        collisionTempYInt += PLAYER_HEIGHT_EXTENDED;
        collisionTempXInt += PLAYER_WIDTH_EXTENDED;

        collisionTempYBottom = ((collisionTempYInt) >> PLAYER_POSITION_SHIFT) - HUD_PIXEL_HEIGHT;
        collisionTempXRight = (collisionTempXInt) >> PLAYER_POSITION_SHIFT;
        
        // Due to how we are calculating the sprite's position, there is a slight chance we can exceed the area that our
        // map takes up near screen edges. To compensate for this, we clamp the Y position of our character to the 
        // window between 0 and 192 pixels, which we can safely test collision within.

        // If collisionTempY is > 240, it can be assumed we calculated a position less than zero, and rolled over to 255
        if (collisionTempY > 240) {
            collisionTempY = 0;
        }
        // The lowest spot we can test collision is at pixel 192 (the 12th 16x16 tile). If we're past that, bump ourselves back.
        if (collisionTempYBottom > 190) { 
            collisionTempYBottom = 190;
        }

        if (playerYVelocity < 0) {
            // We're going up - test the top left, and top right
            if (test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempX, collisionTempY)], 1) || test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempXRight, collisionTempY)], 1)) {
                playerYVelocity = 0;
                playerControlsLockTime = 0;
            }
            if (!(playerControlsLockTime | pickaxePosition) && playerYVelocity < (0-PLAYER_VELOCITY_NUDGE)) {
                playerDirection = SPRITE_DIRECTION_UP;
            }
        } else {
            // Okay, we're going down - test the bottom left and bottom right
            if (test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempX, collisionTempYBottom)], 1) || test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempXRight, collisionTempYBottom)], 1)) {
                playerYVelocity = 0;
                playerControlsLockTime = 0;

            }
            if (!(playerControlsLockTime | pickaxePosition) && playerYVelocity > PLAYER_VELOCITY_NUDGE) {
                playerDirection = SPRITE_DIRECTION_DOWN;
            }
        }
    }

    if (playerXVelocity != 0) {
        collisionTempXInt = playerXPosition + PLAYER_X_OFFSET_EXTENDED + playerXVelocity;
        collisionTempYInt = playerYPosition + PLAYER_Y_OFFSET_EXTENDED + playerYVelocity;
        collisionTempX = (collisionTempXInt) >> PLAYER_POSITION_SHIFT;
        collisionTempY = ((collisionTempYInt) >> PLAYER_POSITION_SHIFT) - HUD_PIXEL_HEIGHT;

        collisionTempXInt += PLAYER_WIDTH_EXTENDED;
        collisionTempYInt += PLAYER_HEIGHT_EXTENDED;

        collisionTempYBottom = ((collisionTempYInt) >> PLAYER_POSITION_SHIFT) - HUD_PIXEL_HEIGHT;
        collisionTempXRight = ((collisionTempXInt) >> PLAYER_POSITION_SHIFT);
        // The lowest spot we can test collision is at pixel 192 (the 12th 16x16 tile). If we're past that, bump ourselves back.
        if (collisionTempYBottom > 190) {
            collisionTempYBottom = 190;
        }



        // Depending on how far to the left/right the player is, there's a chance part of our bounding box falls into
        // the next column of tiles on the opposite side of the screen. This if statement prevents those collisions.
        if (collisionTempX > 2 && collisionTempX < 238) {
            if (playerXVelocity < 0) {
                // Okay, we're moving left. Need to test the top-left and bottom-left
                if (test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempX, collisionTempY)], 1) || test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempX, collisionTempYBottom)], 1)) {
                    playerXVelocity = 0;
                    playerControlsLockTime = 0;

                }
                if (!(playerControlsLockTime | pickaxePosition) && playerXVelocity < (0-PLAYER_VELOCITY_NUDGE)) {
                    playerDirection = SPRITE_DIRECTION_LEFT;
                }
            } else {
                // Going right - need to test top-right and bottom-right
                if (test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempXRight, collisionTempY)], 1) || test_collision(currentMap[PLAYER_MAP_POSITION(collisionTempXRight, collisionTempYBottom)], 1)) {
                    playerXVelocity = 0;
                    playerControlsLockTime = 0;

                }
                if (!(playerControlsLockTime | pickaxePosition) && playerXVelocity > PLAYER_VELOCITY_NUDGE) {
                    playerDirection = SPRITE_DIRECTION_RIGHT;
                }
            }
        }
    }
}

void use_pickaxe_durability(void) {
    if (playerOverworldPosition < 16) {
        return;
    }
    if (pickaxeDurability > 0) {
        --pickaxeDurability;
        if (pickaxeDurability == 0) {
            hasPickaxe = 0;
            sfx_play(SFX_HURT, SFX_CHANNEL_2);
        }
    }
}

void handle_player_sprite_collision(void) {
    // Handle pickaxe weapon hits first
    if (lastPlayerWeaponCollisionId != NO_SPRITE_HIT) {
        currentMapSpriteIndex = lastPlayerWeaponCollisionId << MAP_SPRITE_DATA_SHIFT;

        switch (currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE]) {
            case SPRITE_TYPE_HEALTH:
            case SPRITE_TYPE_KEY:
            case SPRITE_TYPE_DOOR:
                lastPlayerSpriteCollisionId = lastPlayerWeaponCollisionId;
                break;

            case SPRITE_TYPE_BREAKABLE:
                if (!hasPickaxe || currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                    break;
                }
                sfx_play(SFX_HURT, SFX_CHANNEL_2);
                use_pickaxe_durability();
                if (--currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] == 0) {
                    if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_DAMAGE] == 1) {
                        // Transform the broken ice block in-place into a Collectible!
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_COLLECTIBLE;
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TILE_ID] = 0x2a;
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_SIZE_PALETTE] = SPRITE_SIZE_16PX_16PX | SPRITE_PALETTE_0;
                    } else {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                        currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerWeaponCollisionId];
                    }
                    // If a solid background wall tile was underneath this breakable sprite, replace it with walkable floor/snow
                    rawXPosition = currentMap[MAP_DATA_TILE_LENGTH + (lastPlayerWeaponCollisionId << 1)];
                    if (rawXPosition < 192 && (currentMap[rawXPosition] & 0x3f) == 63) {
                        currentMap[rawXPosition] = 67;
                    } else if (rawXPosition < 192 && (currentMap[rawXPosition] & 0x3f) >= 8) {
                        tempSpriteCollisionX = NAMETABLE_A + ((rawXPosition & 0xf0) << 2) + ((rawXPosition & 0x0f) << 1);
                        screenBuffer[0] = MSB(tempSpriteCollisionX) | NT_UPD_HORZ;
                        screenBuffer[1] = LSB(tempSpriteCollisionX);
                        screenBuffer[2] = 2;
                        tempSpriteCollisionX += 0x20;
                        screenBuffer[5] = MSB(tempSpriteCollisionX) | NT_UPD_HORZ;
                        screenBuffer[6] = LSB(tempSpriteCollisionX);
                        screenBuffer[7] = 2;
                        if (playerOverworldPosition == 0) {
                            currentMap[rawXPosition] = 4; // Indoor grey floor
                            screenBuffer[3] = 100;
                            screenBuffer[4] = 100;
                            screenBuffer[8] = 100;
                            screenBuffer[9] = 100;
                        } else {
                            currentMap[rawXPosition] = 67; // Outdoor snow (Tiled GID 68)
                            screenBuffer[3] = 6;
                            screenBuffer[4] = 7;
                            screenBuffer[8] = 22;
                            screenBuffer[9] = 23;
                        }
                        screenBuffer[10] = NT_UPD_EOF;
                        set_vram_update(screenBuffer);
                        ppu_wait_nmi();
                        set_vram_update(NULL);
                    }
                } else {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 20;
                }
                break;

            case SPRITE_TYPE_SNOWDRIFT:
                if (currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                    break;
                }
                if ((playerOverworldPosition & 0x07) >= 4) {
                    if (!hasPlowTruck) {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 20;
                        trigger_game_text("This roadblock is way too     "
                                          "heavy! You need the Plow      "
                                          "Truck to ram through it!      ");
                        break;
                    }
                } else if (playerOverworldPosition >= 32) {
                    if (!hasSnowblower) {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 20;
                        trigger_game_text("This snowbank is packed solid!"
                                          "A shovel won't cut it. You    "
                                          "need the Snowblower!          ");
                        break;
                    }
                } else if (!hasShovel) {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 20;
                    trigger_game_text("This snowdrift is too deep    "
                                      "for a pickaxe! You need a     "
                                      "Snow Shovel to clear it!      ");
                    break;
                }
                if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] > 0) {
                    sfx_play(SFX_HURT, SFX_CHANNEL_2);
                    if (--currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] == 0) {
                        // 2nd hit cleared the Snowdrift! Hide its graphics, leaving an invisible walk-into warp spot!
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TILE_ID] = SPRITE_TILE_ID_OFFSCREEN;
                    } else {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 20;
                    }
                }
                break;

            case SPRITE_TYPE_REGULAR_ENEMY:
                if (!hasPickaxe || currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                    break;
                }
                if (--currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] == 0) {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerWeaponCollisionId];
                } else {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = SPRITE_INVULNERABILITY_TIME;
                    currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_CURRENT_DIRECTION] = playerDirection;
                    currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_DIRECTION_TIME] = SPRITE_INVULNERABILITY_TIME;
                }
                use_pickaxe_durability();
                break;

            case SPRITE_TYPE_BOSS:
                // Jack Frost can ONLY be damaged by the Pickaxe while salt-stunned (INVULN_COUNTDOWN > 30)
                if (!hasPickaxe || currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] <= 30) {
                    break;
                }
                sfx_play(SFX_HURT, SFX_CHANNEL_2);
                use_pickaxe_durability();
                // Knock Jack Frost backward away from the player for 30 frames (during which he cannot hurt the player)
                currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 30;
                currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_CURRENT_DIRECTION] = playerDirection;
                currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_DIRECTION_TIME] = 16;
                playerInvulnerabilityTime = 24;
                if (bossHealth > 0) {
                    --bossHealth;
                }
                if (bossHealth == 0) {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerWeaponCollisionId];
                    bossSpearTimer = 0;
                    snowballTimer = 0;
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    trigger_game_text("You shattered Jack Frost's    "
                                      "icy grip and saved the town   "
                                      "from the eternal blizzard!    ");
                }
                break;
        }
    }

    // We store the last sprite hit when we update the sprites in `map_sprites.c`, so here all we have to do is react to it.
    if (lastPlayerSpriteCollisionId != NO_SPRITE_HIT) {
        currentMapSpriteIndex = lastPlayerSpriteCollisionId<<MAP_SPRITE_DATA_SHIFT;
        switch (currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE]) {
            case SPRITE_TYPE_HEALTH:
                // This if statement ensures that we don't remove hearts if you don't need them yet.
                if (playerHealth < playerMaxHealth) {
                    playerHealth += currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH];
                    if (playerHealth > playerMaxHealth) {
                        playerHealth = playerMaxHealth;
                    }
                    // Hide the sprite now that it has been taken.
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;

                    // Play the heart sound!
                    sfx_play(SFX_HEART, SFX_CHANNEL_3);

                    // Mark the sprite as collected, so we can't get it again.
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                }
                break;
            case SPRITE_TYPE_KEY:
                if (playerKeyCount < MAX_KEY_COUNT) {
                    playerKeyCount++;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;

                    sfx_play(SFX_KEY, SFX_CHANNEL_3);

                    // Mark the sprite as collected, so we can't get it again.
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                }
                break;
            case SPRITE_TYPE_COLLECTIBLE:
                if (playerCollectibleCount < 9) {
                    playerCollectibleCount++;
                }
                currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                sfx_play(SFX_KEY, SFX_CHANNEL_3);
                currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                break;
            case SPRITE_TYPE_BOSS:
            case SPRITE_TYPE_REGULAR_ENEMY:
            case SPRITE_TYPE_INVULNERABLE_ENEMY:
                if (hasPlowTruck && currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_TYPE] != SPRITE_TYPE_BOSS) {
                    currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                    sfx_play(SFX_HURT, SFX_CHANNEL_2);
                    break;
                }
                if (playerInvulnerabilityTime ||
                    (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_TYPE] != SPRITE_TYPE_BOSS &&
                     currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN])) {
                    return;
                }
                playerHealth -= currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_DAMAGE]; 
                // Since playerHealth is unsigned, we need to check for wraparound damage. 
                // NOTE: If something manages to do more than 16 damage at once, this might fail.
                if (playerHealth == 0 || playerHealth > 240) {
                    gameState = GAME_STATE_GAME_OVER;
                    music_stop();
                    sfx_play(SFX_GAMEOVER, SFX_CHANNEL_1);
                    return;
                }
                // Knock the player back
                playerControlsLockTime = PLAYER_DAMAGE_CONTROL_LOCK_TIME;
                playerInvulnerabilityTime = PLAYER_DAMAGE_INVULNERABILITY_TIME;
                if (playerDirection == SPRITE_DIRECTION_LEFT) {
                    // Punt them back in the opposite direction
                    playerXVelocity = PLAYER_MAX_VELOCITY;
                    // Reverse their velocity in the other direction, too.
                    playerYVelocity = 0 - playerYVelocity;
                } else if (playerDirection == SPRITE_DIRECTION_RIGHT) {
                    playerXVelocity = 0-PLAYER_MAX_VELOCITY;
                    playerYVelocity = 0 - playerYVelocity;
                } else if (playerDirection == SPRITE_DIRECTION_DOWN) {
                    playerYVelocity = 0-PLAYER_MAX_VELOCITY;
                    playerXVelocity = 0 - playerXVelocity;
                } else { // Make being thrown downward into a catch-all, in case your direction isn't set or something.
                    playerYVelocity = PLAYER_MAX_VELOCITY;
                    playerXVelocity = 0 - playerXVelocity;
                }
                sfx_play(SFX_HURT, SFX_CHANNEL_2);

                
                break;
            case SPRITE_TYPE_DOOR: 
                // Doors without locks are very simple - they just open! Hide the sprite until the user comes back
                // note that we intentionally *don't* store this state, so it comes back next time.
                currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                break;
            case SPRITE_TYPE_LOCKED_DOOR:
                // First off, do you have a key? If so, let's just make this go away...
                if (playerKeyCount > 0) {
                    playerKeyCount--;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;

                    // Mark the door as gone, so it doesn't come back.
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                    rawXPosition = currentMap[MAP_DATA_TILE_LENGTH + (lastPlayerSpriteCollisionId << 1)];
                    if (rawXPosition < 192) {
                        currentMap[rawXPosition] = 67;
                    }
                    sfx_play(SFX_KEY, SFX_CHANNEL_3);
                    break;
                }
                // So you don't have a key.
                playerXVelocity = 0;
                playerYVelocity = 0;
                playerControlsLockTime = 0;
                if (!currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 30;
                    trigger_game_text("The garage is locked! You     "
                                      "need to find a key to open it!");
                }
                break;
            case SPRITE_TYPE_BREAKABLE:
                if (hasPlowTruck) {
                    sfx_play(SFX_HURT, SFX_CHANNEL_2);
                    if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_DAMAGE] == 1) {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_COLLECTIBLE;
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TILE_ID] = 0x2a;
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_SIZE_PALETTE] = SPRITE_SIZE_16PX_16PX | SPRITE_PALETTE_0;
                    } else {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                        currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                    }
                    rawXPosition = currentMap[MAP_DATA_TILE_LENGTH + (lastPlayerSpriteCollisionId << 1)];
                    if (rawXPosition < 192) {
                        currentMap[rawXPosition] = 67;
                    }
                    break;
                }
                playerControlsLockTime = 0;
                tempSpriteCollisionX = ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_X]) + ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_X + 1]) << 8)) + 16;
                tempSpriteCollisionY = ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_Y]) + ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_Y + 1]) << 8)) + 32;
                if ((playerYPosition + PLAYER_Y_OFFSET_EXTENDED) < tempSpriteCollisionY + 224 &&
                    (playerYPosition + PLAYER_Y_OFFSET_EXTENDED + PLAYER_HEIGHT_EXTENDED) > tempSpriteCollisionY) {
                    playerXVelocity = 0;
                }
                if ((playerXPosition + PLAYER_X_OFFSET_EXTENDED) < tempSpriteCollisionX + 224 &&
                    (playerXPosition + PLAYER_X_OFFSET_EXTENDED + PLAYER_WIDTH_EXTENDED) > tempSpriteCollisionX) {
                    playerYVelocity = 0;
                }
                break;
            case SPRITE_TYPE_SNOWDRIFT:
                if (hasPlowTruck ||
                    ((playerOverworldPosition & 0x07) < 4 && ((playerOverworldPosition < 32 && hasShovel) || (playerOverworldPosition >= 32 && hasSnowblower))) ||
                    currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] == 0) {
                    // The snowdrift was cleared (or rammed with the Plow Truck)! Warp to the next level!
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                    pickaxePosition = 0;
                    saltTimer = 0;
                    if ((playerOverworldPosition & 0x07) >= 4) {
                        // Completed Level 3 -> Warp to the Final Boss Area far-left (Screen 5, Row 0 Col 5)
                        hasPlowTruck = 0;
                        if (hasPickaxe) {
                            pickaxeDurability = 9;
                        }
                        if (hasSalt) {
                            saltCount = 9;
                        }
                        playerOverworldPosition = 5;
                        playerXPosition = (16 << PLAYER_POSITION_SHIFT);
                        playerYPosition = (144 << PLAYER_POSITION_SHIFT);
                        playerDirection = SPRITE_DIRECTION_RIGHT;
                        playerXVelocity = 0;
                        playerYVelocity = 0;
                        banked_call(PRG_BANK_MAP_LOGIC, do_fade_screen_transition);
                        trigger_game_text("You plow straight through the "
                                          "roadblock and step out for    "
                                          "the final showdown!           ");
                    } else if (playerOverworldPosition < 32) {
                        // Completed Level 1 -> Warp to Level 2 (Screen 49)
                        playerOverworldPosition = 49;
                        playerXPosition = (32 << PLAYER_POSITION_SHIFT);
                        playerYPosition = (64 << PLAYER_POSITION_SHIFT);
                        playerXVelocity = 0;
                        playerYVelocity = 0;
                        banked_call(PRG_BANK_MAP_LOGIC, do_fade_screen_transition);
                        trigger_game_text("You step through the cleared  "
                                          "snowdrift and arrive at       "
                                          "the next block!               ");
                    } else {
                        // Completed Level 2 -> Warp to Level 3 bottom-right (Screen 47, Row 5 Col 7)
                        playerOverworldPosition = 47;
                        playerXPosition = (128 << PLAYER_POSITION_SHIFT);
                        playerYPosition = (96 << PLAYER_POSITION_SHIFT);
                        playerXVelocity = 0;
                        playerYVelocity = 0;
                        banked_call(PRG_BANK_MAP_LOGIC, do_fade_screen_transition);
                        trigger_game_text("You clear the packed snowbank "
                                          "with the Snowblower and reach "
                                          "the next neighborhood!        ");
                    }
                    return;
                }
                playerXVelocity = 0;
                playerYVelocity = 0;
                playerControlsLockTime = 0;
                break;
            case SPRITE_TYPE_PICKAXE:
                if (currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 8;
                    break;
                }
                rawXPosition = saltCount;
                if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] > 0) {
                    pickaxeDurability = currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH];
                } else {
                    pickaxeDurability = PICKAXE_MAX_DURABILITY;
                }
                hasPickaxe = 1;
                if (hasSalt && playerOverworldPosition >= 16) {
                    // Swap in Levels 1-3: drop the Salt Shaker (with its remaining throws) on the ground in this spot!
                    hasSalt = 0;
                    saltCount = 0;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_SALT_ITEM;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TILE_ID] = 0x4c;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_SIZE_PALETTE] = SPRITE_SIZE_16PX_16PX | SPRITE_PALETTE_1;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_HEALTH] = rawXPosition;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 15;
                } else {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                }
                banked_call(PRG_BANK_MAP_SPRITES, update_map_sprites);
                sfx_play(SFX_KEY, SFX_CHANNEL_3);
                if (!(seenWeaponPopups & 0x01)) {
                    seenWeaponPopups |= 0x01;
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    trigger_game_text("You equipped the Ice Pickaxe! "
                                      "Now you can chop through ice  "
                                      "and defend the area!          ");
                }
                break;
            case SPRITE_TYPE_SHOVEL:
                hasShovel = 1;
                if (!hasSalt && pickaxeDurability < 5) {
                    hasPickaxe = 1;
                    pickaxeDurability = 5;
                }
                currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                playerXVelocity = 0;
                playerYVelocity = 0;
                banked_call(PRG_BANK_MAP_SPRITES, update_map_sprites);
                sfx_play(SFX_KEY, SFX_CHANNEL_3);
                trigger_game_text("You found the Snow Shovel!    "
                                  "You also grab a sharpened     "
                                  "pickaxe for the next block!   ");
                break;
            case SPRITE_TYPE_SNOWBLOWER:
                if (playerKeyCount > 0) {
                    --playerKeyCount;
                    hasSnowblower = 1;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    banked_call(PRG_BANK_MAP_SPRITES, update_map_sprites);
                    sfx_play(SFX_KEY, SFX_CHANNEL_3);
                    trigger_game_text("You turn the key and fire up  "
                                      "the Snowblower! Now you can   "
                                      "blast through packed snow!    ");
                } else {
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    playerControlsLockTime = 0;
                    if (!currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 30;
                        trigger_game_text("An old gas snowblower! You    "
                                          "need to find the ignition key "
                                          "to start it up!               ");
                    }
                }
                break;
            case SPRITE_TYPE_PLOW_TRUCK:
                if (playerKeyCount > 0) {
                    --playerKeyCount;
                    hasPlowTruck = 1;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    banked_call(PRG_BANK_MAP_SPRITES, update_map_sprites);
                    sfx_play(SFX_KEY, SFX_CHANNEL_3);
                    trigger_game_text("You unlock the Plow Truck and "
                                      "fire up the engine! Time to   "
                                      "plow through everything!      ");
                } else {
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    playerControlsLockTime = 0;
                    if (!currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                        currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 30;
                        trigger_game_text("The Plow Truck is locked! You "
                                          "need the truck keys to start  "
                                          "it up!                        ");
                    }
                }
                break;
            case SPRITE_TYPE_SALT_ITEM:
                if (currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN]) {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 8;
                    break;
                }
                rawXPosition = pickaxeDurability;
                if (currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH] > 0) {
                    saltCount = currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_HEALTH];
                } else {
                    saltCount = PICKAXE_MAX_DURABILITY;
                }
                hasSalt = 1;
                if (hasPickaxe && playerOverworldPosition >= 16) {
                    // Swap in Levels 1-3: drop the Pickaxe (with its remaining durability) on the ground in this spot!
                    hasPickaxe = 0;
                    pickaxeDurability = 0;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_PICKAXE;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TILE_ID] = 0x0c;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_SIZE_PALETTE] = SPRITE_SIZE_16PX_16PX | SPRITE_PALETTE_0;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_HEALTH] = rawXPosition;
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_INVULN_COUNTDOWN] = 15;
                } else {
                    currentMapSpriteData[(currentMapSpriteIndex) + MAP_SPRITE_DATA_POS_TYPE] = SPRITE_TYPE_OFFSCREEN;
                    currentMapSpritePersistance[playerOverworldPosition] |= bitToByte[lastPlayerSpriteCollisionId];
                }
                banked_call(PRG_BANK_MAP_SPRITES, update_map_sprites);
                sfx_play(SFX_KEY, SFX_CHANNEL_3);
                if (!(seenWeaponPopups & 0x02)) {
                    seenWeaponPopups |= 0x02;
                    playerXVelocity = 0;
                    playerYVelocity = 0;
                    trigger_game_text("You equipped the Rock Salt!   "
                                      "Press A to drop salt behind   "
                                      "you to slow and hurt enemies! ");
                }
                break;
            case SPRITE_TYPE_NPC:
                playerControlsLockTime = 0;
                tempSpriteCollisionX = ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_X]) + ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_X + 1]) << 8));
                tempSpriteCollisionY = ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_Y]) + ((currentMapSpriteData[currentMapSpriteIndex + MAP_SPRITE_DATA_POS_Y + 1]) << 8));

                // If moving right and player is to the left of the NPC
                if (playerXVelocity > 0 && (playerXPosition + PLAYER_X_OFFSET_EXTENDED) < (tempSpriteCollisionX + (8 << PLAYER_POSITION_SHIFT))) {
                    playerXVelocity = 0;
                // If moving left and player is to the right of the NPC
                } else if (playerXVelocity < 0 && (playerXPosition + PLAYER_X_OFFSET_EXTENDED + PLAYER_WIDTH_EXTENDED) > (tempSpriteCollisionX + (8 << PLAYER_POSITION_SHIFT))) {
                    playerXVelocity = 0;
                }

                // If moving down and player is above the NPC
                if (playerYVelocity > 0 && (playerYPosition + PLAYER_Y_OFFSET_EXTENDED) < (tempSpriteCollisionY + (8 << PLAYER_POSITION_SHIFT))) {
                    playerYVelocity = 0;
                // If moving up and player is below the NPC
                } else if (playerYVelocity < 0 && (playerYPosition + PLAYER_Y_OFFSET_EXTENDED + PLAYER_HEIGHT_EXTENDED) > (tempSpriteCollisionY + (8 << PLAYER_POSITION_SHIFT))) {
                    playerYVelocity = 0;
                }
                break;
            case SPRITE_TYPE_ENDGAME:
                gameState = GAME_STATE_CREDITS;
                break;
        }

    }
}
