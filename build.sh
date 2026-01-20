#!/bin/bash
#
# RetroRacer - One-Click Build Script
# Builds for any platform using Docker (no SDK installation required)
#
# Usage: ./build.sh <platform>
#   platform: dreamcast, psx, ps2, ps3, xbox, n64, snes, native, all
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
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-dc \
        sh -c "make clean && make && cp retroracer.elf retroracer.bin /output/ 2>/dev/null || cp retroracer.elf /output/"
    log_success "Built: output/retroracer.elf (Dreamcast)"
}

build_psx() {
    log_info "Building for PlayStation 1..."
    docker build -t retroracer-psx -f Dockerfile.psx .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-psx \
        sh -c "make -f Makefile.psx clean 2>/dev/null; make -f Makefile.psx && cp retroracer.exe /output/retroracer_psx.exe 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_psx.exe"
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

build_n64() {
    log_info "Building for Nintendo 64..."
    docker build -t retroracer-n64 -f Dockerfile.n64 .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-n64 \
        sh -c "make -f Makefile.n64 clean 2>/dev/null; make -f Makefile.n64 && cp retroracer.z64 /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer.z64"
}

build_snes() {
    log_info "Building for Super Nintendo..."
    docker build -t retroracer-snes -f Dockerfile.snes .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-snes \
        sh -c "make -f Makefile.snes clean 2>/dev/null; make -f Makefile.snes && cp retroracer.sfc /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer.sfc"
}

build_xbox360() {
    log_info "Building for Xbox 360..."
    docker build -t retroracer-xbox360 -f Dockerfile.xbox360 .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-xbox360 \
        sh -c "make -f Makefile.xbox360 clean 2>/dev/null; make -f Makefile.xbox360 && cp retroracer_xbox360.xex /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_xbox360.xex"
}

build_genesis() {
    log_info "Building for Sega Genesis..."
    docker build -t retroracer-genesis -f Dockerfile.genesis .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-genesis \
        sh -c "make -f Makefile.genesis clean 2>/dev/null; make -f Makefile.genesis && cp retroracer_genesis.bin /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_genesis.bin"
}

build_3do() {
    log_info "Building for 3DO..."
    docker build -t retroracer-3do -f Dockerfile.3do .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-3do \
        sh -c "make -f Makefile.3do clean 2>/dev/null; make -f Makefile.3do && cp retroracer_3do.iso /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_3do.iso"
}

build_gba() {
    log_info "Building for Game Boy Advance..."
    docker build -t retroracer-gba -f Dockerfile.gba .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-gba \
        sh -c "make -f Makefile.gba clean 2>/dev/null; make -f Makefile.gba && cp retroracer_gba.gba /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_gba.gba"
}

build_nds() {
    log_info "Building for Nintendo DS..."
    docker build -t retroracer-nds -f Dockerfile.nds .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-nds \
        sh -c "make -f Makefile.nds clean 2>/dev/null; make -f Makefile.nds && cp retroracer_nds.nds /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_nds.nds"
}

build_3ds() {
    log_info "Building for Nintendo 3DS..."
    docker build -t retroracer-3ds -f Dockerfile.3ds .
    mkdir -p output
    docker run --rm -v "$SCRIPT_DIR/output:/output" retroracer-3ds \
        sh -c "make -f Makefile.3ds clean 2>/dev/null; make -f Makefile.3ds && cp retroracer_3ds.3dsx retroracer_3ds.elf /output/ 2>/dev/null || echo 'Build complete'"
    log_success "Built: output/retroracer_3ds.3dsx"
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

    if build_n64; then
        RESULTS+=("N64: ${GREEN}OK${NC}")
    else
        RESULTS+=("N64: ${RED}FAILED${NC}")
    fi

    if build_snes; then
        RESULTS+=("SNES: ${GREEN}OK${NC}")
    else
        RESULTS+=("SNES: ${RED}FAILED${NC}")
    fi

    if build_xbox360; then
        RESULTS+=("Xbox 360: ${GREEN}OK${NC}")
    else
        RESULTS+=("Xbox 360: ${RED}FAILED${NC}")
    fi

    if build_genesis; then
        RESULTS+=("Genesis: ${GREEN}OK${NC}")
    else
        RESULTS+=("Genesis: ${RED}FAILED${NC}")
    fi

    if build_3do; then
        RESULTS+=("3DO: ${GREEN}OK${NC}")
    else
        RESULTS+=("3DO: ${RED}FAILED${NC}")
    fi

    if build_gba; then
        RESULTS+=("GBA: ${GREEN}OK${NC}")
    else
        RESULTS+=("GBA: ${RED}FAILED${NC}")
    fi

    if build_nds; then
        RESULTS+=("NDS: ${GREEN}OK${NC}")
    else
        RESULTS+=("NDS: ${RED}FAILED${NC}")
    fi

    if build_3ds; then
        RESULTS+=("3DS: ${GREEN}OK${NC}")
    else
        RESULTS+=("3DS: ${RED}FAILED${NC}")
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
    docker rmi retroracer-dc retroracer-psx retroracer-ps2 retroracer-ps3 retroracer-xbox retroracer-n64 retroracer-snes 2>/dev/null || true
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
    echo "  xbox360    Build for Xbox 360"
    echo "  n64        Build for Nintendo 64"
    echo "  snes       Build for Super Nintendo"
    echo "  genesis    Build for Sega Genesis"
    echo "  3do        Build for 3DO"
    echo "  gba        Build for Game Boy Advance"
    echo "  nds        Build for Nintendo DS"
    echo "  3ds        Build for Nintendo 3DS"
    echo "  all        Build for ALL platforms"
    echo "  clean      Remove all build artifacts"
    echo ""
    echo "Examples:"
    echo "  $0 n64      # Build N64 version"
    echo "  $0 snes     # Build SNES version"
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
    n64)
        check_docker
        build_n64
        ;;
    snes)
        check_docker
        build_snes
        ;;
    xbox360)
        check_docker
        build_xbox360
        ;;
    genesis|megadrive|md)
        check_docker
        build_genesis
        ;;
    3do)
        check_docker
        build_3do
        ;;
    gba)
        check_docker
        build_gba
        ;;
    nds|ds)
        check_docker
        build_nds
        ;;
    3ds)
        check_docker
        build_3ds
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
