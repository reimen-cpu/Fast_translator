#!/bin/bash
set -e

# ===========================================
# Fast Translator - .deb Package Builder
# Con glibc bundled para máxima portabilidad
# ===========================================

PACKAGE_NAME="fast-translator"
VERSION="1.0.2"
ARCH="amd64"
BUILD_DIR="build-deb"
DEB_ROOT="deb-build/${PACKAGE_NAME}_${VERSION}_${ARCH}"
OUTPUT_DIR="dist"

# 0. Check Dependencies
echo "Checking dependencies..."
chmod +x ./scripts/install_dependencies.sh
./scripts/install_dependencies.sh

echo "=== Building Fast Translator .deb Package v${VERSION} ==="

# 1. Clean and prepare
rm -rf "$DEB_ROOT" "$OUTPUT_DIR"
mkdir -p "$DEB_ROOT/DEBIAN"
mkdir -p "$DEB_ROOT/usr/lib/fast-translator"
mkdir -p "$DEB_ROOT/usr/bin"
mkdir -p "$DEB_ROOT/usr/share/fast-translator/packages"
mkdir -p "$DEB_ROOT/usr/share/applications"
mkdir -p "$DEB_ROOT/usr/share/icons/hicolor/128x128/apps"
mkdir -p "$OUTPUT_DIR"

# 2. Build
echo "Compiling..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"

