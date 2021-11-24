ZPack Unit Tests
================================
These tests are only used to check the basic functionality of the library with a small set of 
files and archives.
- `open_archive`: Open the archives and verify the file entries's fields.
- `read_archive`: Read the archives and verify files.
- `write_archive`: Write archives containing the test files.

The intended working directory for these tests is in `workdir`. Output files will be prefixed with 
`out_` (which are already in .gitignore)