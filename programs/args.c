#include "args.h"
#include "utils.h"
#include <zpack_common.h>
#include <stdlib.h>
#include <string.h>

static zpack_bool args_list_insert(char*** list, int* count, int* size, char* arg)
{
    int num = (*count)++;
    if (*size < *count)
    {
        *size = utils_get_heap_size(*count);
        *list = (char**)realloc(*list, sizeof(char*) * (*size));
        if (*list == NULL)
        {
            printf("Error: Failed to allocate memory\n");
            return ZPACK_FALSE;
        }
    }
    (*list)[num] = arg;
    return ZPACK_TRUE;
}

zpack_bool args_parse(int argc, char** argv, args_options* options)
{
    if (argc < 2)
        return ZPACK_FALSE;
    
    for (int i = 1; i < argc; ++i)
    {
        char* arg = argv[i];
        if (arg[0] == '-')
        {
            size_t len = strlen(arg);

            if (len == 2)
            {
                switch (arg[1])
                {
                case 'm':
                {
                    char* method_str = argv[++i];
                    if (strncmp(method_str, "zstd", 4) == 0)
                        options->comp_options.method = ZPACK_COMPRESSION_ZSTD;
                    else if (strncmp(method_str, "lz4", 3) == 0)
                        options->comp_options.method = ZPACK_COMPRESSION_LZ4;
                    else
                    {
                        printf("Invalid compression method: %s\n", method_str);
                        return ZPACK_FALSE;
                    }

                    int sep_pos = utils_find_index_of(method_str, ':');
                    if (sep_pos != -1)
                    {
                        char* level_str = method_str + sep_pos + 1;
                        char* end;

                        options->comp_options.level = strtol(level_str, &end, 10);
                        if (end == level_str || *end != '\0')
                        {
                            printf("Invalid compression level: %s\n", level_str);
                            return ZPACK_FALSE;
                        }
                    }
                }

                case 'o':
                    if (options->output)
                        printf("Warning: Ignoring previous output \"%s\"\n", options->output);
                    options->output = argv[++i];
                    break;

                case 'x':
                    if (!args_list_insert(&options->exclude_list, &options->exclude_count,
                                          &options->exclude_list_size, argv[++i]))
                        return ZPACK_FALSE;
                    break;

                case 'h':
                    // immediately return to print the help message
                    return ZPACK_FALSE;

                }
            }
            else if (len > 2) // long switches
            {
                char* name = arg + 2;
                if (strcmp(name, "unsafe") == 0)
                    options->unsafe = ZPACK_TRUE;
                else if (strcmp(name, "help") == 0)
                    return ZPACK_FALSE;
                else
                {
                    printf("Invalid switch: %s\n", arg);
                    return ZPACK_FALSE;
                }
            }
            else
            {
                printf("Invalid switch: %s\n", arg);
                return ZPACK_FALSE;
            }
        }
        else
        {
            if (!options->command)
                options->command = arg;
            else
                if (!args_list_insert(&options->path_list, &options->path_count,
                                      &options->path_list_size, arg))
                    return ZPACK_FALSE;
        }
    }

    return ZPACK_TRUE;
}

void args_options_free(args_options* options)
{
    free(options->path_list);
    free(options->exclude_list);
}