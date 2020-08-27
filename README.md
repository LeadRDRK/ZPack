# ZPack
ZPack is a simple, general-purpose archive file format with lossless data compression. It is based on the ZIP/ZIP64 format by PKWARE.

Designed to be extremely fast, it exclusively uses the Zstandard compression algorithm for file compression (hence the name)

More about Zstandard/zstd at https://facebook.github.io/zstd
# Status
ZPack is still a relatively new format. Its implementation is also only recently developed, therefore the API is not perfect yet but it's already fully functional and can be used in your own projects. However, it should not be considered as stable and API changes can be made at any time. Be mindful when upgrading to a newer version.
# Documentation
The specifications for the ZPack file format can be found [here](docs/specs.txt)

The API documentation can be generated using the Doxyfile in docs/Doxyfile. An online copy of it can be found [here](https://leadrdrk.eu.org/zpack)
# Development
You are welcome to make any contributions in this project. Code that you wrote must be portable and compatible with C++11.
## Building
You can build both the library and the CLI using the CMake file in the root directory.

To include the library in your own project, add the /lib directory as a subdirectory in your own CMake file.
# License
ZPack is licensed under the [BSD 3-Clause license](LICENSE)
