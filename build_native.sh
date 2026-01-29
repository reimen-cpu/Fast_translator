#!/bin/bash
set -e

# Configuration
VCPKG_ROOT="$HOME/vcpkg"
BUILD_DIR="build"

echo "=== Fast Translator Build Script ==="

# 1. Check Prerequisites and Install Dependencies
echo "Checking dependencies..."
chmod +x ./scripts/install_dependencies.sh
./scripts/install_dependencies.sh

if [ ! -d "$VCPKG_ROOT" ]; then
    echo "Warning: vcpkg not found at $VCPKG_ROOT. Attempting to use system libraries only."
    # We don't exit here because we might strictly use system libs now.
    # But if existing logic relies on vcpkg toolchain file, we might warn.
fi

if ! command -v cmake &> /dev/null; then
    echo "Error: cmake not installed (and failed to install via script?)"
    exit 1
fi

# 2. Setup Environment
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"

# 3. Clean build if requested
if [ "$1" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# 4. Configure (only if needed)
if [ ! -d "$BUILD_DIR" ] || [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "Configuring project..."
    mkdir -p "$BUILD_DIR"
    TOOLCHAIN=""
    if [ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
        TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    fi

    cmake -B "$BUILD_DIR" -S . \
        $TOOLCHAIN \
        -DCMAKE_BUILD_TYPE=Release
else
    echo "Build directory exists, skipping configure..."
fi

# 5. Build
echo "Building..."
cmake --build "$BUILD_DIR" --config Release --parallel

echo ""
echo "=== Build Complete ==="
echo "Executables:"
echo "  ./$BUILD_DIR/Fast_translator"
echo "  ./$BUILD_DIR/Fast_translator_manager"
echo ""
echo "Usage:"
echo "  Translation: ./$BUILD_DIR/Fast_translator es:en"
echo "  AI Mode:     ./$BUILD_DIR/Fast_translator --ollama <model>"
echo "  Manager GUI: ./$BUILD_DIR/Fast_translator_manager"
