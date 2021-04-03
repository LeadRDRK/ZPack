cmake_minimum_required(VERSION 3.0.0)
project(zpack)

option(ZPACK_BUILD_PROGRAMS "Build the ZPack command line utility" ON)

add_subdirectory(lib)
if(ZPACK_BUILD_PROGRAMS)
    add_subdirectory(programs)
endif()
