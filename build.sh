#!/usr/bin/env bash
# build.sh — build script for Linux / WSL2
#
# Usage:
#   ./build.sh [OPTIONS]
#
# Options:
#   --deps              Install system dependencies (apt) then exit
#   --build             Configure and build (default when no action flag given)
#   --jobs N            Parallel compile jobs (default: all logical cores)
#   --video FILE        Embed FILE as assets/video.mp4 before building
#   --vcpkg DIR         Use vcpkg at DIR instead of system apt packages
#   --help              Show this help
#
# Examples:
#   ./build.sh --deps                          # install deps only
#   ./build.sh --build                         # build with all cores
#   ./build.sh --build --jobs 4                # build with 4 threads
#   ./build.sh --deps --build --video vid.mp4  # full first-time setup
#   ./build.sh --build --vcpkg ~/vcpkg         # use vcpkg instead of apt

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# ── Defaults ─────────────────────────────────────────────────────────────
DO_DEPS=false
DO_BUILD=false
JOBS="$(nproc)"
VIDEO_PATH=""
VCPKG_ROOT="${VCPKG_ROOT:-}"
USE_SYSTEM_PKGS=true

# ── Argument parsing ──────────────────────────────────────────────────────
if [[ $# -eq 0 ]]; then
    DO_BUILD=true   # bare invocation → just build
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --deps)   DO_DEPS=true;  shift ;;
        --build)  DO_BUILD=true; shift ;;
        --jobs)
            if [[ -z "${2:-}" || ! "${2}" =~ ^[1-9][0-9]*$ ]]; then
                echo "ERROR: --jobs requires a positive integer." >&2; exit 1
            fi
            JOBS="$2"; shift 2 ;;
        --video)
            if [[ -z "${2:-}" ]]; then
                echo "ERROR: --video requires a file path." >&2; exit 1
            fi
            VIDEO_PATH="$2"; shift 2 ;;
        --vcpkg)
            if [[ -z "${2:-}" ]]; then
                echo "ERROR: --vcpkg requires a directory path." >&2; exit 1
            fi
            VCPKG_ROOT="$2"; USE_SYSTEM_PKGS=false; shift 2 ;;
        --help|-h)
            sed -n '3,20p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        *) echo "ERROR: Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# If --vcpkg was given without an explicit action, still build
if [[ "$DO_DEPS" == false && "$DO_BUILD" == false ]]; then
    DO_BUILD=true
fi

echo "[build] Jobs: ${JOBS} thread(s)"

# ── Install dependencies ──────────────────────────────────────────────────
CMAKE_EXTRA_ARGS=()

if [[ "$DO_DEPS" == true ]]; then
    if [[ "$USE_SYSTEM_PKGS" == true ]]; then
        echo "[build] Installing system packages via apt..."
        sudo apt-get update -qq
        sudo apt-get install -y --no-install-recommends \
            cmake \
            build-essential \
            pkg-config \
            libsdl2-dev \
            libavcodec-dev \
            libavformat-dev \
            libavutil-dev \
            libswscale-dev
        echo "[build] Dependencies installed."
    else
        if [[ ! -f "${VCPKG_ROOT}/vcpkg" ]]; then
            echo "ERROR: vcpkg executable not found at ${VCPKG_ROOT}/vcpkg" >&2; exit 1
        fi
        echo "[build] Installing vcpkg packages (x64-linux) with ${JOBS} thread(s)..."
        VCPKG_MAX_CONCURRENCY="${JOBS}" \
            "${VCPKG_ROOT}/vcpkg" install sdl2:x64-linux ffmpeg:x64-linux
        echo "[build] vcpkg packages installed."
    fi
fi

if [[ "$USE_SYSTEM_PKGS" == false ]]; then
    if [[ ! -f "${VCPKG_ROOT}/vcpkg" ]]; then
        echo "ERROR: vcpkg executable not found at ${VCPKG_ROOT}/vcpkg" >&2; exit 1
    fi
    CMAKE_EXTRA_ARGS=(
        "-DVCPKG_ROOT=${VCPKG_ROOT}"
        "-DVCPKG_TARGET_TRIPLET=x64-linux"
        "-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    )
fi

# ── Build ─────────────────────────────────────────────────────────────────
if [[ "$DO_BUILD" == true ]]; then

    # Copy video if supplied
    if [[ -n "$VIDEO_PATH" ]]; then
        echo "[build] Copying video: $VIDEO_PATH -> assets/video.mp4"
        mkdir -p "${SCRIPT_DIR}/assets"
        cmake -E copy_if_different "$VIDEO_PATH" "${SCRIPT_DIR}/assets/video.mp4"
    fi

    if [[ ! -f "${SCRIPT_DIR}/assets/video.mp4" ]]; then
        echo "WARNING: assets/video.mp4 not found."
        echo "         The app will show an error at runtime until you embed a video."
        echo "         Re-run: ./build.sh --build --video /path/to/your_video.mp4"
    fi

    # Configure
    echo "[build] Configuring..."
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        "${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"}"

    # Compile
    echo "[build] Compiling with ${JOBS} thread(s)..."
    cmake --build "${BUILD_DIR}" --config Release --parallel "${JOBS}"

    echo ""
    echo "[build] Done!  Binary: ${BUILD_DIR}/breakdance"
    echo "        Run:   ${BUILD_DIR}/breakdance"
    echo "        (WSL2: WSLg works out of the box; otherwise export DISPLAY=<host-ip>:0)"
fi
