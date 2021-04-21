cmake_minimum_required(VERSION 3.0.0)
project(zpack)

cmake_policy(SET CMP0077 NEW)
option(ZPACK_BUILD_PROGRAMS "Build the ZPack command line utility" ON)

add_subdirectory(lib)
if(ZPACK_BUILD_PROGRAMS)
    add_subdirectory(programs)
endif()
