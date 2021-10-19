# zstd
if(NOT ZPACK_USE_SYSTEM_ZSTD)
    set(ZSTD_BUILD_PROGRAMS OFF)
    if(ZPACK_BUILD_ZSTD_STATIC)
        set(ZSTD_BUILD_SHARED OFF)
    else()
        set(ZSTD_BUILD_STATIC OFF)
    endif()
    add_subdirectory(zstd/build/cmake)
endif()

# xxHash
if(NOT ZPACK_USE_SYSTEM_XXHASH)
    add_subdirectory(xxHash/cmake_unofficial)
endif()