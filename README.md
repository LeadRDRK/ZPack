ZPack
================================
[![Total alerts](https://img.shields.io/lgtm/alerts/g/LeadRDRK/ZPack.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/LeadRDRK/ZPack/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/LeadRDRK/ZPack.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/LeadRDRK/ZPack/context:cpp)

ZPack is a simple, general-purpose archive file format with lossless data compression. It is designed with performance in mind and uses fast compression algorithms such as [zstd](https://github.com/facebook/zstd) and [lz4](https://github.com/lz4/lz4).

Documentation
-------------------------
The specifications for the ZPack file format can be found [here](docs/specs.md)

The API documentation can be generated using Doxygen with docs/Doxyfile. An online copy of it can be found [here](https://leadrdrk.eu.org/zpack)

Installing
-------------------------
You can get the Windows binaries for the latest release on the Releases page. Otherwise, you may compile it manually.

Compiling
-------------------------
The CMakeLists.txt file in the root directory can be used to build both the library and the command line utility.

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
