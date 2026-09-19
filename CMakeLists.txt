cmake_minimum_required(VERSION 3.22)
project(fastsed LANGUAGES CXX)

# ── Standard ──────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_TESTS "Build GoogleTest suite" ON)

# MSVC/clang-cl flag the whole POSIX-ish C stdio/string API (fopen,
# strerror, ...) as "insecure" and wants the _s variants instead; those
# aren't portable to POSIX systems, so silence the deprecation warnings
# rather than fork the code per-platform for cosmetics.
add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

# ── Compiler flags ────────────────────────────────────────────
add_compile_options(
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-O3>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Release>>:-march=native>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-g>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-O0>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2>
)

# ── Paths ─────────────────────────────────────────────────────
set(DIR_BIN       ${CMAKE_BINARY_DIR})
set(DIR_BIN_TEST  ${CMAKE_BINARY_DIR}/Tests)

# GoogleTest: default to the copy that already comes along for free
# with `git submodule update --init --recursive` (it's CppLogicMake's
# own test dependency, vendored under external/CppLogiMake/external/
# googletest) — no separate manual clone required. $ENV{HOME}/External/
# googletest, if present, still wins, since that's where the old setup
# expected a shared checkout and some machines already have one there;
# -DDIR_GTEST=... overrides either default explicitly.
set(_fastsed_default_gtest "${CMAKE_SOURCE_DIR}/external/CppLogiMake/external/googletest")
if(EXISTS "$ENV{HOME}/External/googletest/CMakeLists.txt")
    set(_fastsed_default_gtest "$ENV{HOME}/External/googletest")
endif()
set(DIR_GTEST "${_fastsed_default_gtest}" CACHE PATH "GoogleTest source directory")

# ── Build graph: generated from fastsed.lm by CppLogicMake ─────
# fastsed_core / fastsed / fastsed_tests and their sources, include
# paths and dependency links all come from GeneratedTargets.cmake,
# which is (re)generated below from fastsed.lm — that file is now the
# source of truth for the target graph; edit it, not the generated
# output. See fastsed.lm for why Boost and GoogleTest are modelled as
# sourceless stand-in targets there.
set(LOGICMAKE_ROOT "${CMAKE_SOURCE_DIR}/external/CppLogiMake" CACHE PATH
    "CppLogicMake checkout used to transpile fastsed.lm into CMake")
if(NOT EXISTS "${LOGICMAKE_ROOT}/prolog/targets.pl")
    message(FATAL_ERROR
        "CppLogicMake not found at ${LOGICMAKE_ROOT} - run:\n"
        "  git submodule update --init --recursive")
endif()

set(LOGICMAKE_HOST_BUILD_DIR "${CMAKE_BINARY_DIR}/logicmake-host")
set(LOGICMAKE_DRIVER "${LOGICMAKE_HOST_BUILD_DIR}/logicmake")
if(NOT EXISTS "${LOGICMAKE_DRIVER}")
    message(STATUS "Building CppLogicMake driver (host tool)...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${LOGICMAKE_ROOT}" -B "${LOGICMAKE_HOST_BUILD_DIR}"
                -DCMAKE_BUILD_TYPE=Release
        RESULT_VARIABLE _logicmake_configure_rc
    )
    if(NOT _logicmake_configure_rc EQUAL 0)
        message(FATAL_ERROR "failed to configure the CppLogicMake driver")
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build "${LOGICMAKE_HOST_BUILD_DIR}" --target logicmake
        RESULT_VARIABLE _logicmake_build_rc
    )
    if(NOT _logicmake_build_rc EQUAL 0)
        message(FATAL_ERROR "failed to build the CppLogicMake driver")
    endif()
endif()

