# cmake/embed_title.cmake
# Usage:
#   cmake -DTITLE_FILE=assets/title.txt -DOUTPUT_FILE=src/title_data.h -P cmake/embed_title.cmake

if(NOT EXISTS "${TITLE_FILE}")
    message(FATAL_ERROR
        "Title file not found: ${TITLE_FILE}\n"
        "Place your title at assets/title.txt and re-run the build.\n"
        "  cmake --build . --target embed_title")
endif()

file(READ "${TITLE_FILE}" raw_title)
string(REGEX REPLACE "[\r\n]+$" "" raw_title "${raw_title}")
string(REPLACE "\\" "\\\\" escaped_title "${raw_title}")
string(REPLACE "\"" "\\\"" escaped_title "${escaped_title}")

set(header_content
"// AUTO-GENERATED — do not edit.
// Source: ${TITLE_FILE}
#pragma once

static constexpr const char kWindowTitle[] = \"${escaped_title}\";
")

file(WRITE "${OUTPUT_FILE}" "${header_content}")
message(STATUS "Embedded title from ${TITLE_FILE} -> ${OUTPUT_FILE}")
