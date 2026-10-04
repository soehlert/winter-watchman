#!/usr/bin/env bash
# ==============================================================================
# build.sh - Native macOS/Linux Build Script for The Winter Watchman (NES)
# ==============================================================================
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_ROOT"

ROM_NAME="winterwatchman"
ROM_DIR="$PROJECT_ROOT/rom"
TEMP_DIR="$PROJECT_ROOT/temp"
TOOLS_DIR="$PROJECT_ROOT/tools"

# 1. Detect cc65 toolchain
CC65="${CC65:-$(command -v cc65 || echo "/opt/homebrew/bin/cc65")}"
CA65="${CA65:-$(command -v ca65 || echo "/opt/homebrew/bin/ca65")}"
LD65="${LD65:-$(command -v ld65 || echo "/opt/homebrew/bin/ld65")}"

if [ ! -x "$CC65" ] || [ ! -x "$CA65" ] || [ ! -x "$LD65" ]; then
    echo "Error: cc65 toolchain not found. Install it with: brew install cc65" >&2
    exit 1
fi

# Detect cc65 share directory (for asminc and nes.lib)
CC65_SHARE=""
if [ -d "/opt/homebrew/share/cc65" ]; then
    CC65_SHARE="/opt/homebrew/share/cc65"
elif command -v brew >/dev/null 2>&1 && [ -d "$(brew --prefix cc65 2>/dev/null)/share/cc65" ]; then
    CC65_SHARE="$(brew --prefix cc65)/share/cc65"
elif [ -d "/usr/local/share/cc65" ]; then
    CC65_SHARE="/usr/local/share/cc65"
elif [ -d "/usr/share/cc65" ]; then
    CC65_SHARE="/usr/share/cc65"
fi

if [ -z "$CC65_SHARE" ]; then
    echo "Error: Could not locate cc65 library directory." >&2
    exit 1
fi

NES_LIB="$CC65_SHARE/lib/nes.lib"

# Ensure tools/cc65/asminc symlink exists for system-runtime.asm
if [ ! -d "$TOOLS_DIR/cc65/asminc" ]; then
    mkdir -p "$TOOLS_DIR/cc65"
    ln -sf "$CC65_SHARE/asminc" "$TOOLS_DIR/cc65/asminc"
fi

# 2. Handle subcommands
COMMAND="${1:-build}"

if [ "$COMMAND" = "clean" ]; then
    echo "==> Cleaning build artifacts..."
    rm -rf "$TEMP_DIR" "$ROM_DIR"
    rm -f source/c/generated/overworld.c source/c/generated/overworld.h
    echo "Done."
    exit 0
fi

# 3. Setup directories
mkdir -p "$TEMP_DIR" "$ROM_DIR"

echo "==> [1/4] Converting Tiled maps to C (tmx2c)..."
node "$TOOLS_DIR/nes-starter-kit-tools-src/tmx2c/src/index.js" 3 overworld \
    levels/overworld.tmx source/c/generated/overworld

echo "==> [2/4] Generating graphics assets (chr2img & sprite_def2img)..."
node "$TOOLS_DIR/nes-starter-kit-tools-src/chr2img/src/index.js" \
    graphics/tiles.chr graphics/palettes/main_bg.pal graphics/generated/tiles.png
node "$TOOLS_DIR/nes-starter-kit-tools-src/sprite_def2img/src/index.js" \
    ./source/c/sprites/sprite_definitions.c ./graphics/sprites.chr ./graphics/palettes/main_sprite.pal graphics/generated/sprites.png

echo "==> [3/4] Compiling C sources..."
C_OBJECTS=()
while IFS= read -r c_file; do
    rel_path="${c_file#source/c/}"
    safe_name="${rel_path//\//_}"
    base_name="${safe_name%.c}"
    s_file="$TEMP_DIR/${base_name}.s"
    o_file="$TEMP_DIR/${base_name}.o"

    "$CC65" -Oirs --add-source -T -g -t nes -I . "$c_file" -o "$s_file"
    "$CA65" -g -t nes "$s_file" -o "$o_file"
    C_OBJECTS+=("$o_file")
done < <(find source/c -name "*.c")

echo "==> [4/4] Assembling and linking ROM..."
"$CA65" -g -t nes -I . source/assembly/system-runtime.asm -o "$TEMP_DIR/system-runtime.o"

"$LD65" -C config/ca65.cfg \
    -o "$ROM_DIR/${ROM_NAME}.nes" \
    --dbgfile "$ROM_DIR/${ROM_NAME}.dbg" \
    "$TEMP_DIR/system-runtime.o" \
    "${C_OBJECTS[@]}" \
    --lib "$NES_LIB"

# Provide fallback symlink matching upstream test expectations
ln -sf "${ROM_NAME}.nes" "$ROM_DIR/nes-starter-kit-example.nes"

echo "======================================================================"
echo " BUILD SUCCESSFUL: $ROM_DIR/${ROM_NAME}.nes"
echo " ROM Size: $(wc -c < "$ROM_DIR/${ROM_NAME}.nes" | tr -d ' ') bytes"
echo "======================================================================"

if [ "$COMMAND" = "run" ]; then
    echo "==> Launching emulator..."
    open "$ROM_DIR/${ROM_NAME}.nes"
fi
