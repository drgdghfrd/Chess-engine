cmake_minimum_required(VERSION 3.15)
project(ChessZero VERSION 1.0.9 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optional memory/undefined-behaviour sanitizer build for regression testing.
# Keep OFF by default so normal Android/Termux and release builds are unchanged.
option(CHESSZERO_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if(CHESSZERO_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "CHESSZERO_ENABLE_SANITIZERS currently requires GCC/Clang")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "Sanitizers: AddressSanitizer + UndefinedBehaviorSanitizer")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(CHESSZERO_ANDROID_OEX "Build as Android OEX libchesszero.so" OFF)
option(CHESSZERO_ENABLE_LTO "Enable link-time optimization for Release builds" ON)
option(CHESSZERO_ENABLE_SYZYGY "Enable real Syzygy probing through pinned Fathom" OFF)
option(CHESSZERO_REQUIRE_VENDORED_FATHOM "Require the vendored Fathom source tree; never FetchContent" OFF)
option(CHESSZERO_ANDROID_16K "Use 16 KB ELF page alignment for Android native binaries" ON)

find_package(Threads REQUIRED)

if(CHESSZERO_ENABLE_SYZYGY)
    include(cmake/FindFathom.cmake)
    add_compile_definitions(CHESSZERO_FATHOM=1)
    include_directories(SYSTEM "${CHESSZERO_FATHOM_INCLUDE_DIR}")
    message(STATUS "Syzygy/Fathom enabled: ${CHESSZERO_FATHOM_DIR}")
endif()

# ---- Sources (chỉ dùng phần engine "uc" sạch) ----
set(CHESSZERO_SRC
    src/main.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/uci/UCI.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
)

add_executable(chesszero ${CHESSZERO_SRC})

# Ship the legacy demo and the v0.75 HalfKP sample network next to the build tree.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/nets)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/nets/ChessZero-v0.32-demo.nnue
               ${CMAKE_CURRENT_BINARY_DIR}/nets/ChessZero-v0.32-demo.nnue COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/nets/ChessZero-v0.75-halfkp.nnue
               ${CMAKE_CURRENT_BINARY_DIR}/nets/ChessZero-v0.75-halfkp.nnue COPYONLY)

