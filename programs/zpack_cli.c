#include <stdio.h>
#include <string.h>
#include "args.h"
#include "commands.h"
#include "utils.h"

#include <zpack.h>
#include <zpack_common.h>

#define PROGRAM_NAME "ZPack command line interface"
#define PROGRAM_VERSION ZPACK_VERSION_STRING
#define PROGRAM_AUTHOR "LeadRDRK"

void print_help()
{
    printf(PROGRAM_NAME " v" PROGRAM_VERSION " by " PROGRAM_AUTHOR "\n"
           "Usage: zpack <command> [<switches>...] <archive> [<files>...]\n"
           "\n"
           "Commands\n"
           "    c: create archive\n"
           "    a: add files to archive\n"
           "    e: extract files from archive (without directories)\n"
           "    x: extract files with full paths\n"
           "    l: list files in archive\n"
           "    d: delete files from archive\n"
           "    m: move files in archive\n"
           "    t: test integrity of files in archive\n"
           "\n"
           "Switches\n"
           "    -m <param>: set compression method\n"
           "      Param follows the format method:level. Default: zstd:3\n"
           "      If level is not specified, default value for that method will be used.\n"
           "    -o <directory>: set output directory\n"
           "    -x <file>: exclude file from extraction\n"
           "    -h, --help: show this help message\n"
           "    --unsafe: allow files to be extracted outside of destination\n"
           "      This option should not be used unless you know what you're doing.\n"
           "\n"
    );
}

int main(int argc, char** argv)
{
    args_options options;
    memset(&options, 0, sizeof(options));
    if (!args_parse(argc, argv, &options))
    {
        print_help();
        args_options_free(&options);
        return 1;
    }

    size_t command_len = strlen(options.command);
    int ret;
    if (command_len > 1)
    {
        printf("Invalid command: %s\n", options.command);
        print_help();
        args_options_free(&options);
        return 1;
    }
    else
    {
        command_handler handler;
        switch (options.command[0])
        {
        case 'c': handler = command_create;       break;
        case 'a': handler = command_add;          break;
        case 'e': handler = command_extract;      break;
        case 'x': handler = command_extract_full; break;
        case 'l': handler = command_list;         break;
        case 'd': handler = command_delete;       break;
        case 'm': handler = command_move;         break;
        case 't': handler = command_test;         break;
        }
        ret = handler(&options);
    }

    args_options_free(&options);
    return ret;
}