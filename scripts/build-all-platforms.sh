#!/bin/bash
#
# RetroRacer - Multi-Platform Build Script
#
# Builds RetroRacer for all supported console platforms
#
# Supported platforms:
#   - dreamcast  : SEGA Dreamcast (PVR)
#   - psx        : Sony PlayStation 1 (PSn00bSDK)
#   - ps2        : Sony PlayStation 2 (PS2SDK)
#   - ps3        : Sony PlayStation 3 (PSL1GHT)
#   - xbox       : Microsoft Xbox (NXDK)
#   - xbox360    : Microsoft Xbox 360 (LibXenon)
#   - native     : Native PC build (development)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Build results tracking
declare -A BUILD_RESULTS

print_header() {
    echo ""
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}======================================${NC}"
    echo ""
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

check_sdk() {
    local sdk_name=$1
    local sdk_path=$2

    if [ -d "$sdk_path" ]; then
        echo -e "  ${GREEN}✓${NC} $sdk_name found at $sdk_path"
        return 0
    else
        echo -e "  ${RED}✗${NC} $sdk_name not found at $sdk_path"
        return 1
    fi
}

build_platform() {
    local platform=$1
    local makefile=$2

    print_header "Building for $platform"

    cd "$PROJECT_DIR"

    if make -f "$makefile" clean all 2>&1; then
        BUILD_RESULTS[$platform]="SUCCESS"
        print_success "$platform build completed"
    else
        BUILD_RESULTS[$platform]="FAILED"
        print_error "$platform build failed"
    fi
}

show_help() {
    echo "RetroRacer Multi-Platform Build Script"
    echo ""
    echo "Usage: $0 [options] [platforms...]"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -c, --clean    Clean all build directories first"
    echo "  -i, --info     Show SDK/toolchain status"
    echo "  -a, --all      Build for all platforms"
    echo ""
    echo "Platforms:"
    echo "  dreamcast      SEGA Dreamcast"
    echo "  psx            Sony PlayStation 1"
    echo "  ps2            Sony PlayStation 2"
    echo "  ps3            Sony PlayStation 3"
    echo "  xbox           Microsoft Xbox (Original)"
    echo "  xbox360        Microsoft Xbox 360"
    echo "  native         Native PC (development)"
    echo ""
    echo "Examples:"
    echo "  $0 dreamcast ps2     # Build for Dreamcast and PS2"
    echo "  $0 --all             # Build for all platforms"
    echo "  $0 --info            # Show toolchain status"
}

show_sdk_status() {
    print_header "SDK/Toolchain Status"

    echo "Checking installed SDKs..."
    echo ""

    check_sdk "KallistiOS (Dreamcast)" "${KOS_BASE:-/opt/toolchains/dc/kos}" || true
    check_sdk "PSn00bSDK (PSX)" "${PSN00BSDK:-/opt/psn00bsdk}" || true
    check_sdk "PS2SDK (PS2)" "${PS2SDK:-/usr/local/ps2dev/ps2sdk}" || true
    check_sdk "PSL1GHT (PS3)" "${PSL1GHT:-/opt/ps3dev}" || true
    check_sdk "NXDK (Xbox)" "${NXDK_DIR:-/opt/nxdk}" || true
    check_sdk "LibXenon (Xbox 360)" "${LIBXENON:-/opt/libxenon/usr}" || true

    echo ""
    echo "To install missing SDKs, see:"
    echo "  https://github.com/KallistiOS/KallistiOS"
    echo "  https://github.com/Lameguy64/PSn00bSDK"
    echo "  https://github.com/ps2dev/ps2sdk"
    echo "  https://github.com/ps3dev/ps3toolchain"
    echo "  https://github.com/XboxDev/nxdk"
    echo "  https://github.com/Free60Project/libxenon"
}

clean_all() {
    print_header "Cleaning All Build Directories"

    cd "$PROJECT_DIR"

    rm -rf build/
    make clean 2>/dev/null || true

    print_success "All build directories cleaned"
}

# Main entry point
main() {
    local platforms=()
    local do_clean=false
    local do_info=false
    local do_all=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -c|--clean)
                do_clean=true
                shift
                ;;
            -i|--info)
                do_info=true
                shift
                ;;
            -a|--all)
                do_all=true
                shift
                ;;
            dreamcast|psx|ps2|ps3|xbox|xbox360|native)
                platforms+=("$1")
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # Show SDK info if requested
    if $do_info; then
        show_sdk_status
        exit 0
    fi

    # Clean if requested
    if $do_clean; then
        clean_all
    fi

    # Build all platforms if requested
    if $do_all; then
        platforms=(dreamcast psx ps2 ps3 xbox xbox360 native)
    fi

    # Default to native if no platform specified
    if [ ${#platforms[@]} -eq 0 ]; then
        platforms=(native)
    fi

    print_header "RetroRacer Multi-Platform Build"
    echo "Platforms to build: ${platforms[*]}"

    # Build each platform
    for platform in "${platforms[@]}"; do
        case $platform in
            dreamcast)
                build_platform "Dreamcast" "Makefile"
                ;;
            psx)
                build_platform "PlayStation 1" "Makefile.psx"
                ;;
            ps2)
                build_platform "PlayStation 2" "Makefile.ps2"
                ;;
            ps3)
                build_platform "PlayStation 3" "Makefile.ps3"
                ;;
            xbox)
                build_platform "Xbox" "Makefile.xbox"
                ;;
            xbox360)
                build_platform "Xbox 360" "Makefile.xbox360"
                ;;
            native)
                build_platform "Native" "Makefile.native"
                ;;
        esac
    done

    # Print summary
    print_header "Build Summary"

    for platform in "${!BUILD_RESULTS[@]}"; do
        if [ "${BUILD_RESULTS[$platform]}" == "SUCCESS" ]; then
            echo -e "  ${GREEN}✓${NC} $platform"
        else
            echo -e "  ${RED}✗${NC} $platform"
        fi
    done

    echo ""
    echo "Build outputs in: $PROJECT_DIR/build/"
}

main "$@"
