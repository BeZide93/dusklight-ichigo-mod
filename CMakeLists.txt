cmake_minimum_required(VERSION 3.26)

if (NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED ENV{CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type")
endif ()

project(ichigo_mod VERSION 1.1.0 LANGUAGES CXX)

# The Dusklight version to build against: a release tag (e.g. "v2.0.0") or a commit SHA.
if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "v2.0.0")
endif ()
# Fetches the Dusklight sources into ./dusklight and provides DUSKLIGHT_DIR.
# Use -DDUSKLIGHT_DIR= to point at an existing Dusklight checkout.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDusklight.cmake")
add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)

configure_file(mod.json.in "${CMAKE_CURRENT_BINARY_DIR}/mod.json" @ONLY NEWLINE_STYLE LF)

include(FetchContent)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ichigo_json
        URL https://github.com/nlohmann/json/archive/9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(ichigo_json)

add_mod(ichigo_mod
        FEATURES game          # remove for service/asset-only mods; add webgpu for GfxService
        SOURCES src/mod.cpp src/update_service.cpp
        MOD_JSON "${CMAKE_CURRENT_BINARY_DIR}/mod.json"
        RES_DIR res            # mod resources, including icon.png and banner.png
        OVERLAY_DIR overlay    # game file overlays; remove if unused
        TEXTURES_DIR textures  # texture replacements; remove if unused
)
target_link_libraries(ichigo_mod PRIVATE nlohmann_json::nlohmann_json)

option(ICHIGO_BUILD_TESTS "Build updater regression tests" OFF)
if(ICHIGO_BUILD_TESTS)
    enable_testing()
    add_executable(update_service_test tests/update_service_test.cpp)
    target_compile_features(update_service_test PRIVATE cxx_std_20)
    target_link_libraries(update_service_test PRIVATE dusklight_mod_api nlohmann_json::nlohmann_json)
    add_test(NAME update_service COMMAND update_service_test)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Linux permits unresolved symbols in modules; service pointer definitions
        # must still be supplied by IMPORT_SERVICE rather than the game binary.
        add_test(NAME service_imports COMMAND ${CMAKE_COMMAND}
            "-DMOD_FILE=$<TARGET_FILE:ichigo_mod>" "-DNM=${CMAKE_NM}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_service_imports.cmake")
    endif()
endif()
