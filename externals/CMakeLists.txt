# zstd
if(NOT ZPACK_DISABLE_ZSTD AND NOT ZPACK_USE_SYSTEM_ZSTD)
    set(ZSTD_BUILD_PROGRAMS OFF)
    add_subdirectory(zstd/build/cmake)
endif()

# lz4
if(NOT ZPACK_DISABLE_LZ4 AND NOT ZPACK_USE_SYSTEM_LZ4)
    set(LZ4_BUILD_CLI OFF)
    add_subdirectory(lz4/build/cmake)
endif()

# xxHash
if(NOT ZPACK_USE_SYSTEM_XXHASH)
    add_subdirectory(xxHash/cmake_unofficial)
endif()