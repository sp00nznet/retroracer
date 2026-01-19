#!/bin/bash
#
# RetroRacer - Multi-Platform Build Script
# Builds for all supported platforms
#
# Usage: ./scripts/build-all.sh [platform]
#   platform: dreamcast, psx, ps2, ps3, xbox, native, all
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}============================================${NC}"
    echo -e "${BLUE}  RetroRacer - $1${NC}"
    echo -e "${BLUE}============================================${NC}"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

build_native() {
    print_header "Building Native (Linux/Mac)"
    make -f Makefile.native clean 2>/dev/null || true
    make -f Makefile.native
    print_success "Native build complete: ./retroracer"
}

build_dreamcast() {
    print_header "Building for Dreamcast"
    if [ -z "$KOS_BASE" ]; then
        print_warning "KOS_BASE not set. Skipping Dreamcast build."
        print_warning "Install KallistiOS and source its environ.sh"
        return 1
    fi
    make clean 2>/dev/null || true
    make
    print_success "Dreamcast build complete: retroracer.elf"
}

build_psx() {
    print_header "Building for PlayStation 1"
    if [ ! -d "${PSYQ_PATH:-/opt/psyq}" ] && [ ! -d "${PSNOOB_PATH:-/opt/psn00bsdk}" ]; then
        print_warning "PsyQ/PSn00bSDK not found. Skipping PSX build."
        print_warning "Install PsyQ SDK or PSn00bSDK"
        return 1
    fi
    make -f Makefile.psx clean 2>/dev/null || true
    make -f Makefile.psx
    print_success "PSX build complete: retroracer.exe"
}

build_ps2() {
    print_header "Building for PlayStation 2"
    if [ -z "$PS2SDK" ]; then
        print_warning "PS2SDK not set. Skipping PS2 build."
        print_warning "Install PS2SDK from ps2dev.github.io"
        return 1
    fi
    make -f Makefile.ps2 clean 2>/dev/null || true
    make -f Makefile.ps2
    print_success "PS2 build complete: retroracer.elf"
}

build_ps3() {
    print_header "Building for PlayStation 3"
    if [ -z "$PSL1GHT" ]; then
        print_warning "PSL1GHT not set. Skipping PS3 build."
        print_warning "Install PSL1GHT SDK from ps3dev"
        return 1
    fi
    make -f Makefile.ps3 clean 2>/dev/null || true
    make -f Makefile.ps3
    print_success "PS3 build complete: retroracer.pkg"
}

build_xbox() {
    print_header "Building for Original Xbox"
    if [ ! -d "${NXDK_DIR:-$HOME/nxdk}" ]; then
        print_warning "nxdk not found. Skipping Xbox build."
        print_warning "Install nxdk from github.com/XboxDev/nxdk"
        return 1
    fi
    make -f Makefile.xbox clean 2>/dev/null || true
    make -f Makefile.xbox
    print_success "Xbox build complete: retroracer.xbe"
}

build_all() {
    echo ""
    echo "Building RetroRacer for all platforms..."
    echo ""

    FAILED=()

    build_native || FAILED+=("Native")
    echo ""

    build_dreamcast || FAILED+=("Dreamcast")
    echo ""

    build_psx || FAILED+=("PSX")
    echo ""

    build_ps2 || FAILED+=("PS2")
    echo ""

    build_ps3 || FAILED+=("PS3")
    echo ""

    build_xbox || FAILED+=("Xbox")
    echo ""

    print_header "Build Summary"

    if [ ${#FAILED[@]} -eq 0 ]; then
        print_success "All platforms built successfully!"
    else
        print_warning "Some platforms failed to build:"
        for platform in "${FAILED[@]}"; do
            echo "  - $platform"
        done
        echo ""
        echo "Make sure the required SDKs are installed:"
        echo "  - Dreamcast: KallistiOS (kallistios.org)"
        echo "  - PSX: PsyQ SDK or PSn00bSDK"
        echo "  - PS2: PS2SDK (ps2dev.github.io)"
        echo "  - PS3: PSL1GHT (ps3dev)"
        echo "  - Xbox: nxdk (github.com/XboxDev/nxdk)"
    fi
}

# Main
case "${1:-all}" in
    native)
        build_native
        ;;
    dreamcast|dc)
        build_dreamcast
        ;;
    psx|ps1|playstation1)
        build_psx
        ;;
    ps2|playstation2)
        build_ps2
        ;;
    ps3|playstation3)
        build_ps3
        ;;
    xbox)
        build_xbox
        ;;
    all)
        build_all
        ;;
    *)
        echo "Usage: $0 [platform]"
        echo ""
        echo "Platforms:"
        echo "  native     - Build for current OS (Linux/Mac)"
        echo "  dreamcast  - Build for Sega Dreamcast"
        echo "  psx        - Build for PlayStation 1"
        echo "  ps2        - Build for PlayStation 2"
        echo "  ps3        - Build for PlayStation 3"
        echo "  xbox       - Build for Original Xbox"
        echo "  all        - Build for all platforms"
        exit 1
        ;;
esac
