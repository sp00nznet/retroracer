#!/bin/bash
#
# RetroRacer - KallistiOS SDK Setup Script
# Downloads and builds the complete Dreamcast development environment
#
# Usage: ./scripts/setup-kos.sh [install_dir]
#   install_dir: Where to install KOS (default: /opt/toolchains/dc)
#
# Requirements:
#   - Linux (Debian/Ubuntu, Fedora, or Arch-based)
#   - ~10GB disk space
#   - Internet connection
#   - Root access (for installing dependencies)
#

set -e

# Configuration
INSTALL_DIR="${1:-/opt/toolchains/dc}"
KOS_REPO="https://github.com/KallistiOS/KallistiOS.git"
KOS_PORTS_REPO="https://github.com/KallistiOS/kos-ports.git"
TOOLCHAIN_REPO="https://github.com/KallistiOS/dc-chain.git"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect package manager and install dependencies
install_dependencies() {
    log_info "Installing build dependencies..."

    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            git \
            texinfo \
            libjpeg-dev \
            libpng-dev \
            libelf-dev \
            curl \
            wget \
            gawk \
            patch \
            bzip2 \
            tar \
            make \
            libgmp-dev \
            libmpfr-dev \
            libmpc-dev \
            gettext \
            libz-dev \
            python3 \
            cmake \
            subversion
    elif command -v dnf &> /dev/null; then
        # Fedora
        sudo dnf install -y \
            gcc \
            gcc-c++ \
            make \
            git \
            texinfo \
            libjpeg-turbo-devel \
            libpng-devel \
            elfutils-libelf-devel \
            curl \
            wget \
            gawk \
            patch \
            bzip2 \
            tar \
            gmp-devel \
            mpfr-devel \
            libmpc-devel \
            gettext \
            zlib-devel \
            python3 \
            cmake \
            subversion
    elif command -v pacman &> /dev/null; then
        # Arch Linux
        sudo pacman -Sy --noconfirm \
            base-devel \
            git \
            texinfo \
            libjpeg-turbo \
            libpng \
            libelf \
            curl \
            wget \
            gawk \
            patch \
            bzip2 \
            tar \
            gmp \
            mpfr \
            libmpc \
            gettext \
            zlib \
            python \
            cmake \
            subversion
    else
        log_error "Unsupported package manager. Please install dependencies manually."
        log_info "Required: gcc, g++, make, git, texinfo, libjpeg-dev, libpng-dev, libelf-dev"
        exit 1
    fi

    log_info "Dependencies installed successfully"
}

# Create directory structure
setup_directories() {
    log_info "Creating directory structure at ${INSTALL_DIR}..."

    sudo mkdir -p "${INSTALL_DIR}"
    sudo chown -R "${USER}:${USER}" "${INSTALL_DIR}"

    mkdir -p "${INSTALL_DIR}/kos"
    mkdir -p "${INSTALL_DIR}/kos-ports"
    mkdir -p "${INSTALL_DIR}/toolchain"
}

# Download and build the SH4 cross-compiler toolchain
build_toolchain() {
    log_info "Building Dreamcast toolchain (this will take a while)..."

    cd "${INSTALL_DIR}"

    if [ ! -d "dc-chain" ]; then
        git clone ${TOOLCHAIN_REPO}
    fi

    cd dc-chain

    # Create configuration
    cp config.mk.stable.sample config.mk

    # Modify config for our paths
    sed -i "s|/opt/toolchains/dc|${INSTALL_DIR}|g" config.mk

    # Download and build toolchain
    log_info "Downloading toolchain sources..."
    make download

    log_info "Building SH4 toolchain (this takes 30-60 minutes)..."
    make build-sh4

    log_info "Building ARM toolchain for sound processor..."
    make build-arm

    log_info "Toolchain built successfully"
}