# CppLogicMake rebases every source/include path in its output to be
# relative to the *generated file's own directory* (cmake_emitter.cpp's
# rebasePath). That's a no-op exactly when the generated file sits
# directly in CMAKE_SOURCE_DIR (same directory fastsed.lm's own
# "Source/..." pathspecs are already relative to), which is also the
# one placement plain include() can use directly — include() evaluates
# relative paths against the *including* file's directory, not the
# included one's, so putting the generated file in a subdirectory (and
# add_subdirectory()-ing it instead) would also move gtest_discover_tests
# and set_target_properties below into a different directory scope than
# the targets it needs to reach. Kept out of the repo root proper and
# out of git via .gitignore, since it's build output, not source.
set(LOGICMAKE_GENERATED "${CMAKE_SOURCE_DIR}/GeneratedTargets.cmake")
message(STATUS "Generating ${LOGICMAKE_GENERATED} from fastsed.lm ...")
execute_process(
    COMMAND "${LOGICMAKE_DRIVER}"
            --schema "${LOGICMAKE_ROOT}/prolog/targets.pl"
            --input  "${CMAKE_SOURCE_DIR}/fastsed.lm"
            --output "${LOGICMAKE_GENERATED}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE _logicmake_generate_rc
)
if(NOT _logicmake_generate_rc EQUAL 0)
    message(FATAL_ERROR "CppLogicMake failed to generate ${LOGICMAKE_GENERATED} from fastsed.lm")
endif()

# ── GoogleTest ────────────────────────────────────────────────
# cmake/Generated.cmake unconditionally defines fastsed_tests linking
# against GTest::gtest_main / GTest::gmock (CppLogicMake's schema has
# no notion of an optional target — see external/CppLogiMake's README,
# "Non-goals"), so those imported targets must already exist by the
# time include() below runs, whether or not BUILD_TESTS is on. When
# it's off, fastsed_tests is still configured but built by nobody
# (EXCLUDE_FROM_ALL below) — no point spending a real GoogleTest build
# on a target that will never run, so it gets a do-nothing stand-in.
if(BUILD_TESTS)
    set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK            ON  CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)
    if(NOT EXISTS "${DIR_GTEST}/CMakeLists.txt")
        message(FATAL_ERROR "GoogleTest not found at ${DIR_GTEST}")
    endif()
    add_subdirectory("${DIR_GTEST}" "${CMAKE_BINARY_DIR}/Ext/googletest" EXCLUDE_FROM_ALL)
else()
    add_library(_fastsed_no_gtest INTERFACE)
    add_library(GTest::gtest_main ALIAS _fastsed_no_gtest)
    add_library(GTest::gmock      ALIAS _fastsed_no_gtest)
endif()

# Defines: fastsed_gtest (INTERFACE stand-in), fastsed_core, fastsed,
# fastsed_tests. No third-party dependency targets remain — fastsed
# has none since Boost was removed.
include("${LOGICMAKE_GENERATED}")

set_target_properties(fastsed PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DIR_BIN}
)

# ── Tests  →  <build>/Tests/fastsed_tests ───────────────────────
if(BUILD_TESTS)
    enable_testing()

    set_target_properties(fastsed_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${DIR_BIN_TEST}
    )

    include(GoogleTest)
    gtest_discover_tests(fastsed_tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        TEST_PREFIX       "fastsed."
    )
else()
    set_target_properties(fastsed_tests PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

# ── Install ───────────────────────────────────────────────────
# Windows resolves/executes a program by its .exe extension; installing
# it as bare "fsed" (no extension) leaves Explorer/cmd/PowerShell
# treating it as an unrecognized data file (a "pick an app to open
# this" prompt) rather than running it, even though the file itself is
# a valid PE executable. Keep the .exe suffix there; POSIX platforms
# don't use extensions to mean anything so "fsed" there is unaffected.
install(PROGRAMS $<TARGET_FILE:fastsed>
    DESTINATION bin
    RENAME fsed${CMAKE_EXECUTABLE_SUFFIX}
)
install(FILES ${CMAKE_SOURCE_DIR}/Man/fsed.1
    DESTINATION share/man/man1
)

# ── Summary ───────────────────────────────────────────────────
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests      : ${BUILD_TESTS}")
message(STATUS "  → ${CMAKE_BINARY_DIR}/fastsed")
message(STATUS "  → ${CMAKE_BINARY_DIR}/Tests/fastsed_tests")
