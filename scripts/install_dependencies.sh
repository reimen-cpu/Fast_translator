#!/bin/bash
set -e

# ==========================================
# Fast Translator - Dependency Installer
# Installs/Builds: CTranslate2, SentencePiece
# ==========================================

echo "=== Fast Translator Dependency Installer ==="

# Check for sudo
if [ "$EUID" -ne 0 ]; then
    echo "Warning: This script should ideally be run with sudo to install system packages."
    echo "If you run it without sudo, it might fail when trying to install 'apt' packages or writing to /usr/local."
    echo "You can proceed, but you might be prompted for your password multiple times."
    # We continue, as the user might have some permissions or sudo timeout active
fi

# 1. System Dependencies (Debian/Ubuntu)
echo "--- Checking System Dependencies ---"
if command -v apt-get &> /dev/null; then
    # Filter out installed packages to speed up
    NEEDED_PACKAGES=""
    # Filter out installed packages to speed up
    NEEDED_PACKAGES=""
    for pkg in build-essential cmake git curl wget pkg-config libcurl4-openssl-dev libwxgtk3.2-dev xclip xdotool libnotify-bin libdnnl-dev gcc-12 g++-12; do
        if ! dpkg -s "$pkg" &> /dev/null; then
            NEEDED_PACKAGES="$NEEDED_PACKAGES $pkg"
        fi
    done

    if [ -n "$NEEDED_PACKAGES" ]; then
        echo "Installing missing system packages: $NEEDED_PACKAGES"
        sudo apt-get update
        sudo apt-get install -y $NEEDED_PACKAGES
    else
        echo "All system packages are already installed."
    fi
else
    echo "Warning: Not a Debian/Ubuntu system? Skipping 'apt' package installation."
    echo "Please ensure you have C++ compilers, CMake, and development libraries installed."
fi

# 2. SentencePiece
echo "--- Checking SentencePiece ---"
if pkg-config --exists sentencepiece; then
    echo "SentencePiece found (system)."
else
    echo "SentencePiece NOT found. Building from source..."
    
    # Clone
    if [ ! -d "sentencepiece" ]; then
        git clone https://github.com/google/sentencepiece.git
    fi
    
    # Build
    pushd sentencepiece
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    echo "Installing SentencePiece..."
    sudo make install
    sudo ldconfig
    popd
    
    # Cleanup
    # rm -rf sentencepiece 
    echo "SentencePiece installed successfully."
fi

# 3. CTranslate2 (with CUDA detection)
echo "--- Checking CTranslate2 ---"
# Check if library exists in common locations or pkg-config (though CT2 often doesn't ship pkg-config)
if [ -f "/usr/local/lib/libctranslate2.so" ] || [ -f "/usr/lib/libctranslate2.so" ]; then
    echo "CTranslate2 found."
else
    echo "CTranslate2 NOT found. Building from source..."
    
    # Check for CUDA
    CUDA_FLAGS="-DWITH_CUDA=OFF"
    if command -v nvcc &> /dev/null; then
        echo "NVIDIA CUDA Compiler (nvcc) detected!"
        CUDA_FLAGS="-DWITH_CUDA=ON -DCMAKE_CUDA_FLAGS='-allow-unsupported-compiler'"
        
        # Check for CUDNN headers
        if [ -f "/usr/include/cudnn.h" ] || [ -f "/usr/local/cuda/include/cudnn.h" ] || [ -f "/usr/include/x86_64-linux-gnu/cudnn.h" ]; then
             echo "cuDNN headers detected. Enabling cuDNN support."
             CUDA_FLAGS="$CUDA_FLAGS -DWITH_CUDNN=ON"
        else
            # Try to find it if we just installed it
            if find /usr -name cudnn.h 2>/dev/null | grep -q "cudnn.h"; then
                 echo "cuDNN headers found (via search). Enabling cuDNN support."
                 CUDA_FLAGS="$CUDA_FLAGS -DWITH_CUDNN=ON"
            else
                 echo "Warning: cuDNN headers (cudnn.h) NOT found even after package check."
                 echo "Building CTranslate2 with CUDA but WITHOUT cuDNN (will be slower)."
                 CUDA_FLAGS="$CUDA_FLAGS -DWITH_CUDNN=OFF"
            fi
        fi
    else
        echo "nvcc not found. Building CPU-only version."
    fi
    
    if [ ! -d "CTranslate2" ]; then
        git clone --recursive https://github.com/OpenNMT/CTranslate2.git
    fi
    
    # PATCH: Force allow unsupported compiler in CMakeLists.txt to fix GCC 13+ with CUDA 12
    # We insert it right after the project() call to be safe, or just before WITH_CUDA block usage.
    # Actually, let's insert it into the WITH_CUDA block where flags are set.
    # Line 521 in their CMakeLists is: list(APPEND CUDA_NVCC_FLAGS "-std=c++17")
    echo "Patching CTranslate2/CMakeLists.txt for GCC compatibility..."
    # We still use -allow-unsupported-compiler just in case GCC 12 is technically "too new" for 12.0 (official max is 11)
    # But GCC 12 should work much better than 13.
    sed -i 's/list(APPEND CUDA_NVCC_FLAGS "-std=c++17")/list(APPEND CUDA_NVCC_FLAGS "-std=c++17" "-allow-unsupported-compiler")/' CTranslate2/CMakeLists.txt
    
    # Build
    pushd CTranslate2
    mkdir -p build
    cd build
    
    # Flags for portability matches cmake config usually
    # FORCE usage of GCC 12
    export CC=gcc-12
    export CXX=g++-12
    
    cmake .. -DCMAKE_BUILD_TYPE=Release $CUDA_FLAGS -DBUILD_CLI=OFF -DWITH_MKL=OFF -DWITH_DNNL=ON
    
    make -j$(nproc)
    echo "Installing CTranslate2..."
    sudo make install
    sudo ldconfig
    popd
    
    # Cleanup
    # rm -rf CTranslate2
    echo "CTranslate2 installed successfully."
fi

echo "=== Dependencies Installed Successfully ==="
