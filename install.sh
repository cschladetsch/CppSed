#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if [[ -z "${PREFIX:-}" ]]; then
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        PREFIX="/usr/local"
    else
        PREFIX="${HOME}/.local"
    fi
fi

choose_build_dir() {
    if [[ -n "${FASTSED_BUILD_DIR:-}" ]]; then
        printf '%s' "${FASTSED_BUILD_DIR}"
        return
    fi
    if [[ -d Bin && -w Bin ]]; then
        printf '%s' Bin
        return
    fi
    printf '%s' .fastsed-build
}

BUILD_DIR="$(choose_build_dir)"
REBUILD=0
DO_BUILD=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        --rebuild)
            REBUILD=1
            shift
            ;;
        --no-build)
            DO_BUILD=0
            shift
            ;;
        *)
            echo "[install] unknown argument: $1"
            echo "usage: ./install.sh [--prefix DIR] [--rebuild] [--no-build]"
            exit 1
            ;;
    esac
done

if [[ "$DO_BUILD" -eq 1 ]]; then
    build_args=(--prefix "$PREFIX" "Release")
    if [[ "$REBUILD" -eq 1 ]]; then
        build_args=(--rebuild --prefix "$PREFIX" "Release")
    fi
    FASTSED_IPO=ON ./b "${build_args[@]}"
else
    echo "[install] prefix: $PREFIX"
    cmake --install "${BUILD_DIR}" --prefix "$PREFIX"
    echo "[install] installed:"
    echo "[install]   $PREFIX/bin/fsed"
    echo "[install]   $PREFIX/share/man/man1/fsed.1"
fi
