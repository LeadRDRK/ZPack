#ifndef __CLI_UTILS_H__
#define __CLI_UTILS_H__

#include <zpack_common.h>

#if defined(_MSC_VER)
    typedef struct __stat64 stat_t;
#elif defined(__MINGW32__) && defined (__MSVCRT__)
    typedef struct _stati64 stat_t;
#else
    typedef struct stat stat_t;
#endif

int utils_find_index_of(const char* str, char c);

zpack_bool utils_mkdir(const char* path);
zpack_bool utils_mkdir_p(const char* path, zpack_bool exclude_last);
zpack_bool utils_stat(const char* path, stat_t* buf);
zpack_bool utils_is_directory(stat_t* buf);

void utils_convert_separators(char* path);
void utils_process_path(const char* path, char* out);

int utils_get_heap_size(int n);

#endif // __CLI_UTILS_H__