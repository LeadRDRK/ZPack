#include "args.h"
#include "utils.h"
#include "platform_defs.h"
#include <zpack_common.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

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
    
#if defined(PLATFORM_WIN32) && !defined(ZPACK_DISABLE_UNICODE)
    LPWSTR* w_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (w_argv == NULL)
    {
        printf("Error: Failed to process command line arguments\n");
        return ZPACK_FALSE;
    }
    // Convert wchar to multibyte UTF-8
    argv = (char**)malloc(sizeof(char*) * argc);
    for (int i = 0; i < argc; ++i)
    {
        // Get the size needed for the UTF-8 conversion
        int needed = zpack_convert_wchar_to_utf8(NULL, 0, w_argv[i]);

        // Alloc memory and convert
        argv[i] = (char*)malloc(sizeof(char) * needed);
        if (zpack_convert_wchar_to_utf8(argv[i], needed, w_argv[i]) == 0)
        {
            printf("Error: Failed to process command line arguments\n");
            return ZPACK_FALSE;
        }
    }
    LocalFree(w_argv);
    options->argv = argv;
#endif

    // default args
    options->comp_options.method = ZPACK_COMPRESSION_ZSTD;
    options->comp_options.level = 3;

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
                    if (strncmp(method_str, "none", 4) == 0)
                        options->comp_options.method = ZPACK_COMPRESSION_NONE;
                    else if (strncmp(method_str, "zstd", 4) == 0)
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
                    else
                    {
                        // default compression levels
                        switch (options->comp_options.method)
                        {
                        case ZPACK_COMPRESSION_NONE:
                        case ZPACK_COMPRESSION_LZ4:
                            options->comp_options.level = 0;
                            break;
                        
                        case ZPACK_COMPRESSION_ZSTD:
                            options->comp_options.level = 3;
                            break;
                            
                        }
                    }
                    break;
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

    if (options->path_count < 1)
    {
        printf("Error: At least 1 file path must be specified\n");
        return ZPACK_FALSE;
    }

    return ZPACK_TRUE;
}

void args_options_free(args_options* options)
{
    free(options->path_list);
    free(options->exclude_list);
#if defined(PLATFORM_WIN32) && !defined(ZPACK_DISABLE_UNICODE)
    free(options->argv);
#endif
}