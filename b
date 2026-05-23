#!/usr/bin/env bash
# ============================================================
#  b — build and install fastsed
#
#  Usage:  ./b [--rebuild] [--prefix DIR] [Release|Debug]
#
#  --rebuild   wipe Bin/ (intermediate + binaries) and
#              reconfigure + rebuild from scratch.
#              ./rt passes this flag through when invoked
#              as:  ./rt --rebuild
#  --prefix    install prefix passed to cmake --install
#
#  Outputs:
#    Bin/fastsed                    main binary
#    Bin/Tests/fastsed_tests        test binary
#    <prefix>/bin/csed              installed executable
#    <prefix>/share/man/man1/csed.1 installed man page
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="Release"
REBUILD=0
COMPILER_FAMILY="${FASTSED_COMPILER:-clang}"
IPO_FLAG="${FASTSED_IPO:-OFF}"
PREFIX="${PREFIX:-/usr/local}"

case "${COMPILER_FAMILY}" in
    clang)
        CC_BIN="${CC:-$(command -v clang || true)}"
        CXX_BIN="${CXX:-$(command -v clang++ || true)}"
        ;;
    gcc)
        CC_BIN="${CC:-$(command -v gcc || true)}"
        CXX_BIN="${CXX:-$(command -v g++ || true)}"
        ;;
    *)
        echo "[b] unknown compiler family: ${COMPILER_FAMILY}"
        echo "[b] use FASTSED_COMPILER=clang or FASTSED_COMPILER=gcc"
        exit 1
        ;;
esac

if [[ -z "${CC_BIN}" || -z "${CXX_BIN}" ]]; then
    echo "[b] unable to resolve compilers for ${COMPILER_FAMILY}"
    exit 1
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rebuild)
            REBUILD=1
            shift
            ;;
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        Release|Debug|RelWithDebInfo|MinSizeRel)
            BUILD_TYPE="$1"
            shift
            ;;
        *)
            echo "[b] unknown argument: $1"
            echo "[b] usage: ./b [--rebuild] [--prefix DIR] [Release|Debug|RelWithDebInfo|MinSizeRel]"
            exit 1
            ;;
    esac
done

JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"

# ── Wipe on --rebuild ─────────────────────────────────────────
if [[ $REBUILD -eq 1 ]]; then
    echo "[b] --rebuild: removing Bin/"
    rm -rf Bin
fi

# ── Wipe if cached compiler differs ───────────────────────────
if [[ -f Bin/CMakeCache.txt ]]; then
    cached_cxx="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' Bin/CMakeCache.txt | head -n 1)"
    if [[ -n "${cached_cxx}" && "${cached_cxx}" != "${CXX_BIN}" ]]; then
        echo "[b] compiler changed: ${cached_cxx} -> ${CXX_BIN}"
        echo "[b] removing Bin/ for a clean reconfigure"
        rm -rf Bin
    fi
fi

# ── Ensure GoogleTest is present ──────────────────────────────
if [[ ! -f Ext/googletest/CMakeLists.txt ]]; then
    echo "[b] fetching GoogleTest..."
    mkdir -p Ext
    # Use submodule update only if Ext/googletest is already registered
    # in .gitmodules; otherwise do a plain clone.
    if git config --file .gitmodules --get submodule."Ext/googletest".url \
            &>/dev/null 2>&1; then
        git submodule update --init --recursive Ext/googletest
    else
        git clone --depth=1 \
            https://github.com/google/googletest.git \
            Ext/googletest
    fi
fi

# ── Configure ─────────────────────────────────────────────────
echo "[b] configuring (${BUILD_TYPE})..."
cmake -B Bin \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_C_COMPILER="${CC_BIN}" \
    -DCMAKE_CXX_COMPILER="${CXX_BIN}" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION="${IPO_FLAG}" \
    -DBUILD_TESTS=ON \
    ${BOOST_ROOT:+-DBOOST_ROOT="${BOOST_ROOT}"} \
    ${BOOST_LIBRARYDIR:+-DBOOST_LIBRARYDIR="${BOOST_LIBRARYDIR}"}

# ── Build ─────────────────────────────────────────────────────
echo "[b] building with ${JOBS} jobs..."
cmake --build Bin -j"${JOBS}"

# ── Install ───────────────────────────────────────────────────
echo "[b] installing to ${PREFIX}..."
cmake --install Bin --prefix "${PREFIX}"

echo "[b] done"
echo "[b]   compiler: ${CXX_BIN}"
echo "[b]   ipo: ${IPO_FLAG}"
echo "[b]   Bin/fastsed"
echo "[b]   Bin/Tests/fastsed_tests"
echo "[b]   ${PREFIX}/bin/csed"
echo "[b]   ${PREFIX}/share/man/man1/csed.1"
