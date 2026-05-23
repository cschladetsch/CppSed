#!/usr/bin/env bash
# ============================================================
#  b — build and install fastsed
#
#  Usage:  ./b [--rebuild] [--prefix DIR] [Release|Debug]
#
#  --rebuild   wipe the selected build dir (intermediate + binaries) and
#              reconfigure + rebuild from scratch.
#              ./rt passes this flag through when invoked
#              as:  ./rt --rebuild
#  --prefix    install prefix passed to cmake --install
#  --no-install skip the install step; only configure + build
#
#  Outputs:
#    <build>/fastsed                main binary
#    <build>/Tests/fastsed_tests    test binary
#    <prefix>/bin/fsed              installed executable
#    <prefix>/share/man/man1/fsed.1 installed man page
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="Release"
REBUILD=0
INSTALL=1
COMPILER_FAMILY="${FASTSED_COMPILER:-clang}"
IPO_FLAG="${FASTSED_IPO:-OFF}"
if [[ -z "${PREFIX:-}" ]]; then
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        PREFIX="/usr/local"
    else
        PREFIX="${HOME}/.local"
    fi
fi
GTEST_DIR="${GTEST_DIR:-$HOME/External/googletest}"

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
        --no-install)
            INSTALL=0
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
            echo "[b] usage: ./b [--rebuild] [--no-install] [--prefix DIR] [Release|Debug|RelWithDebInfo|MinSizeRel]"
            exit 1
            ;;
    esac
done

JOBS="$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"
TEST_BIN="${BUILD_DIR}/Tests/fastsed_tests"
MAIN_BIN="${BUILD_DIR}/fastsed"

# ── Wipe on --rebuild ─────────────────────────────────────────
if [[ $REBUILD -eq 1 ]]; then
    echo "[b] --rebuild: removing ${BUILD_DIR}/"
    rm -rf "${BUILD_DIR}"
fi

# ── Wipe if cached compiler differs ───────────────────────────
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cached_cxx="$(sed -n 's/^CMAKE_CXX_COMPILER:FILEPATH=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n 1)"
    if [[ -n "${cached_cxx}" && "${cached_cxx}" != "${CXX_BIN}" ]]; then
        echo "[b] compiler changed: ${cached_cxx} -> ${CXX_BIN}"
        echo "[b] removing ${BUILD_DIR}/ for a clean reconfigure"
        rm -rf "${BUILD_DIR}"
    fi
fi

if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    cached_gtest="$(sed -n 's/^DIR_GTEST:PATH=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n 1)"
    if [[ -n "${cached_gtest}" && "${cached_gtest}" != "${GTEST_DIR}" ]]; then
        echo "[b] gtest path changed: ${cached_gtest} -> ${GTEST_DIR}"
        echo "[b] removing ${BUILD_DIR}/ for a clean reconfigure"
        rm -rf "${BUILD_DIR}"
    fi
fi

# ── Ensure GoogleTest is present ──────────────────────────────
if [[ ! -f "${GTEST_DIR}/CMakeLists.txt" ]]; then
    echo "[b] fetching GoogleTest..."
    mkdir -p "$(dirname "${GTEST_DIR}")"
    git clone --depth=1 \
        https://github.com/google/googletest.git \
        "${GTEST_DIR}"
fi

# ── Configure ─────────────────────────────────────────────────
echo "[b] configuring (${BUILD_TYPE}) in ${BUILD_DIR}/..."
cmake -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_C_COMPILER="${CC_BIN}" \
    -DCMAKE_CXX_COMPILER="${CXX_BIN}" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION="${IPO_FLAG}" \
    -DDIR_GTEST="${GTEST_DIR}" \
    -DBUILD_TESTS=ON \
    ${BOOST_ROOT:+-DBOOST_ROOT="${BOOST_ROOT}"} \
    ${BOOST_LIBRARYDIR:+-DBOOST_LIBRARYDIR="${BOOST_LIBRARYDIR}"}

# ── Build ─────────────────────────────────────────────────────
echo "[b] building with ${JOBS} jobs..."
cmake --build "${BUILD_DIR}" -j"${JOBS}"

if [[ "$INSTALL" -eq 1 ]]; then
    # ── Install ───────────────────────────────────────────────────
    echo "[b] installing to ${PREFIX}..."
    cmake --install "${BUILD_DIR}" --prefix "${PREFIX}"
fi

echo "[b] done"
echo "[b]   compiler: ${CXX_BIN}"
echo "[b]   ipo: ${IPO_FLAG}"
echo "[b]   gtest: ${GTEST_DIR}"
echo "[b]   ${MAIN_BIN}"
echo "[b]   ${TEST_BIN}"
if [[ "$INSTALL" -eq 1 ]]; then
    echo "[b]   ${PREFIX}/bin/fsed"
    echo "[b]   ${PREFIX}/share/man/man1/fsed.1"
fi