target_include_directories(chesszero PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(chesszero PRIVATE /O2 /W4 /DNDEBUG)
else()
    target_compile_options(chesszero PRIVATE
        -O3
        -DNDEBUG
        -Wall
        -Wextra
        -ffunction-sections
        -fdata-sections
    )
endif()

target_compile_definitions(chesszero PRIVATE CHESSZERO_RELEASE=1)

# Clang/NDK LTO: useful for a compute-heavy native engine and typically
# improves cross-TU inlining and code generation without changing the search.
if(CHESSZERO_ENABLE_LTO AND CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    set_property(TARGET chesszero PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    # Some Clang distributions lack LLVMgold.so but ship LLD. Prefer LLD so
    # ThinLTO remains buildable on those hosts; Android NDK Clang already uses
    # the NDK linker path.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(CHESSZERO_LLD_EXECUTABLE NAMES ld.lld lld)
        if(CHESSZERO_LLD_EXECUTABLE)
            target_link_options(chesszero PRIVATE -fuse-ld=lld)
        endif()
    endif()
endif()

# ARM NEON (điện thoại Android / aarch64)
if(ANDROID AND CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    message(STATUS "Enabling ARM64 NEON optimizations for Android")
    target_compile_options(chesszero PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero PRIVATE USE_NEON=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    message(STATUS "Enabling ARM NEON optimizations")
    target_compile_options(chesszero PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero PRIVATE USE_NEON=1)
endif()

target_link_libraries(chesszero PRIVATE Threads::Threads)

# ---- Android OEX (tùy chọn) ----
if(CHESSZERO_ANDROID_OEX)
    set_target_properties(chesszero PROPERTIES
        OUTPUT_NAME "libchesszero"
        PREFIX ""
        SUFFIX ".so"
        POSITION_INDEPENDENT_CODE ON
    )
    target_compile_options(chesszero PRIVATE -fPIE)
    target_link_options(chesszero PRIVATE -fPIE -pie -static-libstdc++ -Wl,--gc-sections)
    # Android NDK r27 and lower need explicit 16 KB ELF alignment. NDK r28+
    # already defaults to 16 KB, so the option remains harmless there.
    if(ANDROID AND CHESSZERO_ANDROID_16K)
        target_link_options(chesszero PRIVATE
            "-Wl,-z,max-page-size=16384"
            "-Wl,-z,common-page-size=16384"
        )
    endif()
endif()

message(STATUS "ChessZero ${PROJECT_VERSION} — build type: ${CMAKE_BUILD_TYPE}")

# ---- Release packaging / installation ----
include(GNUInstallDirs)
install(TARGETS chesszero
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    BUNDLE DESTINATION .
)
install(DIRECTORY nets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/chesszero/nets)
install(FILES README.md CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DATADIR}/chesszero)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/chesszero/docs)

# ---- v0.75 self-play dataset generator ----
add_executable(chesszero_selfplay
    tools/selfplay_dataset.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
    src/book/OpeningBook.cpp
)
target_include_directories(chesszero_selfplay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_selfplay PRIVATE -O3 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_selfplay PRIVATE Threads::Threads)


add_executable(chesszero_v112_runtime_resource_tests
    tests/v112_runtime_resource_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v112_runtime_resource_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v112_runtime_resource_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_v112_runtime_resource_tests PRIVATE Threads::Threads)
add_test(NAME v112_runtime_resource COMMAND chesszero_v112_runtime_resource_tests)

add_executable(chesszero_v113_low_hash_profile_tests
    tests/v113_low_hash_profile_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v113_low_hash_profile_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v113_low_hash_profile_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_v113_low_hash_profile_tests PRIVATE Threads::Threads)
add_test(NAME v113_low_hash_profiles COMMAND chesszero_v113_low_hash_profile_tests)

add_executable(chesszero_v108_android_game_api_tests
    tests/v108_android_game_api_tests.cpp
    src/api/EngineAPI.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v108_android_game_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v108_android_game_api_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_v108_android_game_api_tests PRIVATE Threads::Threads)
add_test(NAME v108_android_game_api COMMAND chesszero_v108_android_game_api_tests)

add_executable(chesszero_v115_engine_api_runtime_reconfigure_tests
    tests/v115_engine_api_runtime_reconfigure_tests.cpp
    src/api/EngineAPI.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v115_engine_api_runtime_reconfigure_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v115_engine_api_runtime_reconfigure_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_v115_engine_api_runtime_reconfigure_tests PRIVATE Threads::Threads)
add_test(NAME v115_engine_api_runtime_reconfigure COMMAND chesszero_v115_engine_api_runtime_reconfigure_tests)

add_executable(chesszero_v116_runtime_reconfigure_matrix_tests
    tests/v116_runtime_reconfigure_matrix_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v116_runtime_reconfigure_matrix_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v116_runtime_reconfigure_matrix_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_v116_runtime_reconfigure_matrix_tests PRIVATE Threads::Threads)
add_test(NAME v116_runtime_reconfigure_matrix COMMAND chesszero_v116_runtime_reconfigure_matrix_tests)

option(CHESSZERO_BUILD_TESTS "Build ChessZero regression tests" ON)

if(CHESSZERO_BUILD_TESTS)
    enable_testing()
    
    # ---------------------------------------------------------
    # TEST 1: v032_nnue_tests
    # ---------------------------------------------------------
    add_executable(chesszero_v032_nnue_tests
        tests/v032_nnue_tests.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/book/OpeningBook.cpp
    )
    target_include_directories(chesszero_v032_nnue_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(chesszero_v032_nnue_tests PRIVATE
        -O2 -Wall -Wextra -UNDEBUG
    )
    add_test(NAME v032_nnue_inference COMMAND chesszero_v032_nnue_tests)

    # ---------------------------------------------------------
    # TEST 2: v051_continuation_tests
    # ---------------------------------------------------------
    add_executable(chesszero_v051_continuation_tests
        tests/v051_continuation_tests.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/book/OpeningBook.cpp
    )
    target_include_directories(chesszero_v051_continuation_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(chesszero_v051_continuation_tests PRIVATE
        -O2 -Wall -Wextra -UNDEBUG
    )
    add_test(NAME v051_continuation COMMAND chesszero_v051_continuation_tests)

    # ---------------------------------------------------------
    # TEST 3: v041_simd_tests (SDOT vs NEON vmlal bit-exact)
    # ---------------------------------------------------------
    add_executable(chesszero_v041_simd_tests
        tests/v041_simd_tests.cpp
        src/nnue/NNUE.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
    )
    target_include_directories(chesszero_v041_simd_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(chesszero_v041_simd_tests PRIVATE
        -O2 -Wall -Wextra -UNDEBUG
    )
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(chesszero_v041_simd_tests PRIVATE -march=armv8-a+simd)
        target_compile_definitions(chesszero_v041_simd_tests PRIVATE USE_NEON=1)
    endif()
    add_test(NAME v041_simd COMMAND chesszero_v041_simd_tests)

    # ---------------------------------------------------------
    # TEST 4: v052_heuristic_tests
    # ---------------------------------------------------------
    add_executable(chesszero_v052_heuristic_tests
        tests/v052_heuristic_tests.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/book/OpeningBook.cpp
    )
    target_include_directories(chesszero_v052_heuristic_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(chesszero_v052_heuristic_tests PRIVATE
        -O2 -Wall -Wextra -UNDEBUG
    )
    add_test(NAME v052_heuristic_search COMMAND chesszero_v052_heuristic_tests)



    # TEST 5: v053_search_tests
    add_executable(chesszero_v053_search_tests
        tests/v053_search_tests.cpp
        src/chess/Position.cpp
        src/chess/Zobrist.cpp
        src/engine/Search.cpp
        src/engine/Evaluation.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/chess/Move.cpp
        src/bitboard/Bitboard.cpp
    )
    target_include_directories(chesszero_v053_search_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(chesszero_v053_search_tests PRIVATE -O2 -Wall -Wextra)
    add_test(NAME v053_alpha_beta_pvs COMMAND chesszero_v053_search_tests)

    # TEST: v1.0.2 position-state safety regression
    add_executable(chesszero_v102_position_regression
        tests/v102_position_regression.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
    )
    target_include_directories(chesszero_v102_position_regression PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(chesszero_v102_position_regression PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    add_test(NAME v102_position_king_capture COMMAND chesszero_v102_position_regression)

    # v1.0.4 C ABI / mobile runtime regression.
    add_executable(chesszero_v104_engine_api_tests
        tests/v104_engine_api_tests.cpp
        src/api/EngineAPI.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/book/OpeningBook.cpp
        src/book/PolyglotZobrist.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
    )
    target_include_directories(chesszero_v104_engine_api_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    )
    target_compile_options(chesszero_v104_engine_api_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    if(UNIX)
        target_link_libraries(chesszero_v104_engine_api_tests PRIVATE Threads::Threads)
    endif()
    add_test(NAME v104_engine_api COMMAND chesszero_v104_engine_api_tests)

    # v1.0.5: persistent private worker contexts / root queue safety
    add_executable(chesszero_v105_threads_performance_tests
        tests/v105_threads_performance_tests.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/book/OpeningBook.cpp
    )
    target_include_directories(chesszero_v105_threads_performance_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v105_threads_performance_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    target_link_libraries(chesszero_v105_threads_performance_tests PRIVATE Threads::Threads)
    add_test(NAME v105_threads_performance COMMAND chesszero_v105_threads_performance_tests)

    # v0.60 closes Phase-1 search stack; keeps quiescence and prior search regressions.
add_executable(chesszero_v056_tt_tests
    tests/v056_tt_tests.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/engine/TranspositionTable.cpp
)
target_include_directories(chesszero_v056_tt_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_options(chesszero_v056_tt_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v056_transposition_table COMMAND chesszero_v056_tt_tests)



    add_executable(chesszero_v060_phase1_tests
        tests/v060_phase1_tests.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/book/OpeningBook.cpp
    )
    target_include_directories(chesszero_v060_phase1_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v060_phase1_tests PRIVATE -O2 -Wall -Wextra)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(chesszero_v060_phase1_tests PRIVATE -march=armv8-a+simd)
        target_compile_definitions(chesszero_v060_phase1_tests PRIVATE USE_NEON=1)
    endif()
    if(UNIX)
        target_link_libraries(chesszero_v060_phase1_tests PRIVATE pthread)
    endif()
    add_test(NAME v060_phase1 COMMAND chesszero_v060_phase1_tests)


    add_executable(chesszero_v069_nnue_bitexact_tests
        tests/v069_nnue_bitexact_tests.cpp
        src/nnue/NNUE.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
    )
    target_include_directories(chesszero_v069_nnue_bitexact_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v069_nnue_bitexact_tests PRIVATE -O2 -Wall)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(chesszero_v069_nnue_bitexact_tests PRIVATE -march=armv8-a+simd)
        target_compile_definitions(chesszero_v069_nnue_bitexact_tests PRIVATE USE_NEON=1)
    endif()
    add_test(NAME v069_nnue_bitexact COMMAND chesszero_v069_nnue_bitexact_tests)


    # ---------------------------------------------------------
    # TEST: v072_halfkp_tests — HalfKP king-move bit-exactness
    # ---------------------------------------------------------
    add_executable(chesszero_v072_halfkp_tests
        tests/v072_halfkp_tests.cpp
        src/nnue/NNUE.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
    )
    target_include_directories(chesszero_v072_halfkp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v072_halfkp_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(chesszero_v072_halfkp_tests PRIVATE -march=armv8-a+simd)
        target_compile_definitions(chesszero_v072_halfkp_tests PRIVATE USE_NEON=1)
    endif()
    add_test(NAME v072_halfkp COMMAND chesszero_v072_halfkp_tests)

    # ---------------------------------------------------------
    # TEST 10: v074 training-pipeline network
    # ---------------------------------------------------------
    add_executable(chesszero_v074_pipeline_tests
        tests/v074_pipeline_tests.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/book/OpeningBook.cpp
    )
    target_include_directories(chesszero_v074_pipeline_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v074_pipeline_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(chesszero_v074_pipeline_tests PRIVATE -march=armv8-a+simd)
        target_compile_definitions(chesszero_v074_pipeline_tests PRIVATE USE_NEON=1)
    endif()
    add_test(NAME v074_training_pipeline COMMAND chesszero_v074_pipeline_tests)
set_tests_properties(v074_training_pipeline PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(CHESSZERO_BUILD_TESTS)
    add_executable(chesszero_v075_network_tests
        tests/v075_network_tests.cpp
        src/nnue/NNUE.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
    )
    target_include_directories(chesszero_v075_network_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v075_network_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(chesszero_v075_network_tests PRIVATE -march=armv8-a+simd)
        target_compile_definitions(chesszero_v075_network_tests PRIVATE USE_NEON=1)
    endif()
    add_test(NAME v075_network COMMAND chesszero_v075_network_tests)
endif()

# ---------------------------------------------------------
# TEST: v0.76 match / Elo statistics
# ---------------------------------------------------------
if(CHESSZERO_BUILD_TESTS)
    add_executable(chesszero_v076_match_tests tests/v076_match_tests.cpp)
    target_compile_options(chesszero_v076_match_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    add_test(NAME v076_match_statistics COMMAND chesszero_v076_match_tests)

add_executable(chesszero_v077_search_tests
        tests/v077_search_tests.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
        src/engine/Evaluation.cpp
        src/engine/Search.cpp
        src/engine/TranspositionTable.cpp
        src/nnue/NNUE.cpp
        src/tablebase/Syzygy.cpp
        src/book/OpeningBook.cpp
    )
    target_include_directories(chesszero_v077_search_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v077_search_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(chesszero_v077_search_tests PRIVATE -march=armv8-a+simd)
        target_compile_definitions(chesszero_v077_search_tests PRIVATE USE_NEON=1)
    endif()
    if(UNIX)
        target_link_libraries(chesszero_v077_search_tests PRIVATE pthread)
    endif()
    add_test(NAME v077_aspiration_pvs COMMAND chesszero_v077_search_tests)
set_tests_properties(v077_aspiration_pvs PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

add_executable(chesszero_v078_lmr_tests
    tests/v078_lmr_tests.cpp
    src/bitboard/Bitboard.cpp
    src/chess/Move.cpp
    src/chess/Position.cpp
    src/chess/Zobrist.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/book/OpeningBook.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v078_lmr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v078_lmr_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    target_compile_options(chesszero_v078_lmr_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v078_lmr_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v078_lmr_tests PRIVATE pthread)
endif()
add_test(NAME v078_lmr_tuning COMMAND chesszero_v078_lmr_tests)
set_tests_properties(v078_lmr_tuning PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(chesszero_v079_null_razor_tests
    tests/v079_null_razor_tests.cpp
    src/bitboard/Bitboard.cpp
    src/chess/Move.cpp
    src/chess/Position.cpp
    src/chess/Zobrist.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/book/OpeningBook.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v079_null_razor_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v079_null_razor_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    target_compile_options(chesszero_v079_null_razor_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v079_null_razor_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v079_null_razor_tests PRIVATE pthread)
endif()
add_test(NAME v079_null_razor_tuning COMMAND chesszero_v079_null_razor_tests)
set_tests_properties(v079_null_razor_tuning PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(chesszero_v080_history_counter_tests
    tests/v080_history_counter_tests.cpp
    src/bitboard/Bitboard.cpp
    src/chess/Move.cpp
    src/chess/Position.cpp
    src/chess/Zobrist.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/book/OpeningBook.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v080_history_counter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v080_history_counter_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    target_compile_options(chesszero_v080_history_counter_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v080_history_counter_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v080_history_counter_tests PRIVATE pthread)
endif()
add_test(NAME v080_history_counter_tuning COMMAND chesszero_v080_history_counter_tests)
set_tests_properties(v080_history_counter_tuning PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(chesszero_v081_search_heuristic_tests
    tests/v081_search_heuristic_tests.cpp
    src/bitboard/Bitboard.cpp src/chess/Move.cpp src/chess/Position.cpp src/chess/Zobrist.cpp
    src/engine/Evaluation.cpp src/engine/Search.cpp src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp src/book/OpeningBook.cpp src/tablebase/Syzygy.cpp)
target_include_directories(chesszero_v081_search_heuristic_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v081_search_heuristic_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    target_compile_options(chesszero_v081_search_heuristic_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v081_search_heuristic_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v081_search_heuristic_tests PRIVATE pthread)
endif()
add_test(NAME v081_search_heuristic_tuning COMMAND chesszero_v081_search_heuristic_tests)
set_tests_properties(v081_search_heuristic_tuning PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(chesszero_v082_measurement_tests
    tests/v082_measurement_tests.cpp
    src/bitboard/Bitboard.cpp src/chess/Move.cpp src/chess/Position.cpp src/chess/Zobrist.cpp
    src/engine/Evaluation.cpp src/engine/Search.cpp src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp src/book/OpeningBook.cpp src/tablebase/Syzygy.cpp)
target_include_directories(chesszero_v082_measurement_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v082_measurement_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    target_compile_options(chesszero_v082_measurement_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v082_measurement_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v082_measurement_tests PRIVATE pthread)
endif()
add_test(NAME v082_measurement_harness COMMAND chesszero_v082_measurement_tests)
set_tests_properties(v082_measurement_harness PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(chesszero_v084_analysis_tests
    tests/v084_analysis_tests.cpp)
target_compile_options(chesszero_v084_analysis_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v084_heuristic_analysis COMMAND chesszero_v084_analysis_tests)
add_executable(chesszero_v085_stabilization_tests
    tests/v085_stabilization_tests.cpp
    src/bitboard/Bitboard.cpp src/chess/Move.cpp src/chess/Position.cpp src/chess/Zobrist.cpp
    src/engine/Evaluation.cpp src/engine/Search.cpp src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp src/book/OpeningBook.cpp src/tablebase/Syzygy.cpp)
target_include_directories(chesszero_v085_stabilization_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v085_stabilization_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    target_compile_options(chesszero_v085_stabilization_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v085_stabilization_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v085_stabilization_tests PRIVATE pthread)
endif()
add_test(NAME v085_stabilization COMMAND chesszero_v085_stabilization_tests)
set_tests_properties(v085_stabilization PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


add_executable(chesszero_v086_lazy_smp_tests
    tests/v086_lazy_smp_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/uci/UCI.cpp
    src/book/OpeningBook.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp)
target_include_directories(chesszero_v086_lazy_smp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v086_lazy_smp_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_compile_options(chesszero_v086_lazy_smp_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v086_lazy_smp_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v086_lazy_smp_tests PRIVATE pthread)
endif()
add_test(NAME v086_lazy_smp COMMAND chesszero_v086_lazy_smp_tests)
set_tests_properties(v086_lazy_smp PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------
# TEST v0.87: Shared TT / thread coordination
# ---------------------------------------------------------
add_executable(chesszero_v087_shared_tt_tests
    tests/v087_shared_tt_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
    src/book/OpeningBook.cpp
)
target_include_directories(chesszero_v087_shared_tt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v087_shared_tt_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(UNIX)
    target_link_libraries(chesszero_v087_shared_tt_tests PRIVATE pthread)
endif()
add_test(NAME v087_shared_tt COMMAND chesszero_v087_shared_tt_tests)
set_tests_properties(v087_shared_tt PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


# ---------------------------------------------------------
# TEST v0.88: Parallel search stabilization
# ---------------------------------------------------------
add_executable(chesszero_v088_parallel_tests
    tests/v088_parallel_stabilization_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
    src/book/OpeningBook.cpp
)
target_include_directories(chesszero_v088_parallel_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v088_parallel_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(UNIX)
    target_link_libraries(chesszero_v088_parallel_tests PRIVATE pthread)
endif()
add_test(NAME v088_parallel_stabilization COMMAND chesszero_v088_parallel_tests)
set_tests_properties(v088_parallel_stabilization PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


# ---------------------------------------------------------
# TEST v0.89 carried stress regression: stop / repeated SMP
# ---------------------------------------------------------
add_executable(chesszero_v089_stress_tests
    tests/v089_stress_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
    src/book/OpeningBook.cpp
)
target_include_directories(chesszero_v089_stress_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v089_stress_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_compile_options(chesszero_v089_stress_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v089_stress_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v089_stress_tests PRIVATE pthread)
endif()
add_test(NAME v089_race_stress COMMAND chesszero_v089_stress_tests)
set_tests_properties(v089_race_stress PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------
# TEST v0.90: Multithreading baseline / regression
# ---------------------------------------------------------
add_executable(chesszero_v090_multithreading_tests
    tests/v090_multithreading_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
    src/book/OpeningBook.cpp
)
target_include_directories(chesszero_v090_multithreading_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v090_multithreading_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_compile_options(chesszero_v090_multithreading_tests PRIVATE -march=armv8-a+simd)
    target_compile_definitions(chesszero_v090_multithreading_tests PRIVATE USE_NEON=1)
endif()
if(UNIX)
    target_link_libraries(chesszero_v090_multithreading_tests PRIVATE pthread)
endif()
add_test(NAME v090_multithreading_baseline COMMAND chesszero_v090_multithreading_tests)
set_tests_properties(v090_multithreading_baseline PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


# ---------------------------------------------------------
# TEST v0.91: Syzygy integration foundation
# ---------------------------------------------------------
add_executable(chesszero_v091_syzygy_tests
    tests/v091_syzygy_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v091_syzygy_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v091_syzygy_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v091_syzygy_integration COMMAND chesszero_v091_syzygy_tests)
set_tests_properties(v091_syzygy_integration PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


add_executable(chesszero_v092_syzygy_probe_tests
    tests/v092_syzygy_probe_tests.cpp
    src/tablebase/Syzygy.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
)
target_include_directories(chesszero_v092_syzygy_probe_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v092_syzygy_probe_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v092_syzygy_probe COMMAND chesszero_v092_syzygy_probe_tests)
set_tests_properties(v092_syzygy_probe PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# TEST v1.0.7: Syzygy 3-7 piece eligibility/discovery matrix and runtime guards
add_executable(chesszero_v107_syzygy_matrix_tests
    tests/v107_syzygy_matrix_tests.cpp
    src/tablebase/Syzygy.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
)
target_include_directories(chesszero_v107_syzygy_matrix_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v107_syzygy_matrix_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v107_syzygy_matrix COMMAND chesszero_v107_syzygy_matrix_tests)
set_tests_properties(v107_syzygy_matrix PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------
# TEST v0.93: PolyGlot binary opening-book reader
# ---------------------------------------------------------
add_executable(chesszero_v093_polyglot_tests
    tests/v093_polyglot_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/book/OpeningBook.cpp
)
target_include_directories(chesszero_v093_polyglot_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v093_polyglot_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v093_polyglot_reader COMMAND chesszero_v093_polyglot_tests)
set_tests_properties(v093_polyglot_reader PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


# TEST v0.94: canonical PolyGlot hash integration gate
add_executable(chesszero_v094_polyglot_hash_tests
    tests/v094_polyglot_hash_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
)
target_include_directories(chesszero_v094_polyglot_hash_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v094_polyglot_hash_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v094_polyglot_hash_gate COMMAND chesszero_v094_polyglot_hash_tests)

# TEST v0.95: canonical PolyGlot hash vectors
add_executable(chesszero_v095_polyglot_tests
    tests/v095_polyglot_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/book/PolyglotZobrist.cpp
)
target_include_directories(chesszero_v095_polyglot_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v095_polyglot_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v095_polyglot_vectors COMMAND chesszero_v095_polyglot_tests)


# TEST v0.96: UCI opening-book workflow regression
add_executable(chesszero_v096_book_workflow_tests
    tests/v096_book_workflow_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
)
target_include_directories(chesszero_v096_book_workflow_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v096_book_workflow_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v096_book_workflow COMMAND chesszero_v096_book_workflow_tests)
set_tests_properties(v096_book_workflow PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# TEST v0.97: UCI lifecycle / asynchronous search ownership
add_executable(chesszero_v097_uci_hardening_tests
    tests/v097_uci_hardening_tests.cpp
    src/uci/UCI.cpp
    src/engine/Search.cpp
    src/engine/Evaluation.cpp
    src/engine/TranspositionTable.cpp
    src/book/OpeningBook.cpp
    src/book/PolyglotZobrist.cpp
    src/tablebase/Syzygy.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/nnue/NNUE.cpp
)
target_include_directories(chesszero_v097_uci_hardening_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v097_uci_hardening_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_v097_uci_hardening_tests PRIVATE Threads::Threads)
add_test(NAME v097_uci_hardening COMMAND chesszero_v097_uci_hardening_tests)
set_tests_properties(v097_uci_hardening PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# TEST v0.97: interactive UCI stop/quit protocol (requires Python 3)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME v097_uci_protocol
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/v097_uci_protocol_test.py)
    set_tests_properties(v097_uci_protocol PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        ENVIRONMENT "CHESSZERO_BIN=$<TARGET_FILE:chesszero>")
endif()


# TEST v0.98: deterministic UCI clock-budget regression
add_executable(chesszero_v098_time_management_tests
    tests/v098_time_management_tests.cpp
    src/engine/Search.cpp
    src/engine/Evaluation.cpp
    src/engine/TranspositionTable.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
)
target_include_directories(chesszero_v098_time_management_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v098_time_management_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME v098_time_management COMMAND chesszero_v098_time_management_tests)
set_tests_properties(v098_time_management PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# TEST v0.98: interactive UCI clock + stop protocol
if(Python3_Interpreter_FOUND)
    add_test(NAME v098_uci_protocol
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/v098_uci_protocol_test.py)
    set_tests_properties(v098_uci_protocol PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        ENVIRONMENT "CHESSZERO_BIN=$<TARGET_FILE:chesszero>")
endif()

# TEST v0.99: release-candidate engine guards
add_executable(chesszero_v099_rc_tests
    tests/v099_rc_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/nnue/NNUE.cpp
    src/tablebase/Syzygy.cpp
    src/book/OpeningBook.cpp
)
target_include_directories(chesszero_v099_rc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(chesszero_v099_rc_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
target_link_libraries(chesszero_v099_rc_tests PRIVATE Threads::Threads)
add_test(NAME v099_rc_engine COMMAND chesszero_v099_rc_tests)
set_tests_properties(v099_rc_engine PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# TEST v0.99: interactive UCI release-candidate protocol
if(Python3_Interpreter_FOUND)
    add_test(NAME v099_uci_protocol
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/v099_uci_protocol_test.py)
    set_tests_properties(v099_uci_protocol PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        ENVIRONMENT "CHESSZERO_BIN=$<TARGET_FILE:chesszero>")
endif()


# TEST v1.0: release metadata / install surface / source version consistency
add_executable(chesszero_v100_release_tests
    tests/v100_release_tests.cpp
)
target_include_directories(chesszero_v100_release_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(chesszero_v100_release_tests PRIVATE /W4)
else()
    target_compile_options(chesszero_v100_release_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
endif()
add_test(NAME v100_release_metadata COMMAND chesszero_v100_release_tests)
set_tests_properties(v100_release_metadata PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# TEST v1.0: interactive release protocol and version identity
if(Python3_Interpreter_FOUND)
    add_test(NAME v100_uci_release_protocol
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/v100_uci_release_protocol_test.py)
    set_tests_properties(v100_uci_release_protocol PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        ENVIRONMENT "CHESSZERO_BIN=$<TARGET_FILE:chesszero>")
endif()

# TEST v1.0.1: cross-engine match runner accepts two distinct executables
if(Python3_Interpreter_FOUND)
    add_test(NAME v101_match_runner
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/v102_match_runner_test.py)
    set_tests_properties(v101_match_runner PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# TEST v1.0.3: cached bitboard state must remain identical to a full recomputation
add_executable(chesszero_v103_bitboard_cache_tests
    tests/v103_bitboard_cache_tests.cpp
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/chess/Zobrist.cpp
    src/bitboard/Bitboard.cpp
)
target_include_directories(chesszero_v103_bitboard_cache_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(chesszero_v103_bitboard_cache_tests PRIVATE /W4)
else()
    target_compile_options(chesszero_v103_bitboard_cache_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    # TEST 93: real Syzygy/Fathom backend API smoke test
    add_executable(chesszero_v093_syzygy_real_tests
        tests/v093_syzygy_real_tests.cpp
        src/tablebase/Syzygy.cpp
        src/chess/Position.cpp
        src/chess/Move.cpp
        src/chess/Zobrist.cpp
        src/bitboard/Bitboard.cpp
    )
    target_include_directories(chesszero_v093_syzygy_real_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(chesszero_v093_syzygy_real_tests PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    add_test(NAME v093_syzygy_real_backend COMMAND chesszero_v093_syzygy_real_tests)

endif()
add_test(NAME v103_bitboard_cache COMMAND chesszero_v103_bitboard_cache_tests)
set_tests_properties(v103_bitboard_cache PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
