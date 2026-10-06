const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const Jimp = require('./nes-starter-kit-tools-src/chr2img/src/node_modules/jimp');

const PROJECT_ROOT = path.resolve(__dirname, '..');
const TILES_CHR_PATH = path.join(PROJECT_ROOT, 'graphics/tiles.chr');
const PAL_PATH = path.join(PROJECT_ROOT, 'graphics/palettes/main_bg.pal');
const MODERN_SHEET = path.join(PROJECT_ROOT, 'temp/residential_sheet.png');

// NES 2bpp encoder: 8x8 tile array of color indices (0..3) -> 16 bytes
function encodeTile8x8(pixelIndices) {
    const bytes = Buffer.alloc(16);
    for (let y = 0; y < 8; y++) {
        let p0 = 0;
        let p1 = 0;
        for (let x = 0; x < 8; x++) {
            const val = pixelIndices[y * 8 + x] & 0x03;
            if (val & 0x01) p0 |= (1 << (7 - x));
            if (val & 0x02) p1 |= (1 << (7 - x));
        }
        bytes[y] = p0;
        bytes[y + 8] = p1;
    }
    return bytes;
}

// Convert 16x16 pixels array into four 8x8 tiles: TL, TR, BL, BR
function split16x16To8x8(pixels16) {
    const tl = [], tr = [], bl = [], br = [];
    for (let y = 0; y < 8; y++) {
        for (let x = 0; x < 8; x++) {
            tl.push(pixels16[y * 16 + x]);
            tr.push(pixels16[y * 16 + (x + 8)]);
            bl.push(pixels16[(y + 8) * 16 + x]);
            br.push(pixels16[(y + 8) * 16 + (x + 8)]);
        }
    }
    return {
        tl: encodeTile8x8(tl),
        tr: encodeTile8x8(tr),
        bl: encodeTile8x8(bl),
        br: encodeTile8x8(br)
    };
}

// Write a 16x16 metatile into tiles.chr buffer at metatile grid coordinate (metaRow, metaCol)
function insertMetatile(chrBuffer, metaRow, metaCol, tiles) {
    const tileRow = metaRow * 2;
    const tileCol = metaCol * 2;

    const offsetTL = (tileRow * 16 + tileCol) * 16;
    const offsetTR = (tileRow * 16 + tileCol + 1) * 16;
    const offsetBL = ((tileRow + 1) * 16 + tileCol) * 16;
    const offsetBR = ((tileRow + 1) * 16 + tileCol + 1) * 16;

    tiles.tl.copy(chrBuffer, offsetTL);
    tiles.tr.copy(chrBuffer, offsetTR);
    tiles.bl.copy(chrBuffer, offsetBL);
    tiles.br.copy(chrBuffer, offsetBR);
}

// Map 32-bit RGBA from Modern Sheet to NES Palette 0 indices (0..3)
// bgIndex defaults to 1 (Floor Gray #787878) so transparent areas blend into room floors
// allowWood = false prevents wood amber (2) on appliances / metal / porcelain to eliminate yellow artifacting
function quantizePixel(rgba, bgIndex = 1, allowWood = true) {
    if (rgba.a < 50) return bgIndex;
    const r = rgba.r, g = rgba.g, b = rgba.b;
    const maxVal = Math.max(r, g, b);
    const minVal = Math.min(r, g, b);
    const sat = maxVal - minVal;
    const brightness = (r + g + b) / 3;

    if (brightness < 35) return 0; // dark outline / black
    if (sat < 25) return 1; // neutral gray / white / silver metal / ceramic -> Floor Gray (1)

    // Bluish / teal (screen, glass, trim)
    if (b > r + 12 || (g > r + 10 && b > r)) return 3;

    // Wood amber / warm orange (only if allowWood is true!)
    if (allowWood && r > b + 18 && sat > 25 && r > 65) return 2;

    return 1; // default neutral gray
}

function quantizeCrop16(jimpImg, srcX, srcY, bgIndex = 1, allowWood = true) {
    const pixels = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            const rgba = Jimp.intToRGBA(jimpImg.getPixelColor(srcX + x, srcY + y));
            pixels.push(quantizePixel(rgba, bgIndex, allowWood));
        }
    }
    return pixels;
}

