#!/bin/bash
set -e

# Configuration
VCPKG_ROOT="$HOME/vcpkg"
BUILD_DIR="build"

echo "=== Argos Native C++ Build Script ==="

# 1. Check Prerequisites
if [ ! -d "$VCPKG_ROOT" ]; then
    echo "Error: vcpkg not found at $VCPKG_ROOT"
    echo "Please install vcpkg or update the VCPKG_ROOT variable in this script."
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed."
    exit 1
fi

# 2. Setup Environment
# Ensure local installs are found (often in /usr/local/lib/pkgconfig)
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"

# 3. Configure and Build
echo "Configuring project..."
mkdir -p "$BUILD_DIR"

# Use vcpkg toolchain
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_BUILD_TYPE=Release

echo "Building project..."
cmake --build "$BUILD_DIR" --config Release

echo "=== Build Complete ==="
echo "Executable located at: ./$BUILD_DIR/argos_native_cpp"
echo ""
echo "Usage:"
echo "  Translate EN -> ES: ./$BUILD_DIR/argos_native_cpp"
echo "  Translate ES -> EN: ./$BUILD_DIR/argos_native_cpp es"
