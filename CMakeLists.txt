cmake_minimum_required(VERSION 3.0.0)
project(zpack)

cmake_policy(SET CMP0077 NEW)

option(ZPACK_BUILD_PROGRAMS "Build the ZPack command line utility" ON)
option(ZPACK_BUILD_TESTS "Build the ZPack unit tests" OFF)

option(ZPACK_BUILD_ZSTD_STATIC "Build zstd as a static library" ON)
option(ZPACK_USE_SYSTEM_ZSTD "Use the system's zstd library instead" OFF)
option(ZPACK_USE_SYSTEM_XXHASH "Use the system's xxHash library instead" OFF)

add_subdirectory(externals)
add_subdirectory(lib)

if(ZPACK_BUILD_PROGRAMS)
    #add_subdirectory(programs)
endif()

if(ZPACK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()