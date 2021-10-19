# - Define macro to check large file support
#
#  ZPACK_TEST_LARGE_FILES(VARIABLE)
#
#  VARIABLE will be set to true if off_t is 64 bits, and fseeko/ftello present.
#  This macro will also defines the necessary variable enable large file support, for instance
#  _LARGE_FILES
#  _LARGEFILE64_SOURCE
#  _FILE_OFFSET_BITS 64
#
#  However, it is YOUR job to make sure these defines are set in a #cmakedefine so they
#  end up in a config.h file that is included in your source if necessary!
#
#  Adapted from Gromacs project (http://www.gromacs.org/)
#  by Julien Malik
#

macro(ZPACK_TEST_LARGE_FILES VARIABLE)
    if(NOT DEFINED ${VARIABLE})

        # On most platforms it is probably overkill to first test the flags for 64-bit off_t,
        # and then separately fseeko. However, in the future we might have 128-bit filesystems
        # (ZFS), so it might be dangerous to indiscriminately set e.g. _FILE_OFFSET_BITS=64.

        message(STATUS "Checking for 64-bit off_t")

        # First check without any special flags
        try_compile(FILE64_OK "${PROJECT_BINARY_DIR}"
                    "${PROJECT_SOURCE_DIR}/cmake/TestFileOffsetBits.c")
        if(FILE64_OK)
          message(STATUS "Checking for 64-bit off_t - present")
       	endif()

        if(NOT FILE64_OK)
            # Test with _FILE_OFFSET_BITS=64
            try_compile(FILE64_OK "${PROJECT_BINARY_DIR}"
                        "${PROJECT_SOURCE_DIR}/cmake/TestFileOffsetBits.c"
                        COMPILE_DEFINITIONS "-D_FILE_OFFSET_BITS=64" )
            if(FILE64_OK)
                message(STATUS "Checking for 64-bit off_t - present with _FILE_OFFSET_BITS=64")
                set(_FILE_OFFSET_BITS 64)
            endif()
        endif()

        if(NOT FILE64_OK)
            # Test with _LARGE_FILES
            try_compile(FILE64_OK "${PROJECT_BINARY_DIR}"
                        "${PROJECT_SOURCE_DIR}/cmake/TestFileOffsetBits.c"
                        COMPILE_DEFINITIONS "-D_LARGE_FILES" )
            if(FILE64_OK)
                message(STATUS "Checking for 64-bit off_t - present with _LARGE_FILES")
                set(_LARGE_FILES 1)
            endif()
        endif()
	
        if(NOT FILE64_OK)
            # Test with _LARGEFILE64_SOURCE
            try_compile(FILE64_OK "${PROJECT_BINARY_DIR}"
                        "${PROJECT_SOURCE_DIR}/cmake/TestFileOffsetBits.c"
                        COMPILE_DEFINITIONS "-D_LARGEFILE64_SOURCE" )
            if(FILE64_OK)
                message(STATUS "Checking for 64-bit off_t - present with _LARGEFILE64_SOURCE")
                set(_LARGEFILE64_SOURCE 1)
            endif()
        endif()


        #if(NOT FILE64_OK)
        #    # now check for Windows stuff
        #    try_compile(FILE64_OK "${PROJECT_BINARY_DIR}"
        #                "${PROJECT_SOURCE_DIR}/cmake/TestWindowsFSeek.c")
        #    if(FILE64_OK)
        #        message(STATUS "Checking for 64-bit off_t - present with _fseeki64")
        #        set(HAVE__FSEEKI64 1)
        #    endif()
        #endif()

        if(NOT FILE64_OK)
            message(STATUS "Checking for 64-bit off_t - not present")
        endif()

        set(_FILE_OFFSET_BITS ${_FILE_OFFSET_BITS} CACHE INTERNAL "Result of test for needed _FILE_OFFSET_BITS=64")
        set(_LARGE_FILES      ${_LARGE_FILES}      CACHE INTERNAL "Result of test for needed _LARGE_FILES")
        set(_LARGEFILE64_SOURCE ${_LARGEFILE64_SOURCE} CACHE INTERNAL "Result of test for needed _LARGEFILE64_SOURCE")

        # Set the flags we might have determined to be required above
        configure_file("${PROJECT_SOURCE_DIR}/cmake/TestLargeFiles.c.cmake.in"
                       "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TestLargeFiles.c")

        message(STATUS "Checking for fseeko/ftello")

	    # Test if ftello/fseeko are	available
	    try_compile(FSEEKO_COMPILE_OK
	                "${PROJECT_BINARY_DIR}"
                    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TestLargeFiles.c")
	
	    if(FSEEKO_COMPILE_OK)
            message(STATUS "Checking for fseeko/ftello - present")
        endif()

        if(NOT FSEEKO_COMPILE_OK)
                try_compile(FSEEKO_COMPILE_OK
                            "${PROJECT_BINARY_DIR}"
                            "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/TestLargeFiles.c"
                            COMPILE_DEFINITIONS "-D_LARGEFILE64_SOURCE" )

                if(FSEEKO_COMPILE_OK)
                    message(STATUS "Checking for fseeko/ftello - present with _LARGEFILE64_SOURCE")
                    set(_LARGEFILE64_SOURCE ${_LARGEFILE64_SOURCE} CACHE INTERNAL "Result of test for needed _LARGEFILE64_SOURCE")
                endif()
        endif()

	    if(FILE64_OK AND FSEEKO_COMPILE_OK)
                message(STATUS "Large File support - found")
                set(${VARIABLE} ON CACHE INTERNAL "Result of test for large file support")
        else()
                message(STATUS "Large File support - not found")
                set(${VARIABLE} OFF CACHE INTERNAL "Result of test for large file support")
        endif()

    endif()
endmacro()



