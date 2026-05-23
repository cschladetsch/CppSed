#!/usr/bin/env bash
# ============================================================
#  b — build fastsed
#
#  Usage:  ./b [--rebuild] [Release|Debug]
#
#  --rebuild   wipe Bin/ (intermediate + binaries) and
#              reconfigure + rebuild from scratch.
#              ./rt passes this flag through when invoked
#              as:  ./rt --rebuild
#
#  Outputs:
#    Bin/fastsed              main binary
#    Bin/Tests/fastsed_tests  test binary
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="Release"
REBUILD=0

for arg in "$@"; do
    case "$arg" in
        --rebuild) REBUILD=1 ;;
        Release|Debug|RelWithDebInfo|MinSizeRel) BUILD_TYPE="$arg" ;;
        *) echo "[b] unknown argument: $arg"; exit 1 ;;
    esac
done

JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

# ── Wipe on --rebuild ─────────────────────────────────────────
if [[ $REBUILD -eq 1 ]]; then
    echo "[b] --rebuild: removing Bin/"
    rm -rf Bin
fi

# ── Ensure GoogleTest submodule is present ────────────────────
if [[ ! -f Ext/googletest/CMakeLists.txt ]]; then
    echo "[b] initialising GoogleTest submodule..."
    if git rev-parse --git-dir &>/dev/null 2>&1; then
        git submodule update --init --recursive Ext/googletest
    else
        mkdir -p Ext
        git clone --depth=1 \
            https://github.com/google/googletest.git \
            Ext/googletest
    fi
fi

# ── Configure ─────────────────────────────────────────────────
echo "[b] configuring (${BUILD_TYPE})..."
cmake -B Bin \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DBUILD_TESTS=ON \
    ${BOOST_ROOT:+-DBOOST_ROOT="${BOOST_ROOT}"} \
    ${BOOST_LIBRARYDIR:+-DBOOST_LIBRARYDIR="${BOOST_LIBRARYDIR}"}

# ── Build ─────────────────────────────────────────────────────
echo "[b] building with ${JOBS} jobs..."
cmake --build Bin -j"${JOBS}"

echo "[b] done"
echo "[b]   Bin/fastsed"
echo "[b]   Bin/Tests/fastsed_tests"
