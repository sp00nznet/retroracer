#!/bin/bash
#
# RetroRacer - Build Script
# Builds the game for Dreamcast
#
# Usage: ./scripts/build.sh [target]
#   Targets:
#     all     - Build the ELF executable (default)
#     clean   - Clean build artifacts
#     cdi     - Build CDI disc image for burning
#     run     - Build and run in emulator (requires lxdream or flycast)
#

set -e

# Get script and project directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[BUILD]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if KOS environment is set up
check_environment() {
    if [ -z "${KOS_BASE}" ]; then
        log_warn "KOS_BASE not set, attempting to source environment..."

        # Try common locations
        if [ -f "${SCRIPT_DIR}/env.sh" ]; then
            source "${SCRIPT_DIR}/env.sh"
        elif [ -f "/opt/toolchains/dc/setup-env.sh" ]; then
            source "/opt/toolchains/dc/setup-env.sh"
        elif [ -f "${HOME}/dc/setup-env.sh" ]; then
            source "${HOME}/dc/setup-env.sh"
        fi
    fi

    if [ -z "${KOS_BASE}" ]; then
        log_error "KallistiOS environment not found!"
        echo ""
        echo "Please either:"
        echo "  1. Run: source /path/to/kos/environ.sh"
        echo "  2. Run: ./scripts/setup-kos.sh (to install KOS)"
        echo ""
        exit 1
    fi

    if [ ! -f "${KOS_BASE}/environ.sh" ]; then
        log_error "KOS_BASE is set but environ.sh not found"
        exit 1
    fi

    log_info "Using KallistiOS at: ${KOS_BASE}"
}

# Build the ELF executable
build_elf() {
    log_info "Building RetroRacer..."

    cd "${PROJECT_DIR}"

    # Clean first if requested
    if [ "$1" = "clean" ]; then
        make clean
    fi

    # Build
    make all

    if [ -f "retroracer.elf" ]; then
        log_info "Build successful: retroracer.elf"
        ls -lh retroracer.elf
    else
        log_error "Build failed - retroracer.elf not created"
        exit 1
    fi
}

# Clean build artifacts
clean_build() {
    log_info "Cleaning build artifacts..."
    cd "${PROJECT_DIR}"
    make clean
    rm -f retroracer.bin 1ST_READ.BIN retroracer.iso retroracer.cdi
    log_info "Clean complete"
}

# Build CDI disc image
build_cdi() {
    log_info "Building CDI disc image..."

    cd "${PROJECT_DIR}"

    # First build the ELF
    build_elf

    # Check for required tools
    if ! command -v mkisofs &> /dev/null; then
        log_warn "mkisofs not found, trying genisoimage..."
        if ! command -v genisoimage &> /dev/null; then
            log_error "Neither mkisofs nor genisoimage found. Install with:"
            echo "  sudo apt install genisoimage"
            exit 1
        fi
        MKISOFS="genisoimage"
    else
        MKISOFS="mkisofs"
    fi

    # Convert ELF to binary
    log_info "Converting ELF to binary..."
    ${KOS_OBJCOPY} -R .stack -O binary retroracer.elf retroracer.bin

    # Scramble for Dreamcast
    log_info "Scrambling binary..."
    if [ -f "${KOS_BASE}/../IP.BIN" ]; then
        IP_BIN="${KOS_BASE}/../IP.BIN"
    elif [ -f "${KOS_BASE}/IP.BIN" ]; then
        IP_BIN="${KOS_BASE}/IP.BIN"
    else
        log_warn "IP.BIN not found, creating basic one..."
        # Create a minimal IP.BIN (bootstrap)
        create_ip_bin
        IP_BIN="IP.BIN"
    fi

    # Use scramble tool from KOS
    if [ -f "${KOS_BASE}/utils/scramble/scramble" ]; then
        ${KOS_BASE}/utils/scramble/scramble retroracer.bin 1ST_READ.BIN
    elif command -v scramble &> /dev/null; then
        scramble retroracer.bin 1ST_READ.BIN
    else
        log_warn "scramble tool not found, copying unscrambled..."
        cp retroracer.bin 1ST_READ.BIN
    fi

    # Create ISO
    log_info "Creating ISO..."
    ${MKISOFS} -C 0,11702 -V "RETRORACER" -G "${IP_BIN}" -l -o retroracer.iso .

    # Convert to CDI
    log_info "Converting to CDI..."
    if command -v cdi4dc &> /dev/null; then
        cdi4dc retroracer.iso retroracer.cdi
        log_info "CDI image created: retroracer.cdi"
        ls -lh retroracer.cdi
    else
        log_warn "cdi4dc not found, ISO created instead: retroracer.iso"
        log_info "Install cdi4dc to create CDI images"
        ls -lh retroracer.iso
    fi
}

# Create a basic IP.BIN if not found
create_ip_bin() {
    log_info "Generating IP.BIN..."

    # This creates a minimal valid IP.BIN
    # In practice, you'd want to use makeip from KOS
    if [ -f "${KOS_BASE}/utils/makeip/makeip" ]; then
        cat > ip.txt << EOF
Hardware ID   : SEGA SEGAKATANA
Maker ID      : SEGA ENTERPRISES
Device Info   : 0000 CD-ROM1/1
Area Symbols  : JUE
Peripherals   : E000F10
Product No    : T0000
Version       : V1.000
Release Date  : $(date +%Y%m%d)
Boot Filename : 1ST_READ.BIN
SW Maker Name : RETRORACER
Game Title    : RETRORACER
EOF
        ${KOS_BASE}/utils/makeip/makeip ip.txt IP.BIN
        rm ip.txt
    else
        log_error "makeip not found, cannot create IP.BIN"
        exit 1
    fi
}

# Run in emulator
run_emulator() {
    build_elf

    cd "${PROJECT_DIR}"

    # Try different emulators
    if command -v flycast &> /dev/null; then
        log_info "Running in Flycast..."
        flycast retroracer.elf
    elif command -v lxdream &> /dev/null; then
        log_info "Running in lxdream..."
        lxdream -e retroracer.elf
    elif command -v reicast &> /dev/null; then
        log_info "Running in Reicast..."
        reicast retroracer.elf
    else
        log_error "No Dreamcast emulator found!"
        echo ""
        echo "Install one of the following:"
        echo "  - Flycast (recommended): https://github.com/flyinghead/flycast"
        echo "  - lxdream: https://github.com/lxdream/lxdream"
        echo ""
        exit 1
    fi
}

# Show usage
show_usage() {
    echo "RetroRacer Build Script"
    echo ""
    echo "Usage: $0 [target]"
    echo ""
    echo "Targets:"
    echo "  all     - Build the ELF executable (default)"
    echo "  clean   - Clean build artifacts"
    echo "  cdi     - Build CDI disc image for burning"
    echo "  run     - Build and run in emulator"
    echo "  help    - Show this help message"
    echo ""
    echo "Environment:"
    echo "  KOS_BASE must be set, or run 'source scripts/env.sh' first"
    echo ""
}

# Main
main() {
    TARGET="${1:-all}"

    case "${TARGET}" in
        all|build)
            check_environment
            build_elf
            ;;
        clean)
            clean_build
            ;;
        cdi|disc|image)
            check_environment
            build_cdi
            ;;
        run|emulator|test)
            check_environment
            run_emulator
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            log_error "Unknown target: ${TARGET}"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"
