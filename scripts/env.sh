#!/bin/bash
#
# RetroRacer - Environment Setup
# Source this file to set up the build environment
#
# Usage: source scripts/env.sh
#

# Common KallistiOS installation paths
KOS_PATHS=(
    "/opt/toolchains/dc"
    "${HOME}/opt/toolchains/dc"
    "${HOME}/dc"
    "${HOME}/dreamcast"
    "/usr/local/dc"
)

# Find KallistiOS installation
find_kos() {
    for path in "${KOS_PATHS[@]}"; do
        if [ -f "${path}/kos/environ.sh" ]; then
            echo "${path}"
            return 0
        fi
        if [ -f "${path}/setup-env.sh" ]; then
            echo "${path}"
            return 0
        fi
    done
    return 1
}

# Main setup
setup_environment() {
    if [ -n "${KOS_BASE}" ]; then
        echo "KallistiOS environment already configured"
        echo "  KOS_BASE: ${KOS_BASE}"
        return 0
    fi

    KOS_INSTALL=$(find_kos)

    if [ -z "${KOS_INSTALL}" ]; then
        echo "Error: KallistiOS not found!"
        echo ""
        echo "Looked in:"
        for path in "${KOS_PATHS[@]}"; do
            echo "  - ${path}"
        done
        echo ""
        echo "Options:"
        echo "  1. Install KallistiOS: ./scripts/setup-kos.sh"
        echo "  2. Use Docker build:   ./scripts/docker-build.sh"
        echo "  3. Set KOS_BASE manually and source \${KOS_BASE}/environ.sh"
        echo ""
        return 1
    fi

    if [ -f "${KOS_INSTALL}/setup-env.sh" ]; then
        source "${KOS_INSTALL}/setup-env.sh"
    elif [ -f "${KOS_INSTALL}/kos/environ.sh" ]; then
        source "${KOS_INSTALL}/kos/environ.sh"
    fi

    if [ -n "${KOS_BASE}" ]; then
        echo "Dreamcast development environment loaded"
        echo "  KOS_BASE: ${KOS_BASE}"
        echo "  KOS_CC:   ${KOS_CC:-not set}"
    fi
}

setup_environment
