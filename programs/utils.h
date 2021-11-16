#ifndef __CLI_UTILS_H__
#define __CLI_UTILS_H__

#include <zpack_common.h>
#include <sys/stat.h>

#if defined(_MSC_VER)
    typedef struct __stat64 stat_t;
#elif defined(__MINGW32__) && defined (__MSVCRT__)
    typedef struct _stati64 stat_t;
#else
    typedef struct stat stat_t;
#endif

typedef struct path_filename_s
{
    char* path;
    char* filename;
    zpack_bool path_alloc;

} path_filename;

int utils_find_index_of(const char* str, char c);
char* utils_get_filename(char* path, int depth);
void utils_remove_trailing_separators(char* path);
void utils_convert_separators(char* path);
void utils_convert_separators_archive(char* path);
void utils_process_path(const char* path, char* out);

zpack_bool utils_mkdir(const char* path);
zpack_bool utils_mkdir_p(const char* path, zpack_bool exclude_last);
zpack_bool utils_move(const char* old, const char* new);
int utils_stat(const char* path, stat_t* buf);
zpack_bool utils_is_directory(stat_t* buf);
zpack_bool utils_get_directory_files(path_filename** files, int* file_count, int* list_size, char* dir_path, int depth);
zpack_bool utils_prepare_file_list(char** paths, int path_count, path_filename** files, int* file_count);
void utils_free_file_list(path_filename* files, int file_count);
char* utils_get_full_path(char* full_path, const char* path);
void utils_get_tmp_path(const char* path, char* tmp_path);

path_filename* utils_add_path_filename(path_filename** files, int* file_count, int* list_size);

int utils_get_heap_size(int n);

#endif // __CLI_UTILS_H__