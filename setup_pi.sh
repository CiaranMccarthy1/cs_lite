#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  setup_pi.sh  –  One-shot build & configure script for Raspberry Pi 4
#  Run as a normal user (sudo is used internally where needed)
#
#  Usage:
#    chmod +x setup_pi.sh
#    ./setup_pi.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "═══════════════════════════════════════════════════════"
echo "  TacticalLite – Pi 4 Setup Script"
echo "═══════════════════════════════════════════════════════"

# ── 1. System packages ────────────────────────────────────────────────────────
echo ""
echo "[1/6] Installing system dependencies..."
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
    build-essential cmake git \
    libgl1-mesa-dev libgles2-mesa-dev \
    libx11-dev libxrandr-dev libxi-dev \
    libasound2-dev libpulse-dev \
    libwayland-dev libxkbcommon-dev \
    pkg-config

# ── 2. Raylib from source (for latest version with GLES2 support) ────────────
RAYLIB_DIR="$HOME/raylib-src"
if [ ! -d "$RAYLIB_DIR" ]; then
    echo ""
    echo "[2/6] Cloning Raylib 4.5..."
    git clone --depth=1 --branch 4.5.0 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
else
    echo "[2/6] Raylib source already present, skipping clone."
fi

RAYLIB_BUILD="$RAYLIB_DIR/build"
if [ ! -f "$RAYLIB_BUILD/raylib/libraylib.a" ]; then
    echo "      Building Raylib (OpenGL ES 2.0 backend)..."
    mkdir -p "$RAYLIB_BUILD"
    cmake -S "$RAYLIB_DIR" -B "$RAYLIB_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPLATFORM=Desktop \
        -DOPENGL_VERSION=2.1 \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_GAMES=OFF \
        -DCMAKE_C_FLAGS="-march=armv8-a+simd -mtune=cortex-a72 -O3"
    cmake --build "$RAYLIB_BUILD" --parallel 4
    sudo cmake --install "$RAYLIB_BUILD"
    echo "      Raylib installed to /usr/local"
else
    echo "[2/6] Raylib already built."
fi

# ── 3. GPU memory split ───────────────────────────────────────────────────────
echo ""
echo "[3/6] Configuring GPU memory split (gpu_mem=256)..."
CONFIG_FILE="/boot/config.txt"
if grep -q "^gpu_mem=" "$CONFIG_FILE" 2>/dev/null; then
    sudo sed -i 's/^gpu_mem=.*/gpu_mem=256/' "$CONFIG_FILE"
    echo "      Updated existing gpu_mem entry."
elif grep -q "^#gpu_mem=" "$CONFIG_FILE" 2>/dev/null; then
    sudo sed -i 's/^#gpu_mem=.*/gpu_mem=256/' "$CONFIG_FILE"
    echo "      Uncommented and set gpu_mem=256."
else
    echo 'gpu_mem=256' | sudo tee -a "$CONFIG_FILE" > /dev/null
    echo "      Added gpu_mem=256."
fi

# Also ensure V3D driver is active (should be default on Pi OS Lite)
if ! grep -q "^dtoverlay=vc4-kms-v3d" "$CONFIG_FILE" 2>/dev/null; then
    if ! grep -q "dtoverlay=vc4-kms-v3d" "$CONFIG_FILE" 2>/dev/null; then
        echo 'dtoverlay=vc4-kms-v3d' | sudo tee -a "$CONFIG_FILE" > /dev/null
        echo "      Enabled vc4-kms-v3d (V3D Mesa driver)."
    fi
fi

# ── 4. Build TacticalLite ─────────────────────────────────────────────────────
echo ""
echo "[4/6] Building TacticalLite..."
mkdir -p "$BUILD_DIR"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" --parallel 4

echo "      Binary: $BUILD_DIR/TacticalLite"

# ── 5. Create launch script ───────────────────────────────────────────────────
echo ""
echo "[5/6] Creating launch script..."
cat > "$SCRIPT_DIR/play.sh" << 'EOF'
#!/usr/bin/env bash
# Launch TacticalLite with correct Mesa environment variables
export MESA_GL_VERSION_OVERRIDE=2.1
export MESA_GLSL_VERSION_OVERRIDE=120
# Optional: uncomment for full-screen
# export SDL_VIDEODRIVER=kmsdrm  # only if running without a compositor
cd "$(dirname "$0")/build"
exec ./TacticalLite "$@"
EOF
chmod +x "$SCRIPT_DIR/play.sh"

# ── 6. Verify ─────────────────────────────────────────────────────────────────
echo ""
echo "[6/6] Build verification..."
if [ -f "$BUILD_DIR/TacticalLite" ]; then
    SIZE=$(du -h "$BUILD_DIR/TacticalLite" | cut -f1)
    echo "      ✓ TacticalLite built successfully ($SIZE)"
else
    echo "      ✗ Build failed — check errors above"
    exit 1
fi

echo ""
echo "═══════════════════════════════════════════════════════"
echo "  Setup complete!"
echo ""
echo "  IMPORTANT: Reboot for GPU memory changes to take effect."
echo "  sudo reboot"
echo ""
echo "  Then run the game:"
echo "  ./play.sh"
echo "═══════════════════════════════════════════════════════"
