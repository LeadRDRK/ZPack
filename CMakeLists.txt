cmake_minimum_required(VERSION 3.0.0)
project(zpack
    LANGUAGES C
)

cmake_policy(SET CMP0077 NEW)
include(CMakeDependentOption)

option(ZPACK_BUILD_PROGRAMS "Build the ZPack command line utility" ON)
option(ZPACK_BUILD_TESTS "Build the ZPack unit tests" OFF)

option(ZPACK_DISABLE_ZSTD "Disable zstd support" OFF)
option(ZPACK_DISABLE_LZ4 "Disable LZ4 support" OFF)
option(ZPACK_DISABLE_UNICODE "Disable Unicode support for paths on Windows" OFF)

option(ZPACK_USE_SYSTEM_LIBS "Use the system's libraries for all dependencies" OFF)
cmake_dependent_option(ZPACK_USE_SYSTEM_ZSTD "Use the system's zstd library" OFF "NOT ZPACK_DISABLE_ZSTD; NOT ZPACK_USE_SYSTEM_LIBS" ON)
cmake_dependent_option(ZPACK_USE_SYSTEM_LZ4 "Use the system's LZ4 library" OFF "NOT ZPACK_DISABLE_LZ4; NOT ZPACK_USE_SYSTEM_LIBS" ON)
cmake_dependent_option(ZPACK_USE_SYSTEM_XXHASH "Use the system's xxHash library" OFF "NOT ZPACK_USE_SYSTEM_LIBS" ON)

# validate options
if(ZPACK_DISABLE_ZSTD AND ZPACK_DISABLE_LZ4)
    message(FATAL_ERROR "At least one compression library must be enabled")
endif()
if(ZPACK_DISABLE_ZSTD)
    set(ZPACK_DISABLE_DEFS ${ZPACK_DISABLE_DEFS} "ZPACK_DISABLE_ZSTD")
endif()
if(ZPACK_DISABLE_LZ4)
    set(ZPACK_DISABLE_DEFS ${ZPACK_DISABLE_DEFS} "ZPACK_DISABLE_LZ4")
endif()
if(ZPACK_DISABLE_UNICODE)
    set(ZPACK_DISABLE_DEFS ${ZPACK_DISABLE_DEFS} "ZPACK_DISABLE_UNICODE")
endif()

add_subdirectory(externals)

# detect and enable large file support
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
if(UNIX AND NOT APPLE)
    include(TestLargeFiles)
    ZPACK_TEST_LARGE_FILES(ZPACK_LFS_SUPPORTED)
    if(ZPACK_LFS_SUPPORTED)
        set(ZPACK_LFS_DEFS _FILE_OFFSET_BITS=64 _LARGE_FILES _LARGEFILE64_SOURCE)
    endif()
endif()

add_subdirectory(lib)

if(ZPACK_BUILD_PROGRAMS)
    add_subdirectory(programs)
endif()

if(ZPACK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()