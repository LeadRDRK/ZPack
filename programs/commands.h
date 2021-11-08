#ifndef __CLI_COMMANDS_H__
#define __CLI_COMMANDS_H__

#include "args.h"

typedef int (*command_handler)(args_options* options);

int command_create(args_options* options);
int command_add(args_options* options);
int command_extract(args_options* options);
int command_extract_full(args_options* options);
int command_list(args_options* options);
int command_delete(args_options* options);
int command_move(args_options* options);
int command_test(args_options* options);

#endif // __CLI_COMMANDS_H__