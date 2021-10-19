# - Find xxHash library
# This will define
# xxHash_FOUND
# xxHash_INCLUDE_DIRS
# xxHash_LIBRARIES
#

find_path(xxHash_INCLUDE_DIRS NAMES xxhash.h)
find_library(xxHash_LIBRARIES NAMES xxhash xxHash)

mark_as_advanced(xxHash_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    xxHash DEFAULT_MSG
    xxHash_LIBRARIES xxHash_INCLUDE_DIRS
)

if (xxHash_FOUND)
    message(STATUS "Found xxHash: ${xxHash_LIBRARIES}")
endif()

mark_as_advanced(xxHash_INCLUDE_DIRS xxHash_LIBRARIES)