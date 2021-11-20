# zstd
if(NOT ZPACK_DISABLE_ZSTD AND NOT ZPACK_USE_SYSTEM_ZSTD)
    set(ZSTD_BUILD_PROGRAMS OFF)
    # set build types explicitly
    if(BUILD_SHARED_LIBS)
        set(ZSTD_BUILD_SHARED ON)
        set(ZSTD_BUILD_STATIC OFF)
    else()
        set(ZSTD_BUILD_SHARED OFF)
        set(ZSTD_BUILD_STATIC ON)
    endif()
    add_subdirectory(zstd/build/cmake)
endif()

# lz4
if(NOT ZPACK_DISABLE_LZ4 AND NOT ZPACK_USE_SYSTEM_LZ4)
    set(LZ4_BUILD_CLI OFF)
    set(LZ4_BUNDLED_MODE OFF)
    set(LZ4_BUILD_CLI OFF)
    set(LZ4_BUILD_LEGACY_LZ4C OFF)
    add_subdirectory(lz4/build/cmake)
endif()

# xxHash
if(NOT ZPACK_USE_SYSTEM_XXHASH)
    set(XXHASH_BUILD_XXHSUM OFF)
    add_subdirectory(xxHash/cmake_unofficial)
endif()