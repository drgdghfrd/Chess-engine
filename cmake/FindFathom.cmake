cmake_policy(SET CMP0169 OLD)
# ChessZero Fathom fetch helper.
# Fathom is pinned to a known commit for reproducible builds.
set(CHESSZERO_FATHOM_REPOSITORY "https://github.com/jdart1/Fathom.git" CACHE STRING "Fathom Git repository")
set(CHESSZERO_FATHOM_TAG "c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e" CACHE STRING "Pinned Fathom commit")
set(CHESSZERO_FATHOM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party/Fathom" CACHE PATH "Use a local Fathom source tree instead of downloading it")
option(CHESSZERO_FATHOM_OFFLINE "Fail instead of fetching Fathom when local source is missing" OFF)
if(CHESSZERO_FATHOM_SOURCE_DIR AND NOT EXISTS "${CHESSZERO_FATHOM_SOURCE_DIR}/src/tbprobe.c")
    if(CHESSZERO_REQUIRE_VENDORED_FATHOM OR CHESSZERO_FATHOM_OFFLINE)
        message(FATAL_ERROR "Fathom source is missing at CHESSZERO_FATHOM_SOURCE_DIR=${CHESSZERO_FATHOM_SOURCE_DIR}; offline/required-vendored mode forbids FetchContent")
    endif()
    set(CHESSZERO_FATHOM_SOURCE_DIR "" CACHE PATH "Use a local Fathom source tree instead of downloading it" FORCE)
endif()

if(CHESSZERO_FATHOM_SOURCE_DIR)
    if(NOT EXISTS "${CHESSZERO_FATHOM_SOURCE_DIR}/src/tbprobe.c")
        message(FATAL_ERROR "CHESSZERO_FATHOM_SOURCE_DIR does not contain src/tbprobe.c")
    endif()
    set(CHESSZERO_FATHOM_DIR "${CHESSZERO_FATHOM_SOURCE_DIR}")
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
    set(CHESSZERO_FATHOM_DIR "${chesszero_fathom_SOURCE_DIR}")
endif()

set(CHESSZERO_FATHOM_SRC
    "${CHESSZERO_FATHOM_DIR}/src/tbprobe.c"
)
set(CHESSZERO_FATHOM_INCLUDE_DIR "${CHESSZERO_FATHOM_DIR}/src")
