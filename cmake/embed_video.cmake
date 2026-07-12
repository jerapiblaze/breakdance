# cmake/embed_video.cmake
# Usage (from CMakeLists.txt custom target, or standalone):
#   cmake -DVIDEO_FILE=assets/video.mp4 -DOUTPUT_FILE=src/video_data.h -P cmake/embed_video.cmake

if(NOT EXISTS "${VIDEO_FILE}")
    message(FATAL_ERROR
        "Video file not found: ${VIDEO_FILE}\n"
        "Place your video at assets/video.mp4 and re-run the build.\n"
        "  cmake --build . --target embed_video")
endif()

file(READ "${VIDEO_FILE}" raw_hex HEX)

# Insert a comma+newline every 16 bytes (32 hex chars) for readable output
string(LENGTH "${raw_hex}" hex_len)
set(formatted "")
set(i 0)
while(i LESS hex_len)
    string(SUBSTRING "${raw_hex}" ${i} 32 chunk)
    string(LENGTH "${chunk}" chunk_len)
    # Convert pairs of hex chars to "0xNN," tokens
    set(j 0)
    while(j LESS chunk_len)
        string(SUBSTRING "${chunk}" ${j} 2 byte_hex)
        if(byte_hex STREQUAL "")
            break()
        endif()
        string(APPEND formatted "0x${byte_hex},")
        math(EXPR j "${j} + 2")
    endwhile()
    string(APPEND formatted "\n")
    math(EXPR i "${i} + 32")
endwhile()

# Strip trailing comma on the very last byte
string(REGEX REPLACE ",(\n*)$" "\\1" formatted "${formatted}")

math(EXPR byte_count "${hex_len} / 2")

set(header_content
"// AUTO-GENERATED — do not edit.
// Source: ${VIDEO_FILE}  (${byte_count} bytes)
#pragma once
#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(modernize-avoid-c-arrays)
static const uint8_t kVideoData[] = {
${formatted}
};

static constexpr std::size_t kVideoDataSize = ${byte_count};
")

file(WRITE "${OUTPUT_FILE}" "${header_content}")
message(STATUS "Embedded ${byte_count} bytes from ${VIDEO_FILE} -> ${OUTPUT_FILE}")