# Download and configure KallistiOS
setup_kos() {
    log_info "Setting up KallistiOS..."

    cd "${INSTALL_DIR}"

    if [ ! -d "kos" ]; then
        git clone ${KOS_REPO} kos
    fi

    cd kos

    # Copy and configure environ.sh
    cp doc/environ.sh.sample environ.sh

    # Update paths in environ.sh
    sed -i "s|/opt/toolchains/dc|${INSTALL_DIR}|g" environ.sh
    sed -i "s|export KOS_BASE=.*|export KOS_BASE=\"${INSTALL_DIR}/kos\"|g" environ.sh
    sed -i "s|export KOS_PORTS=.*|export KOS_PORTS=\"${INSTALL_DIR}/kos-ports\"|g" environ.sh

    # Source environment and build KOS
    log_info "Building KallistiOS..."
    source environ.sh
    make

    log_info "KallistiOS built successfully"
}

# Download kos-ports (additional libraries)
setup_kos_ports() {
    log_info "Setting up KOS-ports..."

    cd "${INSTALL_DIR}"

    if [ ! -d "kos-ports" ]; then
        git clone ${KOS_PORTS_REPO} kos-ports
    fi

    log_info "KOS-ports downloaded (build individual ports as needed)"
}

# Create environment setup script
create_env_script() {
    log_info "Creating environment script..."

    cat > "${INSTALL_DIR}/setup-env.sh" << EOF
#!/bin/bash
# Source this file to set up the Dreamcast development environment
# Usage: source ${INSTALL_DIR}/setup-env.sh

export KOS_BASE="${INSTALL_DIR}/kos"
export KOS_PORTS="${INSTALL_DIR}/kos-ports"

# Source KOS environment
if [ -f "\${KOS_BASE}/environ.sh" ]; then
    source "\${KOS_BASE}/environ.sh"
    echo "Dreamcast development environment loaded"
    echo "  KOS_BASE: \${KOS_BASE}"
    echo "  KOS_CC: \${KOS_CC}"
else
    echo "Error: KOS environ.sh not found"
fi
EOF

    chmod +x "${INSTALL_DIR}/setup-env.sh"
}

# Create helper script for the project
create_project_helper() {
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"

    cat > "${PROJECT_DIR}/scripts/env.sh" << EOF
#!/bin/bash
# Source this to set up build environment for RetroRacer
# Usage: source scripts/env.sh

KOS_INSTALL="${INSTALL_DIR}"

if [ -f "\${KOS_INSTALL}/setup-env.sh" ]; then
    source "\${KOS_INSTALL}/setup-env.sh"
else
    echo "Error: KallistiOS not found at \${KOS_INSTALL}"
    echo "Run ./scripts/setup-kos.sh first"
fi
EOF

    chmod +x "${PROJECT_DIR}/scripts/env.sh"
}

# Main installation process
main() {
    echo "========================================"
    echo "  KallistiOS SDK Setup for Dreamcast"
    echo "========================================"
    echo ""
    echo "Install directory: ${INSTALL_DIR}"
    echo ""
    echo "This script will:"
    echo "  1. Install build dependencies"
    echo "  2. Build SH4 and ARM cross-compilers"
    echo "  3. Build KallistiOS"
    echo "  4. Download kos-ports"
    echo ""
    echo "This process takes 1-2 hours and requires ~10GB of disk space."
    echo ""
    read -p "Continue? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 0
    fi

    install_dependencies
    setup_directories
    build_toolchain
    setup_kos
    setup_kos_ports
    create_env_script
    create_project_helper

    echo ""
    echo "========================================"
    echo "  Installation Complete!"
    echo "========================================"
    echo ""
    echo "To use the Dreamcast toolchain, run:"
    echo "  source ${INSTALL_DIR}/setup-env.sh"
    echo ""
    echo "Or for this project specifically:"
    echo "  source scripts/env.sh"
    echo ""
    echo "Then build RetroRacer with:"
    echo "  ./scripts/build.sh"
    echo ""
}

main "$@"
