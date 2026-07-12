cmake_minimum_required(VERSION 3.20)
project(breakdance CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── vcpkg toolchain (optional) ──────────────────────────────────────────────
# Pass -DVCPKG_ROOT=... or export VCPKG_ROOT in the environment.
# If absent the build falls back to pkg-config (system packages on Linux).
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(NOT DEFINED VCPKG_ROOT AND DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    endif()
    if(DEFINED VCPKG_ROOT)
        set(CMAKE_TOOLCHAIN_FILE
            "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
        if(WIN32)
            set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
        else()
            set(VCPKG_TARGET_TRIPLET "x64-linux"          CACHE STRING "")
        endif()
    endif()
endif()

# ── Dependencies ────────────────────────────────────────────────────────────
# Try CMake config packages first (vcpkg), then fall back to pkg-config.
find_package(SDL2  CONFIG QUIET)
find_package(FFMPEG       QUIET)

if(NOT SDL2_FOUND OR NOT FFMPEG_FOUND)
    find_package(PkgConfig REQUIRED)
endif()

if(NOT SDL2_FOUND)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(SDL2_VIA_PKGCONFIG TRUE)
endif()

if(NOT FFMPEG_FOUND)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec libavformat libavutil libswscale)
    set(FFMPEG_VIA_PKGCONFIG TRUE)
endif()

# ── Embed assets ────────────────────────────────────────────────────────────
# Generate headers only when the source asset changes.
set(VIDEO_HEADER ${CMAKE_SOURCE_DIR}/src/video_data.h)
set(TITLE_HEADER ${CMAKE_SOURCE_DIR}/src/title_data.h)

add_custom_command(
    OUTPUT ${VIDEO_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DVIDEO_FILE=${CMAKE_SOURCE_DIR}/assets/video.mp4
        -DOUTPUT_FILE=${VIDEO_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/embed_video.cmake
    DEPENDS
        ${CMAKE_SOURCE_DIR}/assets/video.mp4
        ${CMAKE_SOURCE_DIR}/cmake/embed_video.cmake
    COMMENT "Embedding video into src/video_data.h"
    VERBATIM
)

add_custom_target(embed_video DEPENDS ${VIDEO_HEADER})

add_custom_command(
    OUTPUT ${TITLE_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DTITLE_FILE=${CMAKE_SOURCE_DIR}/assets/title.txt
        -DOUTPUT_FILE=${TITLE_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/embed_title.cmake
    DEPENDS
        ${CMAKE_SOURCE_DIR}/assets/title.txt
        ${CMAKE_SOURCE_DIR}/cmake/embed_title.cmake
    COMMENT "Embedding title into src/title_data.h"
    VERBATIM
)

add_custom_target(embed_title DEPENDS ${TITLE_HEADER})

# ── Executable ──────────────────────────────────────────────────────────────
# WIN32: suppresses the console window on Windows; ignored on Linux/macOS.
add_executable(breakdance WIN32
    src/main.cpp
    src/player.cpp
    src/window_ui.cpp
)

add_dependencies(breakdance embed_video embed_title)

target_include_directories(breakdance PRIVATE src)

# ── Link: SDL2 ──────────────────────────────────────────────────────────────
if(SDL2_VIA_PKGCONFIG)
    target_link_libraries(breakdance PRIVATE PkgConfig::SDL2)
else()
    # vcpkg provides SDL2::SDL2-static (Windows static triplet) or SDL2::SDL2
    if(TARGET SDL2::SDL2-static)
        target_link_libraries(breakdance PRIVATE SDL2::SDL2-static)
    else()
        target_link_libraries(breakdance PRIVATE SDL2::SDL2)
    endif()
    if(TARGET SDL2::SDL2main)
        target_link_libraries(breakdance PRIVATE SDL2::SDL2main)
    endif()
endif()

# ── Link: FFmpeg ─────────────────────────────────────────────────────────────
if(FFMPEG_VIA_PKGCONFIG)
    target_link_libraries(breakdance PRIVATE PkgConfig::FFMPEG)
else()
    target_include_directories(breakdance PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(breakdance PRIVATE ${FFMPEG_LIBRARIES})
endif()

# ── Single-binary linking ────────────────────────────────────────────────────
if(MSVC)
    # Static CRT (no MSVCRT*.dll dependency)
    set_property(TARGET breakdance PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(WIN32)
    # MinGW: fully static (no DLL dependencies)
    target_link_options(breakdance PRIVATE -static)
else()
    # Linux/WSL2: bake libgcc + libstdc++ in; keep glibc dynamic (glibc ABI
    # is stable across distros so this is universally safe).
    target_link_options(breakdance PRIVATE -static-libgcc -static-libstdc++)
endif()
