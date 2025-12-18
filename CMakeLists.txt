cmake_minimum_required(VERSION 3.23)
project(BRender VERSION 1.3.2 LANGUAGES C)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Prefer glvnd
cmake_policy(SET CMP0072 NEW)

cmake_policy(SET CMP0079 NEW)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BRENDER_BUILD_DRIVERS "Build Drivers" ON)
option(BRENDER_BUILD_EXAMPLES "Build Examples" "${BRender_IS_TOPLEVEL}")
option(BRENDER_INSTALL "Enable installation" "${BRender_IS_TOPLEVEL}")
option(BRENDER_ASAN_ENABLED "Address Sanitizer" OFF)

add_subdirectory(core)

if(CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    set(WORDS_BIGENDIAN TRUE)
    add_definitions(-DBR_ENDIAN_BIG=1 -DBR_ENDIAN_LITTLE=0)
elseif(CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    set(WORDS_BIGENDIAN FALSE)
    add_definitions(-DBR_ENDIAN_BIG=0 -DBR_ENDIAN_LITTLE=1)
else()
    set(WORDS_BIGENDIAN FALSE)
    message(WARNING "Endianness could not be determined.")
endif()

if(BRENDER_BUILD_DRIVERS)
    add_subdirectory(x86emu)
    add_subdirectory(drivers)
endif()

# #
# Core BRender, no drivers, no DDI.
# #
add_library(BRender::Core ALIAS brender)

# #
# Full BRender, with drivers, no DDI.
# #
add_library(BRender::Full ALIAS brender-full)

# #
# Core BRender, no drivers, with DDI.
# #
add_library(BRender::DDI ALIAS brender-ddi)

if(BRENDER_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(BRENDER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BRENDER_INSTALL)
    include(cmake/packaging.cmake)
    include(CPack)
endif()
