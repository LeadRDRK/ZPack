ZPack
================================
[![build](https://github.com/LeadRDRK/ZPack/actions/workflows/cmake.yml/badge.svg)](https://github.com/LeadRDRK/ZPack/actions/workflows/cmake.yml)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/LeadRDRK/ZPack.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/LeadRDRK/ZPack/context:cpp)

ZPack is a simple, general-purpose archive file format with lossless data compression (similar to zip, rar, etc.). It is designed with performance in mind and uses fast compression algorithms such as [zstd](https://github.com/facebook/zstd) and [lz4](https://github.com/lz4/lz4).

Documentation
-------------------------
The specifications for the ZPack file format can be found [here](docs/specs.md)

The API documentation can be generated using Doxygen. An online copy of it can be found [here](https://leadrdrk.eu.org/zpack)

Installing
-------------------------
Prebuilt Windows and Linux binaries are available on the [Releases](https://github.com/LeadRDRK/ZPack/releases) page.

Compiling
-------------------------
The CMake lists in the root directory can be used to build both the library and the command line utility.

A few options are available when configuring with CMake. Here are some common options:
- `ZPACK_BUILD_PROGRAMS`: Build the ZPack command line utility. Default: `ON`
- `ZPACK_BUILD_TESTS`: Build the ZPack unit tests. Default: `OFF`
- `ZPACK_DISABLE_ZSTD`: Disable zstd support. Default: `OFF`
- `ZPACK_DISABLE_LZ4`: Disable LZ4 support. Default: `OFF`

More options can be found in [CMakeLists.txt](CMakeLists.txt)

To use the library in your CMake project:
```cmake
set(ZPACK_BUILD_PROGRAMS OFF) # optional
add_subdirectory(path/to/zpack)

target_include_directories(target PRIVATE path/to/zpack/lib)
target_link_libraries(target PRIVATE zpack)
```

License
-------------------------
ZPack is licensed under the [zlib License](LICENSE)
