cmake_minimum_required(VERSION 3.22.1)
project(ChessZeroAndroid LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CHESSZERO_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
option(CHESSZERO_ENABLE_SYZYGY "Enable real Syzygy probing through pinned Fathom" ON)
option(CHESSZERO_FATHOM_OFFLINE "Fail instead of fetching Fathom when local source is missing" OFF)
option(CHESSZERO_REQUIRE_VENDORED_FATHOM "Require the vendored Fathom source tree; never FetchContent" ON)
set(CHESSZERO_FATHOM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party/Fathom" CACHE PATH "Use a local Fathom source tree for Android")
set(CHESSZERO_FATHOM_SRC "")
if(CHESSZERO_ENABLE_SYZYGY)
    if(NOT EXISTS "${CHESSZERO_FATHOM_SOURCE_DIR}/src/tbprobe.c")
        if(CHESSZERO_FATHOM_OFFLINE OR CHESSZERO_REQUIRE_VENDORED_FATHOM)
            message(FATAL_ERROR "Fathom source missing at ${CHESSZERO_FATHOM_SOURCE_DIR}; offline/required-vendored mode forbids FetchContent")
        endif()
        set(CHESSZERO_FATHOM_SOURCE_DIR "" CACHE PATH "Local Fathom source tree" FORCE)
    endif()
    set(CHESSZERO_FATHOM_REPOSITORY "https://github.com/jdart1/Fathom.git" CACHE STRING "Fathom Git repository")
    set(CHESSZERO_FATHOM_TAG "c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e" CACHE STRING "Pinned Fathom commit")
    if(CHESSZERO_FATHOM_SOURCE_DIR)
        set(CHESSZERO_FATHOM_INCLUDE_DIR "${CHESSZERO_FATHOM_SOURCE_DIR}/src")
        set(CHESSZERO_FATHOM_SRC
            "${CHESSZERO_FATHOM_SOURCE_DIR}/src/tbprobe.c"
        )
    else()
        if(CHESSZERO_FATHOM_OFFLINE OR CHESSZERO_REQUIRE_VENDORED_FATHOM)
            message(FATAL_ERROR "Fathom source is missing and offline/required-vendored mode is enabled. Populate third_party/Fathom or set CHESSZERO_FATHOM_SOURCE_DIR.")
        endif()
        include(FetchContent)
        FetchContent_Declare(
            chesszero_fathom
            GIT_REPOSITORY "${CHESSZERO_FATHOM_REPOSITORY}"
            GIT_TAG "${CHESSZERO_FATHOM_TAG}"
            GIT_SHALLOW FALSE
        )
        FetchContent_Populate(chesszero_fathom)
        set(CHESSZERO_FATHOM_INCLUDE_DIR "${chesszero_fathom_SOURCE_DIR}/src")
        set(CHESSZERO_FATHOM_SRC
            "${chesszero_fathom_SOURCE_DIR}/src/tbprobe.c"
        )
    endif()
    add_compile_definitions(CHESSZERO_FATHOM=1)
    include_directories(SYSTEM "${CHESSZERO_FATHOM_INCLUDE_DIR}")
    # Fathom sources must be compiled as C (not C++).
    set_source_files_properties(${CHESSZERO_FATHOM_SRC} PROPERTIES LANGUAGE C)
endif()
# Embed the release HalfKP network into every Android native engine target.
# OEX processes launched by Chess GUIs cannot reliably resolve app-private
# assets, so the engine must be self-contained instead of depending on
# `nets/*.nnue` beside the executable. The generated ELF object is read-only.
set(CHESSZERO_NNUE_SOURCE "${CHESSZERO_ROOT}/nets/ChessZero-v1.0.9-current.nnue")
set(CHESSZERO_NNUE_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/nnue")
set(CHESSZERO_NNUE_BIN "${CHESSZERO_NNUE_GEN_DIR}/embedded_nnue.bin")
set(CHESSZERO_NNUE_OBJ "${CHESSZERO_NNUE_GEN_DIR}/embedded_nnue.o")
get_filename_component(CHESSZERO_CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(CHESSZERO_LLVM_OBJCOPY
    NAMES llvm-objcopy
    HINTS "${CHESSZERO_CLANG_BIN_DIR}"
    REQUIRED
)
add_custom_command(
    OUTPUT "${CHESSZERO_NNUE_OBJ}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CHESSZERO_NNUE_GEN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CHESSZERO_NNUE_SOURCE}" "${CHESSZERO_NNUE_BIN}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${CHESSZERO_NNUE_GEN_DIR}"
            "${CHESSZERO_LLVM_OBJCOPY}"
            -I binary -O elf64-littleaarch64 -B aarch64
            --rename-section .data=.rodata,alloc,load,readonly,data,contents
            embedded_nnue.bin embedded_nnue.o
    DEPENDS "${CHESSZERO_NNUE_SOURCE}"
    VERBATIM
)
set_source_files_properties("${CHESSZERO_NNUE_OBJ}" PROPERTIES
    GENERATED TRUE
    EXTERNAL_OBJECT TRUE
)
set(CHESSZERO_NNUE_COMPILE_DEFS CHESSZERO_EMBED_NNUE=1)
set(CHESSZERO_ANDROID_ENGINE_SRC
    ${CHESSZERO_ROOT}/src/api/EngineAPI.cpp
    ${CHESSZERO_ROOT}/src/chess/Position.cpp
    ${CHESSZERO_ROOT}/src/chess/Move.cpp
    ${CHESSZERO_ROOT}/src/chess/Zobrist.cpp
    ${CHESSZERO_ROOT}/src/bitboard/Bitboard.cpp
    ${CHESSZERO_ROOT}/src/engine/Evaluation.cpp
    ${CHESSZERO_ROOT}/src/engine/Search.cpp
    ${CHESSZERO_ROOT}/src/engine/TranspositionTable.cpp
    ${CHESSZERO_ROOT}/src/book/OpeningBook.cpp
    ${CHESSZERO_ROOT}/src/book/PolyglotZobrist.cpp
    ${CHESSZERO_ROOT}/src/nnue/NNUE.cpp
    ${CHESSZERO_ROOT}/src/tablebase/Syzygy.cpp
    ${CHESSZERO_FATHOM_SRC}
)
set(CHESSZERO_OEX_ENGINE_SRC
    ${CHESSZERO_ROOT}/src/main.cpp
    ${CHESSZERO_ROOT}/src/chess/Position.cpp
    ${CHESSZERO_ROOT}/src/chess/Move.cpp
    ${CHESSZERO_ROOT}/src/chess/Zobrist.cpp
    ${CHESSZERO_ROOT}/src/bitboard/Bitboard.cpp
    ${CHESSZERO_ROOT}/src/engine/Evaluation.cpp
    ${CHESSZERO_ROOT}/src/engine/Search.cpp
    ${CHESSZERO_ROOT}/src/engine/TranspositionTable.cpp
    ${CHESSZERO_ROOT}/src/uci/UCI.cpp
    ${CHESSZERO_ROOT}/src/book/OpeningBook.cpp
    ${CHESSZERO_ROOT}/src/book/PolyglotZobrist.cpp
    ${CHESSZERO_ROOT}/src/nnue/NNUE.cpp
    ${CHESSZERO_ROOT}/src/tablebase/Syzygy.cpp
    ${CHESSZERO_FATHOM_SRC}
)
add_executable(chesszero_oex ${CHESSZERO_OEX_ENGINE_SRC})
target_include_directories(chesszero_oex PRIVATE ${CHESSZERO_ROOT}/src)
target_sources(chesszero_oex PRIVATE "${CHESSZERO_NNUE_OBJ}")
target_compile_definitions(chesszero_oex PRIVATE CHESSZERO_RELEASE=1 CHESSZERO_ANDROID=1 ${CHESSZERO_NNUE_COMPILE_DEFS})
target_compile_options(chesszero_oex PRIVATE
    -O3
    -DNDEBUG
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
    -fPIE
)
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    target_compile_options(chesszero_oex PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_oex PRIVATE USE_NEON=1)
endif()
target_link_options(chesszero_oex PRIVATE
    -fPIE
    -pie
    -Wl,--gc-sections
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
)
set_target_properties(chesszero_oex PROPERTIES
    OUTPUT_NAME "libchesszero"
    PREFIX ""
    SUFFIX ".so"
)
# Use the asset directory passed from Gradle if provided, otherwise fall back to a default.
if(NOT DEFINED CHESSZERO_OEX_ASSET_DIR OR CHESSZERO_OEX_ASSET_DIR STREQUAL "")
    set(CHESSZERO_OEX_ASSET_DIR "${CMAKE_CURRENT_LIST_DIR}/app/build/generated/oex")
endif()
add_custom_command(TARGET chesszero_oex POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CHESSZERO_OEX_ASSET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:chesszero_oex>" "${CHESSZERO_OEX_ASSET_DIR}/libchesszero.so"
    VERBATIM
)
add_library(chesszero_android SHARED
    ${CHESSZERO_ANDROID_ENGINE_SRC}
    ${CMAKE_CURRENT_LIST_DIR}/app/src/main/cpp/jni_bridge.cpp
)
target_include_directories(chesszero_android PRIVATE
    ${CHESSZERO_ROOT}/src
    ${CHESSZERO_ROOT}/src/api
)
target_sources(chesszero_android PRIVATE "${CHESSZERO_NNUE_OBJ}")
target_compile_definitions(chesszero_android PRIVATE CHESSZERO_RELEASE=1 CHESSZERO_ANDROID=1 ${CHESSZERO_NNUE_COMPILE_DEFS})
target_compile_options(chesszero_android PRIVATE
    -O3
    -DNDEBUG
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    target_compile_options(chesszero_android PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_android PRIVATE USE_NEON=1)
endif()
# Android NDK r28+ already produces 16 KB aligned ELF. Keep explicit flags so
# the project remains correct when built with an older r27-or-earlier NDK.
target_link_options(chesszero_android PRIVATE
    -Wl,--gc-sections
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
)
# The Gradle project requests c++_static explicitly so the APK ships one engine JNI .so.
# NDK toolchains keep their standard-library selection outside target properties.
set_target_properties(chesszero_android PROPERTIES
    OUTPUT_NAME "chesszero_android"
    POSITION_INDEPENDENT_CODE ON
)
if(NOT ANDROID)
    find_package(JNI REQUIRED)
    target_include_directories(chesszero_android PRIVATE ${JNI_INCLUDE_DIRS})
endif()
