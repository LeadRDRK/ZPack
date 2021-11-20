# add executable with name zpack_cli to avoid conflict with the library
add_executable(zpack_cli
    args.c
    args.h
    commands.c
    commands.h
    platform_defs.h
    utils.c
    utils.h
    zpack_cli.c
)
target_include_directories(zpack_cli PRIVATE "../lib")
target_link_libraries(zpack_cli PRIVATE zpack)
target_compile_definitions(zpack_cli PRIVATE ${ZPACK_LFS_DEFS} ${ZPACK_DISABLE_DEFS})
set_target_properties(zpack_cli PROPERTIES OUTPUT_NAME zpack)