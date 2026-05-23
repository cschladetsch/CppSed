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
    cmake --install Bin --prefix "$PREFIX"
    echo "[install] installed:"
    echo "[install]   $PREFIX/bin/fsed"
    echo "[install]   $PREFIX/share/man/man1/fsed.1"
fi
