Contributing to ZPack
================================
Branches
-------------------------
The `next` branch is where most of the development is done. New features are added to this branch.

The `main` branch is where the current patch version lives. It contains bugfixes and the likes for 
the current minor version.

Depending on your contribution, you must create a new branch, then make a pull request to either the
`next` branch or the `main` branch. Do not directly make modifications on the actual branches you want
to contribute to.

Your branch names should be concise and descriptive. Avoid using generic branch names such as 
"patch-1" or "bugfix"

Coding style
-------------------------
- Indentation: Allman/BSD style. Use 4 spaces instead of tabs.
- C standard: strict C90, no extensions.
- Naming:
    - All symbols must be prefixed with `zpack_`
    - Casing convention: `snake_case`
    - For macros, write in all caps. Macros defined in headers must be prefixed with `ZPACK_`
    - Function names must be concise and descriptive. The format `zpack_<action>_<object>_<variant>` 
    should be used if applicable.
    Examples: zpack_read_archive, zpack_reset_reader, zpack_read_cdr_memory, etc.
- General:
    - If a line/statement is too long, you may break it into multiple lines.

Documentation
-------------------------
The API documentation is generated using Doxygen. All contents are written as comments in zpack.h,
using the Javadoc style.

You are required to write documentation when updating or adding new features to the API.

Pull requests
-------------------------
Before submitting a pull request, please make sure that:
1. All tests have passed.
2. You've added sufficient documentation.
3. Your code lints.

When submitting a pull request, the title needs to be as descriptive as possible. If needed, you may
add more related information in the description.