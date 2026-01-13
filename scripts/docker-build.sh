#!/bin/bash
#
# RetroRacer - Docker Build Script
# Build the game using a pre-configured Docker container
# No local KallistiOS installation required!
#
# Usage: ./scripts/docker-build.sh [target]
#   Targets:
#     all     - Build the ELF executable (default)
#     clean   - Clean build artifacts
#     shell   - Open interactive shell in build container
#

set -e

# Get script and project directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"

# Docker image with KallistiOS pre-installed
# Using the official KallistiOS docker image
DOCKER_IMAGE="kazade/dreamcast-sdk:latest"

# Alternative images if the above doesn't work:
# DOCKER_IMAGE="ghcr.io/kallistios/kos-toolchain:latest"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[DOCKER]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check Docker is available
check_docker() {
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed!"
        echo ""
        echo "Install Docker from: https://docs.docker.com/get-docker/"
        exit 1
    fi

    if ! docker info &> /dev/null; then
        log_error "Docker daemon is not running or you don't have permission"
        echo ""
        echo "Try: sudo systemctl start docker"
        echo "Or add yourself to the docker group: sudo usermod -aG docker \$USER"
        exit 1
    fi
}

# Pull the Docker image
pull_image() {
    log_info "Pulling KallistiOS Docker image..."
    log_info "Image: ${DOCKER_IMAGE}"

    if ! docker pull "${DOCKER_IMAGE}"; then
        log_warn "Failed to pull ${DOCKER_IMAGE}, trying alternative..."
        DOCKER_IMAGE="ghcr.io/kallistios/kos-toolchain:latest"
        docker pull "${DOCKER_IMAGE}" || {
            log_error "Failed to pull Docker image"
            exit 1
        }
    fi

    log_info "Image ready"
}

# Run build in container
docker_build() {
    local target="${1:-all}"

    log_info "Building RetroRacer (target: ${target})..."

    docker run --rm \
        -v "${PROJECT_DIR}:/src" \
        -w /src \
        -e "KOS_BASE=/opt/toolchains/dc/kos" \
        "${DOCKER_IMAGE}" \
        bash -c "source /opt/toolchains/dc/kos/environ.sh && make ${target}"

    if [ "${target}" = "all" ] && [ -f "${PROJECT_DIR}/retroracer.elf" ]; then
        log_info "Build successful!"
        ls -lh "${PROJECT_DIR}/retroracer.elf"
    fi
}

# Clean build
docker_clean() {
    log_info "Cleaning build artifacts..."

    docker run --rm \
        -v "${PROJECT_DIR}:/src" \
        -w /src \
        "${DOCKER_IMAGE}" \
        make clean

    rm -f "${PROJECT_DIR}/retroracer.bin" \
          "${PROJECT_DIR}/1ST_READ.BIN" \
          "${PROJECT_DIR}/retroracer.iso" \
          "${PROJECT_DIR}/retroracer.cdi"

    log_info "Clean complete"
}

# Interactive shell
docker_shell() {
    log_info "Opening interactive shell in build container..."
    log_info "Type 'exit' to leave the container"
    echo ""

    docker run -it --rm \
        -v "${PROJECT_DIR}:/src" \
        -w /src \
        -e "KOS_BASE=/opt/toolchains/dc/kos" \
        "${DOCKER_IMAGE}" \
        bash -c "source /opt/toolchains/dc/kos/environ.sh && exec bash"
}

# Show usage
show_usage() {
    echo "RetroRacer Docker Build Script"
    echo ""
    echo "Build the game without installing KallistiOS locally."
    echo "Requires Docker to be installed."
    echo ""
    echo "Usage: $0 [target]"
    echo ""
    echo "Targets:"
    echo "  all     - Build the ELF executable (default)"
    echo "  clean   - Clean build artifacts"
    echo "  shell   - Open interactive shell in build container"
    echo "  pull    - Just pull/update the Docker image"
    echo "  help    - Show this help message"
    echo ""
}

# Main
main() {
    TARGET="${1:-all}"

    check_docker

    case "${TARGET}" in
        all|build)
            pull_image
            docker_build all
            ;;
        clean)
            docker_clean
            ;;
        shell|bash)
            pull_image
            docker_shell
            ;;
        pull|update)
            pull_image
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
