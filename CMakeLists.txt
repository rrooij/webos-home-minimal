cmake_minimum_required(VERSION 3.16)
project(webos-home-minimal C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(APP_ID "org.rrooij.homeminimal" CACHE STRING "webOS application id")
option(WEBOS "Build for a webOS TV (fullscreen, LG SDL hints)" ${CMAKE_CROSSCOMPILING})

include(FetchContent)

# --- LVGL ------------------------------------------------------------------
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf.h" CACHE PATH "" FORCE)
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.2.2
    GIT_SHALLOW TRUE)

# --- JSON ------------------------------------------------------------------
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)

# --- stb_image (background JPEG, baseline and progressive) ------------------
FetchContent_Declare(stb
    URL https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE)

FetchContent_MakeAvailable(lvgl json stb)
target_include_directories(lvgl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/config")

# --- SDL2 (system SDL2 on webOS, comes with the buildroot-nc4 toolchain) ----
find_package(SDL2 CONFIG QUIET)
if(NOT TARGET SDL2::SDL2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
endif()

add_executable(webos-home-minimal
    src/main.cpp
    src/sdl_backend.cpp
    src/luna.cpp
    src/apps.cpp
    src/config.cpp
    src/launcher_ui.cpp
    src/background.cpp)

target_compile_definitions(webos-home-minimal PRIVATE APP_ID="${APP_ID}")
if(WEBOS)
    target_compile_definitions(webos-home-minimal PRIVATE TARGET_WEBOS)
endif()
target_compile_options(webos-home-minimal PRIVATE -Wall -Wextra)
target_include_directories(webos-home-minimal SYSTEM PRIVATE "${stb_SOURCE_DIR}")
target_link_libraries(webos-home-minimal PRIVATE lvgl nlohmann_json::nlohmann_json SDL2::SDL2)

# --- webOS IPK staging dir ---------------------------------------------------
set(PKG_DIR "${CMAKE_BINARY_DIR}/pkg")
configure_file(webos/appinfo.json.in "${PKG_DIR}/appinfo.json" @ONLY)
add_custom_command(TARGET webos-home-minimal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:webos-home-minimal> "${PKG_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/webos/icon.png" "${PKG_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/webos/home.conf" "${PKG_DIR}/")

# Optional custom background: put webos/background.jpeg in the tree to ship it
# in the IPK (re-run cmake after adding or removing it).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/webos/background.jpeg")
    add_custom_command(TARGET webos-home-minimal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/webos/background.jpeg" "${PKG_DIR}/")
endif()