TOOLCHAIN=""
if [ -f "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
    TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
fi

cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
    $TOOLCHAIN

cmake --build "$BUILD_DIR" --config Release --parallel

# 3. Copy executables to lib directory (they'll be called via wrapper)
echo "Installing files..."
cp "$BUILD_DIR/Fast_translator" "$DEB_ROOT/usr/lib/fast-translator/fast-translator"
if [ -f "$BUILD_DIR/Fast_translator_manager" ]; then
    cp "$BUILD_DIR/Fast_translator_manager" "$DEB_ROOT/usr/lib/fast-translator/fast-translator-manager"
fi

# 4. Collect ALL libraries including glibc
echo "Collecting libraries (including glibc)..."

collect_all_libs() {
    local binary="$1"
    
    ldd "$binary" 2>/dev/null | while read -r line; do
        lib_path=$(echo "$line" | grep -oP '=> \K[^ ]+' 2>/dev/null || true)
        
        if [ -z "$lib_path" ]; then
            lib_path=$(echo "$line" | awk '{print $1}')
        fi
        
        # Skip virtual
        [[ "$lib_path" == linux-vdso* ]] && continue
        
        if [ -f "$lib_path" ]; then
            lib_name=$(basename "$lib_path")
            if [ ! -f "$DEB_ROOT/usr/lib/fast-translator/$lib_name" ]; then
                cp -L "$lib_path" "$DEB_ROOT/usr/lib/fast-translator/" 2>/dev/null || true
                echo "  + $lib_name"
            fi
        fi
    done
}

# Collect for both executables
collect_all_libs "$DEB_ROOT/usr/lib/fast-translator/fast-translator"
[ -f "$DEB_ROOT/usr/lib/fast-translator/fast-translator-manager" ] && \
    collect_all_libs "$DEB_ROOT/usr/lib/fast-translator/fast-translator-manager"

# Copy dynamic loader
LD_LINUX=$(readelf -l "$DEB_ROOT/usr/lib/fast-translator/fast-translator" 2>/dev/null | grep "interpreter" | grep -oP '\[.*\]' | tr -d '[]')
if [ -f "$LD_LINUX" ]; then
    cp -L "$LD_LINUX" "$DEB_ROOT/usr/lib/fast-translator/"
    LD_NAME=$(basename "$LD_LINUX")
    echo "  + $LD_NAME (loader)"
fi

# Explicit copies from /usr/local
for lib in /usr/local/lib/libctranslate2.so* /usr/local/lib/libsentencepiece.so*; do
    [ -f "$lib" ] && cp -L "$lib" "$DEB_ROOT/usr/lib/fast-translator/" 2>/dev/null || true
done

# 5. Remove RPATH from all binaries/libs
echo "Cleaning RPATH..."
for f in "$DEB_ROOT/usr/lib/fast-translator"/*; do
    patchelf --remove-rpath "$f" 2>/dev/null || true
done

# 6. Create wrapper scripts that use bundled loader
echo "Creating wrapper scripts..."

LD_NAME=$(basename "$LD_LINUX" 2>/dev/null || echo "ld-linux-x86-64.so.2")

cat > "$DEB_ROOT/usr/bin/fast-translator" << WRAPPER
#!/bin/bash
LIB_DIR="/usr/lib/fast-translator"
LOG_FILE="/tmp/fast_translator_error.log"

# Run with bundled loader and libraries
"\$LIB_DIR/$LD_NAME" --library-path "\$LIB_DIR" "\$LIB_DIR/fast-translator" "\$@" 2> >(tee "\$LOG_FILE" >&2)
EXIT_CODE=\$?

if [ \$EXIT_CODE -ne 0 ]; then
    if command -v zenity >/dev/null; then
        zenity --text-info --title="Fast Translator Error" --filename="\$LOG_FILE" --width=600 --height=400 2>/dev/null
    elif command -v kdialog >/dev/null; then
        kdialog --textbox "\$LOG_FILE" 600 400 2>/dev/null
    else
        notify-send "Fast Translator Error" "Check log at \$LOG_FILE"
    fi
fi
exit \$EXIT_CODE
WRAPPER
chmod +x "$DEB_ROOT/usr/bin/fast-translator"

if [ -f "$DEB_ROOT/usr/lib/fast-translator/fast-translator-manager" ]; then
cat > "$DEB_ROOT/usr/bin/fast-translator-manager" << WRAPPER
#!/bin/bash
LIB_DIR="/usr/lib/fast-translator"
LOG_FILE="/tmp/fast_translator_manager_error.log"

"\$LIB_DIR/$LD_NAME" --library-path "\$LIB_DIR" "\$LIB_DIR/fast-translator-manager" "\$@" 2> >(tee "\$LOG_FILE" >&2)
EXIT_CODE=\$?

if [ \$EXIT_CODE -ne 0 ]; then
    if command -v zenity >/dev/null; then
        zenity --text-info --title="Manager Error" --filename="\$LOG_FILE" --width=600 --height=400 2>/dev/null
    elif command -v kdialog >/dev/null; then
        kdialog --textbox "\$LOG_FILE" 600 400 2>/dev/null
    else
        notify-send "Manager Error" "Check log at \$LOG_FILE"
    fi
fi
exit \$EXIT_CODE
WRAPPER
chmod +x "$DEB_ROOT/usr/bin/fast-translator-manager"
fi

# 7. Copy translation packages
echo "Copying translation packages..."
ARGOS_PACKAGES="$HOME/.local/share/argos-translate/packages"
[ -d "$ARGOS_PACKAGES" ] && cp -rL "$ARGOS_PACKAGES"/* "$DEB_ROOT/usr/share/fast-translator/packages/" 2>/dev/null || true
[ -d "./packages" ] && cp -rL ./packages/* "$DEB_ROOT/usr/share/fast-translator/packages/" 2>/dev/null || true

# 8. Create .desktop files
cat > "$DEB_ROOT/usr/share/applications/fast-translator-manager.desktop" << 'DESKTOP'
[Desktop Entry]
Name=Fast Translator Manager
Comment=Manage Ollama translation settings
Exec=fast-translator-manager
Icon=fast-translator
Terminal=false
Type=Application
Categories=Utility;TextTools;
Keywords=translate;translation;language;ollama;
DESKTOP

cat > "$DEB_ROOT/usr/share/applications/fast-translator.desktop" << 'DESKTOP'
[Desktop Entry]
Name=Fast Translator
Comment=Translate clipboard text
Exec=fast-translator es:en
Icon=fast-translator
Terminal=true
Type=Application
Categories=Utility;TextTools;
DESKTOP

# Icon
cat > "$DEB_ROOT/usr/share/icons/hicolor/128x128/apps/fast-translator.svg" << 'ICON'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">
  <rect width="128" height="128" rx="16" fill="#3b82f6"/>
  <text x="64" y="75" font-family="sans-serif" font-size="48" font-weight="bold" fill="white" text-anchor="middle">Tr</text>
</svg>
ICON

# 9. Create DEBIAN/control
INSTALLED_SIZE=$(du -sk "$DEB_ROOT" | cut -f1)

cat > "$DEB_ROOT/DEBIAN/control" << CONTROL
Package: fast-translator
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_SIZE}
Depends: xclip, zenity
Recommends: libcurl4 | libcurl3
Suggests: ollama
Maintainer: Fast Translator <noreply@example.com>
Description: Fast offline translation tool (self-contained)
 Native C++ translation tool with bundled runtime libraries.
 Works on any x86_64 Linux system.
 .
 Features: Clipboard translation, GPU acceleration, Ollama AI.
CONTROL

# 10. Create postinst
cat > "$DEB_ROOT/DEBIAN/postinst" << 'POSTINST'
#!/bin/bash
echo "Fast Translator installed!"
echo "Usage: fast-translator es:en"
echo "GUI:   fast-translator-manager"
POSTINST
chmod 755 "$DEB_ROOT/DEBIAN/postinst"

# 11. Create symlink for packages in lib directory (where executable looks for them)
ln -sf /usr/share/fast-translator/packages "$DEB_ROOT/usr/lib/fast-translator/packages"

# 12. Set permissions
find "$DEB_ROOT" -type d -exec chmod 755 {} \;
find "$DEB_ROOT/usr/bin" -type f -exec chmod 755 {} \;
find "$DEB_ROOT/usr/lib/fast-translator" -type f -exec chmod 755 {} \; 2>/dev/null || true

# 13. Build .deb
echo "Building .deb..."
dpkg-deb --build --root-owner-group "$DEB_ROOT"
mv "deb-build/${PACKAGE_NAME}_${VERSION}_${ARCH}.deb" "$OUTPUT_DIR/"

echo ""
echo "=== Build Complete ==="
echo ""
ls -lh "$OUTPUT_DIR/"
echo ""
echo "Install: sudo dpkg -i $OUTPUT_DIR/${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"
echo "Fix deps: sudo apt-get install -f"
