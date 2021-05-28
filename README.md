# ZPack
[![Total alerts](https://img.shields.io/lgtm/alerts/g/LeadRDRK/ZPack.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/LeadRDRK/ZPack/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/LeadRDRK/ZPack.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/LeadRDRK/ZPack/context:cpp)

ZPack is a simple, general-purpose archive file format with lossless data compression.

Designed to be extremely fast, it exclusively uses the Zstandard compression algorithm for file compression (hence the name)

More about Zstandard/zstd at https://facebook.github.io/zstd
# Documentation
The specifications for the ZPack file format can be found [here](docs/specs.txt)

The API documentation can be generated using the Doxyfile in docs/Doxyfile. An online copy of it can be found [here](https://leadrdrk.eu.org/zpack)
# Usage
The CMake list in the root directory can be used to build both the library and the command line utility.

To use the library in your CMake project:
```cmake
option(ZPACK_BUILD_PROGRAMS "Build the ZPack command line utility" OFF) # optional
add_subdirectory(path/to/zpack)
###############################
target_include_directories(MyTarget PRIVATE path/to/zpack/lib)
target_link_libraries(MyTarget PRIVATE zpack)
```
# License
ZPack is licensed under the [BSD 3-Clause license](LICENSE)
