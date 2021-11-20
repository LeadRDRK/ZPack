#ifndef __CLI_ARGS_H__
#define __CLI_ARGS_H__

#include <zpack.h>

typedef struct args_options_s
{
    char* command;
    zpack_compress_options comp_options;
    char* output;

    char** path_list;
    int path_count;
    int path_list_size;

    char** exclude_list;
    int exclude_count;
    int exclude_list_size;

    zpack_bool unsafe;

    char** argv; // Used on Windows only (to keep the pointer for the utf-8 args)

} args_options;

zpack_bool args_parse(int argc, char** argv, args_options* options);
void args_options_free(args_options* options);

#endif // __CLI_ARGS_H__