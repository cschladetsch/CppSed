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

# ── Boost ─────────────────────────────────────────────────────
# Windows: cmake -DBOOST_ROOT=D:/boost/boost_1_91_0 ...
# Linux/macOS: system Boost found automatically.
set(Boost_USE_STATIC_LIBS    OFF)
set(Boost_USE_MULTITHREADED  ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.74 REQUIRED
    COMPONENTS filesystem iostreams program_options)

# ── Paths ─────────────────────────────────────────────────────
set(DIR_INC       ${CMAKE_SOURCE_DIR}/Include)
set(DIR_SRC       ${CMAKE_SOURCE_DIR}/Source)
set(DIR_TEST_INC  ${CMAKE_SOURCE_DIR}/Test/Include)
set(DIR_TEST_SRC  ${CMAKE_SOURCE_DIR}/Test/Source)
set(DIR_BIN       ${CMAKE_SOURCE_DIR}/Bin)
set(DIR_BIN_TEST  ${CMAKE_SOURCE_DIR}/Bin/Tests)
set(DIR_GTEST     "$ENV{HOME}/External/googletest" CACHE PATH "GoogleTest source directory")

# ── Core library  (all production code except Main.cpp) ───────
add_library(fastsed_core STATIC
    ${DIR_SRC}/Common.cpp
    ${DIR_SRC}/Regex.cpp
    ${DIR_SRC}/Replacement.cpp
    ${DIR_SRC}/Parser.cpp
    ${DIR_SRC}/Linker.cpp
    ${DIR_SRC}/EngineHelpers.cpp
    ${DIR_SRC}/Engine.cpp
    ${DIR_SRC}/LineSource.cpp
    ${DIR_SRC}/Options.cpp
)
target_include_directories(fastsed_core PUBLIC
    ${DIR_INC}
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(fastsed_core PUBLIC
    Boost::filesystem
    Boost::iostreams
    Boost::program_options
)

# ── Main executable  →  Bin/fastsed ──────────────────────────
add_executable(fastsed ${DIR_SRC}/Main.cpp)
target_link_libraries(fastsed PRIVATE fastsed_core)
set_target_properties(fastsed PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${DIR_BIN}
)

# ── Test binary  →  Bin/Tests/fastsed_tests ──────────────────
if(BUILD_TESTS)
    enable_testing()

    set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK            ON  CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)
    if(NOT EXISTS "${DIR_GTEST}/CMakeLists.txt")
        message(FATAL_ERROR "GoogleTest not found at ${DIR_GTEST}")
    endif()
    add_subdirectory("${DIR_GTEST}" "${CMAKE_BINARY_DIR}/Ext/googletest" EXCLUDE_FROM_ALL)

    add_executable(fastsed_tests
        ${DIR_TEST_SRC}/TestReplacement.cpp
        ${DIR_TEST_SRC}/TestParser.cpp
        ${DIR_TEST_SRC}/TestLinker.cpp
        ${DIR_TEST_SRC}/TestEngineHelpers.cpp
        ${DIR_TEST_SRC}/TestIntegration.cpp
    )
    target_include_directories(fastsed_tests PRIVATE
        ${DIR_TEST_INC}   # TestHelper.hpp
        ${DIR_INC}        # fastsed/*.hpp
    )
    target_link_libraries(fastsed_tests PRIVATE
        fastsed_core
        GTest::gtest_main
        GTest::gmock
    )
    set_target_properties(fastsed_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${DIR_BIN_TEST}
    )

    include(GoogleTest)
    gtest_discover_tests(fastsed_tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        TEST_PREFIX       "fastsed."
    )
endif()

# ── Install ───────────────────────────────────────────────────
install(PROGRAMS $<TARGET_FILE:fastsed>
    DESTINATION bin
    RENAME fsed
)
install(FILES ${CMAKE_SOURCE_DIR}/Man/fsed.1
    DESTINATION share/man/man1
)

# ── Summary ───────────────────────────────────────────────────
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Boost      : ${Boost_VERSION}")
message(STATUS "  Tests      : ${BUILD_TESTS}")
message(STATUS "  → Bin/fastsed")
message(STATUS "  → Bin/Tests/fastsed_tests")
