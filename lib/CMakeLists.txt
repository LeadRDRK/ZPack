list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
find_package(ZSTD REQUIRED)

add_library(zpack
    reader.cpp
    writer.cpp
    crc.cpp

    zpack.h
    zpack_utils.h
    zpack_crc.h
)
set_property(TARGET zpack PROPERTY CXX_STANDARD 11)
target_link_libraries(zpack zstd)
