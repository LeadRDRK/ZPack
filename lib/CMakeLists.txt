set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# xxHash
if(NOT DEFINED xxHash_LIBRARIES)
    if(ZPACK_USE_SYSTEM_XXHASH)
        find_package(xxHash REQUIRED)
    else()
        set(xxHash_LIBRARIES xxhash)
    endif()
endif()

# zstd
if(NOT ZPACK_DISABLE_ZSTD)
    if(NOT DEFINED ZSTD_LIBRARIES)
        if(ZPACK_USE_SYSTEM_ZSTD)
            find_package(ZSTD REQUIRED)
        else()
            # prefer shared library
            if(TARGET libzstd_shared)
                set(ZSTD_LIBRARIES libzstd_shared)
            else()
                set(ZSTD_LIBRARIES libzstd_static)
            endif()
            set(ZSTD_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/externals/zstd/lib)
        endif()
    endif()
endif()

# lz4
if(NOT ZPACK_DISABLE_LZ4)
    if(NOT DEFINED LZ4_LIBRARIES)
        if(ZPACK_USE_SYSTEM_LZ4)
            find_package(LZ4 REQUIRED)
        else()
            if(BUILD_SHARED_LIBS)
                set(LZ4_LIBRARIES lz4_shared)
            else()
                set(LZ4_LIBRARIES lz4_static)
            endif()

            ## cheap workaround
            # since lz4 includes its own xxhash header in the lib directory,
            # copy the lz4 headers to another directory to prevent include conflicts
            file(COPY ${PROJECT_SOURCE_DIR}/externals/lz4/lib/lz4.h DESTINATION ${CMAKE_BINARY_DIR}/lib)
            file(COPY ${PROJECT_SOURCE_DIR}/externals/lz4/lib/lz4hc.h DESTINATION ${CMAKE_BINARY_DIR}/lib)
            file(COPY ${PROJECT_SOURCE_DIR}/externals/lz4/lib/lz4frame.h DESTINATION ${CMAKE_BINARY_DIR}/lib)

            set(LZ4_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/lib)
        endif()
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

add_library(zpack ${ZPACK_LIBRARY_TYPE}
    zpack_common.c
    zpack_read.c
    zpack_stream.c
    zpack_write.c

    zpack_common.h
    zpack.h
)
target_link_libraries(zpack ${xxHash_LIBRARIES} ${ZSTD_LIBRARIES} ${LZ4_LIBRARIES})
target_include_directories(zpack PRIVATE ${xxHash_INCLUDE_DIRS} ${ZSTD_INCLUDE_DIRS} ${LZ4_INCLUDE_DIRS})
target_compile_definitions(zpack PRIVATE ${ZPACK_ENDIAN_DEFS} ${ZPACK_LFS_DEFS} ${ZPACK_DISABLE_DEFS})
set_target_properties(zpack PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER zpack.h
)

install(TARGETS zpack
    LIBRARY
    PUBLIC_HEADER
)