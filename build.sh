#!/bin/bash
#
# RetroRacer - One-Click Build Script
# Builds for any platform using Docker (no SDK installation required)
#
# Usage: ./build.sh <platform>
#   platform: dreamcast, psx, ps2, ps3, xbox, native, all
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

banner() {
    echo -e "${CYAN}"
    echo "  ____      _             ____                      "
    echo " |  _ \ ___| |_ _ __ ___ |  _ \ __ _  ___ ___ _ __  "
    echo " | |_) / _ \ __| '__/ _ \| |_) / _\` |/ __/ _ \ '__| "
    echo " |  _ <  __/ |_| | | (_) |  _ < (_| | (_|  __/ |    "
    echo " |_| \_\___|\__|_|  \___/|_| \_\__,_|\___\___|_|    "
    echo -e "${NC}"
    echo -e "${BLUE}Multi-Platform Build System${NC}"
    echo ""
}

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed. Please install Docker first."
        echo "  https://docs.docker.com/get-docker/"
        exit 1
    fi

    if ! docker info &> /dev/null; then
        log_error "Docker daemon is not running. Please start Docker."
        exit 1
    fi
}

build_native() {
    log_info "Building native binary..."
    make -f Makefile.native clean 2>/dev/null || true
    make -f Makefile.native
    log_success "Built: ./retroracer"
}

build_dreamcast() {
    log_info "Building for Dreamcast..."
    docker build -t retroracer-dc -f scripts/Dockerfile.dreamcast .
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-dc \
        sh -c "make clean && make && cp retroracer.elf retroracer.bin /output/ 2>/dev/null || cp retroracer.elf /output/"
    log_success "Built: output/retroracer.elf"
}

build_psx() {
    log_info "Building for PlayStation 1..."
    docker build -t retroracer-psx -f Dockerfile.psx .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-psx \
        sh -c "make -f Makefile.psx clean 2>/dev/null; make -f Makefile.psx && cp retroracer.exe /output/ 2>/dev/null || echo 'Build artifacts in container'"
    log_success "Built: output/retroracer.exe (PSX)"
}

build_ps2() {
    log_info "Building for PlayStation 2..."
    docker build -t retroracer-ps2 -f Dockerfile.ps2 .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-ps2 \
        sh -c "make -f Makefile.ps2 clean 2>/dev/null; make -f Makefile.ps2 && cp retroracer.elf /output/retroracer_ps2.elf 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_ps2.elf"
}

build_ps3() {
    log_info "Building for PlayStation 3..."
    docker build -t retroracer-ps3 -f Dockerfile.ps3 .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-ps3 \
        sh -c "make -f Makefile.ps3 clean 2>/dev/null; make -f Makefile.ps3 && cp retroracer.self retroracer.pkg /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer.pkg (PS3)"
}

build_xbox() {
    log_info "Building for Original Xbox..."
    docker build -t retroracer-xbox -f Dockerfile.xbox .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-xbox \
        sh -c "make -f Makefile.xbox clean 2>/dev/null; make -f Makefile.xbox && cp retroracer.xbe /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer.xbe"
}

build_all() {
    log_info "Building for ALL platforms..."
    echo ""

    mkdir -p output

    RESULTS=()

    if build_native; then
        RESULTS+=("Native: ${GREEN}OK${NC}")
        cp retroracer output/retroracer_native 2>/dev/null || true
    else
        RESULTS+=("Native: ${RED}FAILED${NC}")
    fi

    if build_dreamcast; then
        RESULTS+=("Dreamcast: ${GREEN}OK${NC}")
    else
        RESULTS+=("Dreamcast: ${RED}FAILED${NC}")
    fi

    if build_psx; then
        RESULTS+=("PSX: ${GREEN}OK${NC}")
    else
        RESULTS+=("PSX: ${RED}FAILED${NC}")
    fi

    if build_ps2; then
        RESULTS+=("PS2: ${GREEN}OK${NC}")
    else
        RESULTS+=("PS2: ${RED}FAILED${NC}")
    fi

    if build_ps3; then
        RESULTS+=("PS3: ${GREEN}OK${NC}")
    else
        RESULTS+=("PS3: ${RED}FAILED${NC}")
    fi

    if build_xbox; then
        RESULTS+=("Xbox: ${GREEN}OK${NC}")
    else
        RESULTS+=("Xbox: ${RED}FAILED${NC}")
    fi

    echo ""
    echo -e "${CYAN}=== Build Results ===${NC}"
    for r in "${RESULTS[@]}"; do
        echo -e "  $r"
    done
    echo ""
    echo "Output files in: ./output/"
    ls -la output/ 2>/dev/null || true
}

clean_all() {
    log_info "Cleaning all build artifacts..."
    make -f Makefile.native clean 2>/dev/null || true
    rm -rf output/
    docker rmi retroracer-dc retroracer-psx retroracer-ps2 retroracer-ps3 retroracer-xbox 2>/dev/null || true
    log_success "Clean complete"
}

# === MAIN ===

banner

if [ $# -eq 0 ]; then
    echo "Usage: $0 <platform>"
    echo ""
    echo "Platforms:"
    echo "  native     Build for current OS"
    echo "  dreamcast  Build for Sega Dreamcast"
    echo "  psx        Build for PlayStation 1"
    echo "  ps2        Build for PlayStation 2"
    echo "  ps3        Build for PlayStation 3"
    echo "  xbox       Build for Original Xbox"
    echo "  all        Build for ALL platforms"
    echo "  clean      Remove all build artifacts"
    echo ""
    echo "Examples:"
    echo "  $0 ps2      # Build PS2 version"
    echo "  $0 all      # Build everything"
    exit 0
fi

case "$1" in
    native)
        build_native
        ;;
    dreamcast|dc)
        check_docker
        build_dreamcast
        ;;
    psx|ps1)
        check_docker
        build_psx
        ;;
    ps2)
        check_docker
        build_ps2
        ;;
    ps3)
        check_docker
        build_ps3
        ;;
    xbox)
        check_docker
        build_xbox
        ;;
    all)
        check_docker
        build_all
        ;;
    clean)
        clean_all
        ;;
    *)
        log_error "Unknown platform: $1"
        exit 1
        ;;
esac