async function run() {
    console.log('Restoring outside tiles, setting clean 2-palette configuration, and injecting interior goods...');

    // Load original baseline tiles.chr to ensure genuine exterior and HUD tiles are preserved:
    // - Row 0: cols 0..4 (snow / ground)
    // - Row 4: cols 0..6 (rocks, dirt waves, stump)
    // - Row 6: cols 3..6 (tree orbs, heart)
    // - Row 7: cols 0, 2..7 (locked door, HUD icons, font)
    const baselineCHR = execSync('env GIT_CONFIG_GLOBAL=/dev/null git show main:graphics/tiles.chr');
    const chrBuffer = Buffer.from(baselineCHR);
    const palBuffer = Buffer.alloc(16, 0x0f);

    // =========================================================================
    // 1. PALETTES: House (Pal 0), Outdoors Snow (Pal 1), Palettes 3 & 4 REMOVED (All $0F)
    // =========================================================================
    // Palette 0: House Interior
    // Color 0: 0x0f (Black)
    // Color 1: 0x00 (Carpet Gray #787878)
    // Color 2: 0x18 (Wood Amber #ac7c00)
    // Color 3: 0x0c (Slate Teal #004058)
    palBuffer[0] = 0x0f;
    palBuffer[1] = 0x00;
    palBuffer[2] = 0x18;
    palBuffer[3] = 0x0c;

    // Palette 1: Outdoors Snow & Ice
    // Color 0: 0x0f (Black)
    // Color 1: 0x11 (Cold Blue #0078f8)
    // Color 2: 0x21 (Ice Cyan #3cbcfc)
    // Color 3: 0x30 (Pure Snow White #fcfcfc)
    palBuffer[4] = 0x0f;
    palBuffer[5] = 0x11;
    palBuffer[6] = 0x21;
    palBuffer[7] = 0x30;

    // Palette 2 (Palette 3 in editor): Matches the computer desk background gray ($00)
    palBuffer[8] = 0x0f;  // Color 0: Black
    palBuffer[9] = 0x00;  // Color 1: Desk background gray ($00)
    palBuffer[10] = 0x00; // Color 2: Desk background gray ($00)
    palBuffer[11] = 0x00; // Color 3: Desk background gray ($00)

    // Palette 3 (Palette 4 in editor): HUD & Dialogue Text Box
    palBuffer[12] = 0x0f; // Color 0: Black background
    palBuffer[13] = 0x00; // Color 1: Border gray ($00)
    palBuffer[14] = 0x16; // Color 2: Hearts red ($16)
    palBuffer[15] = 0x30; // Color 3: Text & digits white ($30)

    fs.writeFileSync(PAL_PATH, palBuffer);
    console.log('Palettes configured: House (Pal 0), Outdoors (Pal 1), Desk Gray (Pal 2), HUD/Text (Pal 3).');

    // =========================================================================
    // 2. ROW 0: Outside Snow (0..4 preserved), Trapdoor (0,5), Ladder (0,6), Door (0,7)
    // =========================================================================
    // (0, 5): Cellar Hatch Trapdoor
    const hatchPixels = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 0 || x === 15 || y === 0 || y === 15) hatchPixels.push(0);
            else if (x === 1 || x === 14 || y === 1 || y === 14) hatchPixels.push(0);
            else if (x === 7 || x === 8) hatchPixels.push(0);
            else if ((x === 5 || x === 10) && (y >= 6 && y <= 9)) hatchPixels.push(1);
            else if (y % 4 === 0) hatchPixels.push(0);
            else hatchPixels.push(2);
        }
    }
    insertMetatile(chrBuffer, 0, 5, split16x16To8x8(hatchPixels));

    // (0, 6): Cellar Ladder for the Trapdoor
    const ladderPixels = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 2 || x === 3 || x === 12 || x === 13) {
                ladderPixels.push(2); // wood rails
            } else if (y % 4 === 0 && x > 3 && x < 12) {
                ladderPixels.push(2); // wood rungs
            } else if (y % 4 === 1 && x > 3 && x < 12) {
                ladderPixels.push(0); // rung shadow
            } else {
                ladderPixels.push(0); // dark cellar hole
            }
        }
    }
    insertMetatile(chrBuffer, 0, 6, split16x16To8x8(ladderPixels));

    // (0, 7): South Exterior Door (Closed)
    const doorBottom_Closed = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x < 3 || x > 12) {
                if (y === 0) doorBottom_Closed.push(0);
                else if (y <= 2) doorBottom_Closed.push(2);
                else if (y <= 13) doorBottom_Closed.push(3);
                else if (y === 14) doorBottom_Closed.push(2);
                else doorBottom_Closed.push(0);
            } else if (x === 3 || x === 12) {
                if (y === 0 || y === 15) doorBottom_Closed.push(0);
                else doorBottom_Closed.push(2);
            } else {
                if (y === 0) doorBottom_Closed.push(0);
                else if (y === 1) doorBottom_Closed.push(2);
                else if (y === 2) doorBottom_Closed.push(0);
                else if (y >= 3 && y <= 6) {
                    if (x === 4 || x === 11) doorBottom_Closed.push(0);
                    else if (x === 5 || x === 6 || x === 9 || x === 10) doorBottom_Closed.push(1);
                    else doorBottom_Closed.push(2);
                } else if (y === 7) {
                    if (x === 4 || x === 11) doorBottom_Closed.push(0);
                    else doorBottom_Closed.push(2);
                } else if (y === 8) {
                    if (x === 4 || x === 11) doorBottom_Closed.push(0);
                    else if (x === 10) doorBottom_Closed.push(1);
                    else if (x === 9) doorBottom_Closed.push(0);
                    else doorBottom_Closed.push(2);
                } else if (y >= 9 && y <= 12) {
                    if (x === 4 || x === 11 || x === 5 || x === 10) doorBottom_Closed.push(0);
                    else doorBottom_Closed.push(2);
                } else if (y === 13) {
                    if (x === 4 || x === 11) doorBottom_Closed.push(0);
                    else doorBottom_Closed.push(2);
                } else if (y === 14) {
                    doorBottom_Closed.push(2);
                } else {
                    doorBottom_Closed.push(0);
                }
            }
        }
    }
    insertMetatile(chrBuffer, 0, 7, split16x16To8x8(doorBottom_Closed));

    // =========================================================================
    // 3. ROW 1: 8-PIECE PERIMETER HOUSE WALL SET
    // =========================================================================
    const wallTop = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (y === 0) wallTop.push(0);
            else if (y === 1) wallTop.push(2);
            else if (y <= 9) wallTop.push(3);
            else if (y === 10) wallTop.push(2);
            else if (y <= 13) {
                if ((x + 1) % 4 === 0) wallTop.push(0);
                else wallTop.push(2);
            } else if (y === 14) wallTop.push(2);
            else wallTop.push(0);
        }
    }
    insertMetatile(chrBuffer, 1, 1, split16x16To8x8(wallTop));

    const wallLeft = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 0) wallLeft.push(0);
            else if (x === 1) wallLeft.push(2);
            else if (x <= 12) wallLeft.push(3);
            else if (x <= 14) wallLeft.push(2);
            else wallLeft.push(0);
        }
    }
    insertMetatile(chrBuffer, 1, 3, split16x16To8x8(wallLeft));

    const wallRight = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 0) wallRight.push(0);
            else if (x <= 2) wallRight.push(2);
            else if (x <= 13) wallRight.push(3);
            else if (x === 14) wallRight.push(2);
            else wallRight.push(0);
        }
    }
    insertMetatile(chrBuffer, 1, 4, split16x16To8x8(wallRight));

    const wallBottom = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (y === 0) wallBottom.push(0);
            else if (y <= 2) wallBottom.push(2);
            else if (y <= 13) wallBottom.push(3);
            else if (y === 14) wallBottom.push(2);
            else wallBottom.push(0);
        }
    }
    insertMetatile(chrBuffer, 1, 6, split16x16To8x8(wallBottom));

    const wallTL = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 0 || y === 0) wallTL.push(0);
            else if (x === 1 || y === 1) wallTL.push(2);
            else if (x === 15) wallTL.push(wallTop[y * 16 + 0]);
            else if (y === 15) wallTL.push(wallLeft[0 * 16 + x]);
            else if (x >= 13 && y >= 13) wallTL.push(2);
            else if (y === 10 && x >= 11) wallTL.push(2);
            else if (y >= 11 && y <= 13 && x >= 11) {
                if ((x + 1) % 4 === 0) wallTL.push(0);
                else wallTL.push(2);
            } else wallTL.push(3);
        }
    }
    insertMetatile(chrBuffer, 1, 0, split16x16To8x8(wallTL));

    const wallTR = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 15 || y === 0) wallTR.push(0);
            else if (x === 14 || y === 1) wallTR.push(2);
            else if (x === 0) wallTR.push(wallTop[y * 16 + 15]);
            else if (y === 15) wallTR.push(wallRight[0 * 16 + x]);
            else if (x <= 2 && y >= 13) wallTR.push(2);
            else if (y === 10 && x <= 4) wallTR.push(2);
            else if (y >= 11 && y <= 13 && x <= 4) {
                if ((x + 1) % 4 === 0) wallTR.push(0);
                else wallTR.push(2);
            } else wallTR.push(3);
        }
    }
    insertMetatile(chrBuffer, 1, 2, split16x16To8x8(wallTR));

    const wallBL = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 0 || y === 15) wallBL.push(0);
            else if (x === 1 || y === 14) wallBL.push(2);
            else if (x === 15) wallBL.push(wallBottom[y * 16 + 0]);
            else if (y === 0) wallBL.push(wallLeft[15 * 16 + x]);
            else if (x >= 13 && y <= 2) wallBL.push(2);
            else wallBL.push(3);
        }
    }
    insertMetatile(chrBuffer, 1, 5, split16x16To8x8(wallBL));

    const wallBR = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 15 || y === 15) wallBR.push(0);
            else if (x === 14 || y === 14) wallBR.push(2);
            else if (x === 0) wallBR.push(wallBottom[y * 16 + 15]);
            else if (y === 0) wallBR.push(wallRight[15 * 16 + x]);
            else if (x <= 2 && y <= 2) wallBR.push(2);
            else wallBR.push(3);
        }
    }
    insertMetatile(chrBuffer, 1, 7, split16x16To8x8(wallBR));

    // =========================================================================
    // 4. ROW 2: 8-PIECE INTERIOR ROOM DIVIDER SYSTEM
    // =========================================================================
    const divHoriz = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (y < 4 || y > 11) divHoriz.push(1);
            else if (y <= 5) divHoriz.push(2);
            else if (y <= 8) divHoriz.push(3);
            else if (y <= 10) divHoriz.push(2);
            else divHoriz.push(0);
        }
    }
    insertMetatile(chrBuffer, 2, 0, split16x16To8x8(divHoriz));

    const divVert = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x < 4 || x > 11) divVert.push(1);
            else if (x <= 5) divVert.push(2);
            else if (x <= 8) divVert.push(3);
            else if (x <= 10) divVert.push(2);
            else divVert.push(0);
        }
    }
    insertMetatile(chrBuffer, 2, 1, split16x16To8x8(divVert));

    const divCorner_TL = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x < 4 || y < 4 || (x > 11 && y > 11)) divCorner_TL.push(1);
            else if ((x >= 6 && y >= 6 && y <= 8) || (y >= 6 && x >= 6 && x <= 8)) divCorner_TL.push(3);
            else if ((y === 11 && x >= 11) || (x === 11 && y >= 11)) divCorner_TL.push(0);
            else divCorner_TL.push(2);
        }
    }
    insertMetatile(chrBuffer, 2, 2, split16x16To8x8(divCorner_TL));

    const divCorner_TR = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x > 11 || y < 4 || (x < 4 && y > 11)) divCorner_TR.push(1);
            else if ((x <= 8 && y >= 6 && y <= 8) || (y >= 6 && x >= 6 && x <= 8)) divCorner_TR.push(3);
            else if (x === 11 || (y === 11 && x < 4)) divCorner_TR.push(0);
            else divCorner_TR.push(2);
        }
    }
    insertMetatile(chrBuffer, 2, 3, split16x16To8x8(divCorner_TR));

    const divCorner_BL = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x < 4 || y > 11 || (x > 11 && y < 4)) divCorner_BL.push(1);
            else if ((x >= 6 && y >= 6 && y <= 8) || (y <= 8 && x >= 6 && x <= 8)) divCorner_BL.push(3);
            else if (y === 11 || (x === 11 && y < 4)) divCorner_BL.push(0);
            else divCorner_BL.push(2);
        }
    }
    insertMetatile(chrBuffer, 2, 4, split16x16To8x8(divCorner_BL));

    const divCorner_BR = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x > 11 || y > 11 || (x < 4 && y < 4)) divCorner_BR.push(1);
            else if ((x <= 8 && y >= 6 && y <= 8) || (y <= 8 && x >= 6 && x <= 8)) divCorner_BR.push(3);
            else if (x === 11 || y === 11) divCorner_BR.push(0);
            else divCorner_BR.push(2);
        }
    }
    insertMetatile(chrBuffer, 2, 5, split16x16To8x8(divCorner_BR));

    const doorUpDown = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x >= 4 && x <= 11) doorUpDown.push(1);
            else if (x <= 2) {
                if (y < 4 || y > 11) doorUpDown.push(1);
                else if (y <= 5) doorUpDown.push(2);
                else if (y <= 8) doorUpDown.push(3);
                else if (y <= 10) doorUpDown.push(2);
                else doorUpDown.push(0);
            } else if (x === 3 || x === 12) {
                if (y < 4 || y > 11) doorUpDown.push(1);
                else if (y === 11) doorUpDown.push(0);
                else doorUpDown.push(2);
            } else {
                if (y < 4 || y > 11) doorUpDown.push(1);
                else if (y <= 5) doorUpDown.push(2);
                else if (y <= 8) doorUpDown.push(3);
                else if (y <= 10) doorUpDown.push(2);
                else doorUpDown.push(0);
            }
        }
    }
    insertMetatile(chrBuffer, 2, 6, split16x16To8x8(doorUpDown));

    const doorSideToSide = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (y >= 4 && y <= 11) doorSideToSide.push(1);
            else if (y <= 2) {
                if (x < 4 || x > 11) doorSideToSide.push(1);
                else if (x <= 5) doorSideToSide.push(2);
                else if (x <= 8) doorSideToSide.push(3);
                else if (x <= 10) doorSideToSide.push(2);
                else doorSideToSide.push(0);
            } else if (y === 3 || y === 12) {
                if (x < 4 || x > 11) doorSideToSide.push(1);
                else if (x === 11) doorSideToSide.push(0);
                else doorSideToSide.push(2);
            } else {
                if (x < 4 || x > 11) doorSideToSide.push(1);
                else if (x <= 5) doorSideToSide.push(2);
                else if (x <= 8) doorSideToSide.push(3);
                else if (x <= 10) doorSideToSide.push(2);
                else doorSideToSide.push(0);
            }
        }
    }
    insertMetatile(chrBuffer, 2, 7, split16x16To8x8(doorSideToSide));

    // =========================================================================
    // 5. ROW 3: REPLACES THE "MOUNTAINS" (BRICK WALLS) WITH DESK, STOVE, MICROWAVE, WINDOW
    // =========================================================================
    const modernSheet = await Jimp.read(MODERN_SHEET);

    // (3, 0): Top Wall with Window
    const wallTop_Win = [...wallTop];
    for (let y = 2; y <= 8; y++) {
        for (let x = 4; x <= 11; x++) {
            if (x === 4 || x === 11 || y === 2 || y === 8 || x === 7 || x === 8) {
                wallTop_Win[y * 16 + x] = 2;
            } else {
                wallTop_Win[y * 16 + x] = 1;
            }
        }
    }
    insertMetatile(chrBuffer, 3, 0, split16x16To8x8(wallTop_Win));

    // (3, 1): Microwave Appliance on Counter (Floor Gray bg, clean metal)
    insertMetatile(chrBuffer, 3, 1, split16x16To8x8(quantizeCrop16(modernSheet, 96, 16, 1, false)));

    // (3, 2) & (3, 3): Kitchen Stove & Oven (32x16 from sheet at x=128, y=48)
    insertMetatile(chrBuffer, 3, 2, split16x16To8x8(quantizeCrop16(modernSheet, 128, 48, 1)));
    insertMetatile(chrBuffer, 3, 3, split16x16To8x8(quantizeCrop16(modernSheet, 144, 48, 1)));

    // (3, 4)..(3, 7): Modern PC Desk with CRT Monitor, Keyboard, Mouse & Wood Drawers (32x32)
    insertMetatile(chrBuffer, 3, 4, split16x16To8x8(quantizeCrop16(modernSheet, 425, 112, 1)));
    insertMetatile(chrBuffer, 3, 5, split16x16To8x8(quantizeCrop16(modernSheet, 441, 112, 1)));
    insertMetatile(chrBuffer, 3, 6, split16x16To8x8(quantizeCrop16(modernSheet, 425, 128, 1)));
    insertMetatile(chrBuffer, 3, 7, split16x16To8x8(quantizeCrop16(modernSheet, 441, 128, 1)));

    // =========================================================================
    // 6. ROW 5: REFRIGERATOR (5,0..3) & KITCHEN ISLAND SINK (5,4..7)
    // Replaces the black rows with authentic kitchen goods
    // =========================================================================
    function quantizeFridge32x32(img) {
        const rows = [];
        for (let y = 43; y <= 79; y++) {
            if (y === 46 || y === 47 || y === 60 || y === 61 || y === 74) continue;
            rows.push(y);
        }
        const pixels32 = [];
        for (let r = 0; r < 32; r++) {
            const srcY = rows[r];
            for (let c = 0; c < 32; c++) {
                if (c < 3 || c > 28) {
                    pixels32.push(1); // Floor Gray
                } else {
                    const srcX = 99 + (c - 3);
                    const rgba = Jimp.intToRGBA(img.getPixelColor(srcX, srcY));
                    pixels32.push(quantizePixel(rgba, 1, false));
                }
            }
        }
        const tl = [], tr = [], bl = [], br = [];
        for (let y = 0; y < 16; y++) {
            for (let x = 0; x < 16; x++) {
                tl.push(pixels32[y * 32 + x]);
                tr.push(pixels32[y * 32 + (x + 16)]);
                bl.push(pixels32[(y + 16) * 32 + x]);
                br.push(pixels32[(y + 16) * 32 + (x + 16)]);
            }
        }
        return {
            tl: split16x16To8x8(tl),
            tr: split16x16To8x8(tr),
            bl: split16x16To8x8(bl),
            br: split16x16To8x8(br)
        };
    }

    const fridgeTiles = quantizeFridge32x32(modernSheet);
    insertMetatile(chrBuffer, 5, 0, fridgeTiles.tl);
    insertMetatile(chrBuffer, 5, 1, fridgeTiles.tr);
    insertMetatile(chrBuffer, 5, 2, fridgeTiles.bl);
    insertMetatile(chrBuffer, 5, 3, fridgeTiles.br);

    function quantizeSink32x32(img) {
        const sinkImg = new Jimp(32, 32, 0x00000000);
        sinkImg.blit(img.clone().crop(96, 84, 8, 32), 0, 0);
        sinkImg.blit(img.clone().crop(128, 84, 16, 32), 8, 0);
        sinkImg.blit(img.clone().crop(168, 84, 8, 32), 24, 0);

        const tl = [], tr = [], bl = [], br = [];
        for (let y = 0; y < 16; y++) {
            for (let x = 0; x < 16; x++) {
                tl.push(quantizePixel(Jimp.intToRGBA(sinkImg.getPixelColor(x, y)), 1, false));
                tr.push(quantizePixel(Jimp.intToRGBA(sinkImg.getPixelColor(x + 16, y)), 1, false));
                bl.push(quantizePixel(Jimp.intToRGBA(sinkImg.getPixelColor(x, y + 16)), 1, false));
                br.push(quantizePixel(Jimp.intToRGBA(sinkImg.getPixelColor(x + 16, y + 16)), 1, false));
            }
        }
        return {
            tl: split16x16To8x8(tl),
            tr: split16x16To8x8(tr),
            bl: split16x16To8x8(bl),
            br: split16x16To8x8(br)
        };
    }

    const sinkTiles = quantizeSink32x32(modernSheet);
    insertMetatile(chrBuffer, 5, 4, sinkTiles.tl);
    insertMetatile(chrBuffer, 5, 5, sinkTiles.tr);

    // (5, 6): Bottom Exterior Wall with Sconce
    const wallBottom_Sconce = [...wallBottom];
    // Flame (y=4..5)
    wallBottom_Sconce[4 * 16 + 7] = 2; wallBottom_Sconce[4 * 16 + 8] = 2;
    wallBottom_Sconce[5 * 16 + 6] = 2; wallBottom_Sconce[5 * 16 + 7] = 1; wallBottom_Sconce[5 * 16 + 8] = 1; wallBottom_Sconce[5 * 16 + 9] = 2;

    // Candle wax (y=6..7)
    wallBottom_Sconce[6 * 16 + 7] = 1; wallBottom_Sconce[6 * 16 + 8] = 1;
    wallBottom_Sconce[7 * 16 + 7] = 1; wallBottom_Sconce[7 * 16 + 8] = 1;

    // Brass Cup (y=8)
    wallBottom_Sconce[8 * 16 + 5] = 0; wallBottom_Sconce[8 * 16 + 6] = 2; wallBottom_Sconce[8 * 16 + 7] = 2; wallBottom_Sconce[8 * 16 + 8] = 2; wallBottom_Sconce[8 * 16 + 9] = 2; wallBottom_Sconce[8 * 16 + 10] = 0;

    // Cup Base / curved arm (y=9)
    wallBottom_Sconce[9 * 16 + 6] = 0; wallBottom_Sconce[9 * 16 + 7] = 2; wallBottom_Sconce[9 * 16 + 8] = 2; wallBottom_Sconce[9 * 16 + 9] = 0;

    // Wall Mount Bracket (y=10..12)
    wallBottom_Sconce[10 * 16 + 7] = 0; wallBottom_Sconce[10 * 16 + 8] = 0;
    wallBottom_Sconce[11 * 16 + 6] = 0; wallBottom_Sconce[11 * 16 + 7] = 2; wallBottom_Sconce[11 * 16 + 8] = 2; wallBottom_Sconce[11 * 16 + 9] = 0;
    wallBottom_Sconce[12 * 16 + 7] = 0; wallBottom_Sconce[12 * 16 + 8] = 0;

    insertMetatile(chrBuffer, 5, 6, split16x16To8x8(wallBottom_Sconce));

    // (5, 7): Bottom Exterior Wall with Big Window
    const wallBottom_BigWin = [...wallBottom];
    for (let y = 2; y <= 13; y++) {
        for (let x = 2; x <= 13; x++) {
            if (x === 2 || x === 13 || y === 2) {
                wallBottom_BigWin[y * 16 + x] = 0; // dark outer casing
            } else if (y === 13) {
                wallBottom_BigWin[y * 16 + x] = 0; // sill shadow
            } else if (y === 12) {
                wallBottom_BigWin[y * 16 + x] = 2; // wood sill
            } else if (x === 3 || x === 12 || y === 3) {
                wallBottom_BigWin[y * 16 + x] = 2; // wood frame
            } else if (x === 7 || x === 8 || y === 7) {
                wallBottom_BigWin[y * 16 + x] = 2; // mullions
            } else {
                wallBottom_BigWin[y * 16 + x] = 1; // bright clean glass
            }
        }
    }
    // Sill horns extending onto baseboard
    wallBottom_BigWin[12 * 16 + 1] = 2; wallBottom_BigWin[12 * 16 + 14] = 2;
    wallBottom_BigWin[13 * 16 + 1] = 0; wallBottom_BigWin[13 * 16 + 14] = 0;

    insertMetatile(chrBuffer, 5, 7, split16x16To8x8(wallBottom_BigWin));

    // =========================================================================
    // 7. ROW 6: DINING TABLE (6,0 & 6,1), CHAIR FRONT (6,2), CHAIR SIDE (6,7)
    // Preserves outside tree orbs & heart at (6, 3..6)
    // =========================================================================
    const tableL = [], tableR = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (y < 8) {
                if (x === 0) tableL.push(0);
                else if (y === 7) tableL.push(0);
                else tableL.push(2);
            } else {
                if (x >= 2 && x <= 5 && y < 15) tableL.push(2);
                else if (x >= 2 && x <= 5 && y === 15) tableL.push(0);
                else tableL.push(1);
            }

            if (y < 8) {
                if (x === 15) tableR.push(0);
                else if (y === 7) tableR.push(0);
                else tableR.push(2);
            } else {
                if (x >= 10 && x <= 13 && y < 15) tableR.push(2);
                else if (x >= 10 && x <= 13 && y === 15) tableR.push(0);
                else tableR.push(1);
            }
        }
    }
    insertMetatile(chrBuffer, 6, 0, split16x16To8x8(tableL));
    insertMetatile(chrBuffer, 6, 1, split16x16To8x8(tableR));

    // Chair Front Facing (with clean black outline)
    const chairFront = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            let p = 1;
            if (y === 0) {
                if (x >= 3 && x <= 12) p = 0; // top outline
            } else if (y === 1) {
                if (x === 3 || x === 12) p = 0;
                else if (x >= 4 && x <= 11) p = 2; // top rail
            } else if (y === 2) {
                if (x === 3 || x === 6 || x === 9 || x === 12) p = 0;
                else if (x === 4 || x === 5 || x === 10 || x === 11) p = 2;
                else p = 1;
            } else if (y >= 3 && y <= 5) {
                if (x === 4 || x === 11) p = 2;
                else if (x === 3 || x === 5 || x === 10 || x === 12) p = 0;
            } else if (y === 6) {
                if (x >= 2 && x <= 13) p = 0; // seat top outline
            } else if (y >= 7 && y <= 8) {
                if (x === 2 || x === 13) p = 0;
                else if (x >= 3 && x <= 12) p = 2; // seat cushion
            } else if (y === 9) {
                if (x >= 2 && x <= 13) p = 0; // seat bottom outline
            } else if (y >= 10 && y <= 13) {
                if (x === 2 || x === 5 || x === 10 || x === 13) p = 0;
                else if ((x >= 3 && x <= 4) || (x >= 11 && x <= 12)) p = 2;
            } else if (y === 14) {
                if ((x >= 2 && x <= 5) || (x >= 10 && x <= 13)) p = 0; // feet outline
            }
            chairFront.push(p);
        }
    }
    insertMetatile(chrBuffer, 6, 2, split16x16To8x8(chairFront));

    // Chair Side Facing (with clean black outline)
    const chairSide = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            let p = 1;
            if (y === 0) {
                if (x >= 3 && x <= 5) p = 0; // top of backrest outline
            } else if (y >= 1 && y <= 6) {
                if (x === 3 || x === 6) p = 0;
                else if (x === 4 || x === 5) p = 2; // backrest post
            } else if (y === 7) {
                if (x >= 3 && x <= 12) p = 0; // seat top outline
            } else if (y === 8) {
                if (x === 3 || x === 13) p = 0;
                else if (x >= 4 && x <= 12) p = 2; // seat
            } else if (y === 9) {
                if (x >= 3 && x <= 13) p = 0; // seat bottom outline
            } else if (y >= 10 && y <= 13) {
                if (x === 3 || x === 5 || x === 10 || x === 12) p = 0;
                else if (x === 4 || x === 11) p = 2;
            } else if (y === 14) {
                if ((x >= 3 && x <= 5) || (x >= 10 && x <= 12)) p = 0; // feet outline
            }
            chairSide.push(p);
        }
    }
    insertMetatile(chrBuffer, 6, 7, split16x16To8x8(chairSide));

    // 32x32 Area Carpet / Rug from Residential Sheet (replaces tree orbs at 6, 3..6)
    function extractRugTile(startX, startY) {
        const pixels = [];
        for (let y = 0; y < 16; y++) {
            for (let x = 0; x < 16; x++) {
                const c = Jimp.intToRGBA(modernSheet.getPixelColor(startX + x, startY + y));
                if (c.r < 15 && c.g < 15 && c.b < 20) pixels.push(0); // outer border
                else if (c.b > 90) pixels.push(1); // diamond
                else if (c.b > 50) pixels.push(3); // teal field
                else pixels.push(1);
            }
        }
        return pixels;
    }

    const rugTL = extractRugTile(224, 16);
    const rugTR = extractRugTile(256, 16);
    const rugBL = extractRugTile(224, 32);
    const rugBR = extractRugTile(256, 32);

    insertMetatile(chrBuffer, 6, 3, split16x16To8x8(rugTL));
    insertMetatile(chrBuffer, 6, 4, split16x16To8x8(rugTR));
    insertMetatile(chrBuffer, 6, 5, split16x16To8x8(rugBL));
    insertMetatile(chrBuffer, 6, 6, split16x16To8x8(rugBR));

    // =========================================================================
    // 8. ROW 4, COL 5: CERAMIC KITCHEN FLOOR TILE (Replaces the top hat)
    // =========================================================================
    const kitchenTile = [];
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            if (x === 0 || x === 8 || y === 0 || y === 8) {
                kitchenTile.push(0); // dark black grout line
            } else {
                const tx = Math.floor(x / 8);
                const ty = Math.floor(y / 8);
                kitchenTile.push((tx ^ ty) ? 3 : 1);
            }
        }
    }
    insertMetatile(chrBuffer, 4, 5, split16x16To8x8(kitchenTile));

    // Write back updated tiles.chr
    fs.writeFileSync(TILES_CHR_PATH, chrBuffer);
    console.log('Successfully wrote graphics/tiles.chr!');
}

run().catch(console.error);
