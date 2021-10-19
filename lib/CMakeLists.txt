set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# zstd
if(NOT DEFINED ZSTD_LIBRARY)
    if(ZPACK_USE_SYSTEM_ZSTD)
        find_package(ZSTD REQUIRED)
    else()
        if(ZPACK_BUILD_ZSTD_STATIC)
            set(ZSTD_LIBRARY libzstd_static)
        else()
            set(ZSTD_LIBRARY libzstd_shared)
        endif()
        set(ZSTD_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/externals/zstd/lib)
    endif()
endif()

# xxHash
if(NOT DEFINED xxHash_LIBRARIES)
    if(ZPACK_USE_SYSTEM_XXHASH)
        find_package(xxHash REQUIRED)
    else()
        set(xxHash_LIBRARIES xxhash)
    endif()
endif()

# check library type
if(NOT DEFINED ZPACK_LIBRARY_TYPE)
    if(BUILD_SHARED_LIBS)
        set(ZPACK_LIBRARY_TYPE SHARED)
    else()
        set(ZPACK_LIBRARY_TYPE STATIC)
    endif()
endif()

# detect endianness
if(${CMAKE_VERSION} VERSION_LESS "3.20.0")
    # old method
    include(TestBigEndian)
    test_big_endian(ZPACK_IS_BIG_ENDIAN)
    if(NOT ZPACK_IS_BIG_ENDIAN)
        set(ZPACK_ENDIAN_DEFS ZPACK_LITTLE_ENDIAN)
    endif()
else()
    # new method
    if(CMAKE_C_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
        set(ZPACK_ENDIAN_DEFS ZPACK_LITTLE_ENDIAN)
    endif()
endif()

# detect and enable large file support
include(TestLargeFiles)
ZPACK_TEST_LARGE_FILES(ZPACK_LFS_SUPPORTED)
if(ZPACK_LFS_SUPPORTED)
    set(ZPACK_LFS_DEFS _FILE_OFFSET_BITS=64 _LARGE_FILES _LARGEFILE64_SOURCE)
endif()

add_library(zpack ${ZPACK_LIBRARY_TYPE}
    zpack_common.c
    zpack_read.c
    zpack_write.c

    zpack_common.h
    zpack.h
)
set_property(TARGET zpack PROPERTY C_STANDARD 99)
target_link_libraries(zpack ${ZSTD_LIBRARY} ${xxHash_LIBRARIES})
target_include_directories(zpack PRIVATE ${ZSTD_INCLUDE_DIR} ${xxHash_INCLUDE_DIRS})
target_compile_definitions(zpack PRIVATE ${ZPACK_ENDIAN_DEFS} ${ZPACK_LFS_DEFS})