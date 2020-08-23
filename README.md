# ZPack
ZPack is a simple, general-purpose archive file format with lossless data compression. It is based on the ZIP/ZIP64 format by PKWARE.

Designed to be extremely fast, it exclusively uses the Zstandard compression algorithm for file compression (hence the name)

More about Zstandard/zstd at https://facebook.github.io/zstd
# Status
ZPack is still a relatively new format. Its implementation is also only recently developed, therefore the API is not perfect yet but it's already fully functional and can be used in your own projects.
# Documentation
The specifications for the ZPack file format can be found [here](docs/specs.txt)

As of now, there are no official documentation of the API yet, but you can always go header spelunking.
# Development
You are welcome to make any contributions in this project. Code that you wrote must be portable and compatible with C++11.
## Building
You can build both the library and the CLI using the CMake file in the parent directory.

To include the library in your own project, add the /lib directory as a subdirectory in your own CMake file.
# License
ZPack is licensed under the [BSD 3-Clause license](LICENSE)