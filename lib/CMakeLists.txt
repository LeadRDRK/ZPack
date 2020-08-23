find_package(ZSTD REQUIRED)

add_library(zpack SHARED
    reader.cpp
    unpacker.cpp
    writer.cpp
    packer.cpp
    crc.c

    zpack.h
    utils.h
    crc.h
)
set_property(TARGET zpack PROPERTY CXX_STANDARD 11)
target_link_libraries(zpack zstd)