set(ZPACK_TESTS_WORKDIR ${PROJECT_SOURCE_DIR}/tests/workdir)

# open_archive
add_executable(open_archive open_archive.c)
target_include_directories(open_archive PRIVATE ../lib)
target_link_libraries(open_archive zpack)
add_test(
    NAME open_archive
    WORKING_DIRECTORY ${ZPACK_TESTS_WORKDIR}
    COMMAND open_archive
)

# read_archive
add_executable(read_archive read_archive.c)
target_include_directories(read_archive PRIVATE ../lib)
target_link_libraries(read_archive zpack)
add_test(
    NAME read_archive
    WORKING_DIRECTORY ${ZPACK_TESTS_WORKDIR}
    COMMAND read_archive
)

# write_archive
add_executable(write_archive write_archive.c)
target_include_directories(write_archive PRIVATE ../lib)
target_link_libraries(write_archive zpack)
target_compile_definitions(write_archive PRIVATE ${ZPACK_LFS_DEFS})
add_test(
    NAME write_archive
    WORKING_DIRECTORY ${ZPACK_TESTS_WORKDIR}
    COMMAND write_archive
)