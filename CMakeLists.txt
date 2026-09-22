cmake_minimum_required(VERSION 3.16)

project(UltraChess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(UC_SRC
    src/chess/Position.cpp
    src/chess/Move.cpp
    src/bitboard/Bitboard.cpp
    src/nnue/NNUE.cpp
    src/engine/Evaluation.cpp
    src/engine/Search.cpp
    src/engine/TranspositionTable.cpp
    src/tablebase/Syzygy.cpp
)

add_executable(ultrachess
    src/main.cpp
    ${UC_SRC}
    src/uci/UCI.cpp
    src/book/OpeningBook.cpp
)

target_include_directories(ultrachess PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(ultrachess PRIVATE
    -O3
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
    -ffunction-sections
    -fdata-sections
)

target_link_options(ultrachess PRIVATE
)

if(UNIX)
    target_link_libraries(ultrachess PRIVATE pthread)
endif()

option(ULTRACHESS_ANDROID_OEX
    "Build UltraChess as an Android OEX executable"
    OFF
)

if(ULTRACHESS_ANDROID_OEX)

    # OEX expects an executable with this filename.
    set_target_properties(ultrachess PROPERTIES
        OUTPUT_NAME "libultrachess"
        PREFIX ""
        SUFFIX ".so"
        POSITION_INDEPENDENT_CODE ON
    )

    target_compile_options(ultrachess PRIVATE
        -fPIE
    )

    target_link_options(ultrachess PRIVATE
        -fPIE
        -pie
        -static-libstdc++
        -Wl,--gc-sections
    )

endif()


# =========================
# Regression / Perft test
# =========================

add_executable(ultrachess-perft
    tests/perft_regression.cpp
    ${UC_SRC}
)

add_executable(ultrachess-position-regression
    tests/position_regression.cpp
    ${UC_SRC}
)

target_include_directories(ultrachess-position-regression PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(ultrachess-position-regression PRIVATE
    -O3
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)

if(UNIX)
    target_link_libraries(ultrachess-position-regression PRIVATE pthread)
endif()

target_include_directories(ultrachess-perft PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(ultrachess-perft PRIVATE
    -O3
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)

if(UNIX)
    target_link_libraries(ultrachess-perft PRIVATE pthread)
endif()

# =========================
# Benchmark
# =========================

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/bench.cpp")
    add_executable(ultrachess-bench
        tools/bench.cpp
        ${UC_SRC}
    )

    target_include_directories(ultrachess-bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_options(ultrachess-bench PRIVATE
        -O3
        -DNDEBUG
        -Wall
        -Wextra
        -Wpedantic
    )

    if(UNIX)
        target_link_libraries(ultrachess-bench PRIVATE pthread)
    endif()
endif()
