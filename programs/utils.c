#include "utils.h"
#include "platform_defs.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#ifdef PLATFORM_UNIX
#define PATH_SEPARATOR '/'
#include <dirent.h>
#elif defined(PLATFORM_WIN32)
#define PATH_SEPARATOR '\\'
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#endif

int utils_find_index_of(const char* str, char c)
{
    for (int i = 0; str[i]; ++i)
    {
        if (str[i] == c)
            return i;
    }
    return -1;
}

void utils_remove_trailing_separators(char* path)
{
    size_t len = strlen(path);
    if (!len) return;

    char* p = path + len - 1;
    while (p != path)
    {
        if (*p != '/' && *p != '\\')
        {
            p[1] = '\0'; // terminate string at character
            break;
        }
        --p;
    }
}

char* utils_get_filename(char* path, int depth)
{
    char* p = path + strlen(path);
    zpack_bool got_dir = ZPACK_FALSE;
    while (p != path)
    {
        if (*p == '/' || *p == '\\')
        {
            if (got_dir)
            {
                if (depth == 0)
                    return p + 1;
                
                --depth;
                got_dir = ZPACK_FALSE;
            }
        }
        else got_dir = ZPACK_TRUE;
        --p;
    }
    return p;
}

zpack_bool utils_mkdir(const char* path)
{
    int ret;
#ifdef PLATFORM_UNIX
    ret = mkdir(path, 0755);
#elif defined(PLATFORM_WIN32)
    ret = _mkdir(path);
#endif

    if (ret)
    {
        if (errno != EEXIST)
            return ZPACK_FALSE;
    }

    return ZPACK_TRUE;
}

zpack_bool utils_mkdir_p(const char* p, zpack_bool exclude_last)
{
    char* tmp = (char*)malloc(sizeof(char) * (strlen(p) + 1));
    int pos = 0;
    zpack_bool got_sep = ZPACK_FALSE;

    while (*p)
    {
        if (*p == PATH_SEPARATOR)
        {
            if (!got_sep)
            {
                got_sep = ZPACK_TRUE;
                tmp[pos++] = PATH_SEPARATOR;
            }
            ++p;
            continue;
        }
        else if (got_sep)
        {
            got_sep = ZPACK_FALSE;
            // create dir
            tmp[pos] = '\0';
            if (!utils_mkdir(tmp))
			{
				free(tmp);
                return ZPACK_FALSE;
			}
        }

        tmp[pos++] = *p;
        ++p;
    }

    if (!exclude_last)
    {
        tmp[pos] = '\0';
        if (!utils_mkdir(tmp))
		{
			free(tmp);
            return ZPACK_FALSE;
		}
    }
    
	free(tmp);
    return ZPACK_TRUE;
}

zpack_bool utils_move(const char* old, const char* new)
{
#ifdef PLATFORM_WIN32
    return MoveFileExA(old, new, MOVEFILE_REPLACE_EXISTING);
#else
    // stdlib method
    if (remove(new))
    {
        if (errno != ENOENT)
            return ZPACK_FALSE;
    }
    if (rename(old, new))
        return ZPACK_FALSE;

    return ZPACK_TRUE;
#endif
}

int utils_stat(const char* path, stat_t* buf)
{
#if defined(_MSC_VER)
    return _stat64(path, buf);
#elif defined(__MINGW32__) && defined (__MSVCRT__)
    return _stati64(path, buf);
#else
    return stat(path, buf);
#endif
}

zpack_bool utils_is_directory(stat_t* buf)
{
#if defined(_MSC_VER)
    return (buf->st_mode & _S_IFMT) == _S_IFDIR;
#else
    return S_ISDIR(buf->st_mode);
#endif
}

#ifdef PLATFORM_WIN32
zpack_bool utils_get_directory_files_i(path_filename** files, int* file_count, int* list_size, wchar_t* dir_path, int depth)
{
    size_t dir_length = wcslen(dir_path);
    // construct find pattern (path\*)
    wchar_t* find_path = (wchar_t*)malloc(sizeof(wchar_t) * (dir_length + 3));
    memcpy(find_path, dir_path, sizeof(wchar_t) * dir_length);
    find_path[dir_length] = L'\\';
    find_path[dir_length + 1] = L'*';
    find_path[dir_length + 2] = L'\0';

    WIN32_FIND_DATAW entry;
    HANDLE h_find;

    h_find = FindFirstFileW(find_path, &entry);
    if (h_find == INVALID_HANDLE_VALUE)
    {
        printf("Error: Failed to open directory \"%ls\"\n", dir_path);
		free(find_path);
        return ZPACK_FALSE;
    }

    do
    {
        if (wcscmp(entry.cFileName, L"..") == 0 || wcscmp(entry.cFileName, L".") == 0)
            continue;

        size_t fn_length = wcslen(entry.cFileName);
        wchar_t* path = (wchar_t*)malloc(sizeof(wchar_t) * (dir_length + fn_length + 2));

        // concatenate path + separator + filename
        memcpy(path, dir_path, sizeof(wchar_t) * dir_length);
        path[dir_length] = PATH_SEPARATOR;
        memcpy(path + dir_length + 1, entry.cFileName, sizeof(wchar_t) * (fn_length + 1));

        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            utils_get_directory_files_i(files, file_count, list_size, path, depth + 1);
            free(path);
        }
        else
        {
            path_filename* entry;
            if ((entry = utils_add_path_filename(files, file_count, list_size)) == NULL)
            {
                FindClose(h_find);
				free(find_path);
                free(path);
                return ZPACK_FALSE;
            }
            
            char* mb_path = (char*)malloc(sizeof(char) * (PATH_MAX+1));
            if (zpack_convert_wchar_to_utf8(mb_path, PATH_MAX+1, path) == 0)
            {
                printf("Error: Failed to process paths\n");
                FindClose(h_find);
				free(find_path);
                free(mb_path);
                free(path);
                return ZPACK_FALSE;
            }

            entry->path = mb_path;
            entry->filename = utils_get_filename(mb_path, depth + 1);
            utils_convert_separators_archive(entry->filename);
            entry->path_alloc = ZPACK_TRUE;
        }
    }
    while (FindNextFileW(h_find, &entry));

    FindClose(h_find);
	free(find_path);
    return ZPACK_TRUE;
}

zpack_bool utils_get_directory_files(path_filename** files, int* file_count, int* list_size, char* dir_path, int depth)
{
    wchar_t w_dir_path[PATH_MAX+1];
    if (MultiByteToWideChar(65001 /* UTF8 */, 0, dir_path, -1, w_dir_path, PATH_MAX+1) == 0)
    {
        printf("Error: Failed to process path \"%s\"\n", dir_path);
        return ZPACK_FALSE;
    }
    return utils_get_directory_files_i(files, file_count, list_size, w_dir_path, depth);
}
#elif defined(__linux__) || (PLATFORM_POSIX_VERSION >= 200112L)
zpack_bool utils_get_directory_files(path_filename** files, int* file_count, int* list_size, char* dir_path, int depth)
{
    size_t dir_length = strlen(dir_path);
    DIR* dir;
    if (!(dir = opendir(dir_path)))
    {
        printf("Error: Failed to open directory \"%s\"", dir_path);
		utils_print_strerror();
        return ZPACK_FALSE;
    }

    errno = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)))
    {
        if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0)
            continue;
        
        size_t fn_length = strlen(entry->d_name);
        char* path = (char*)malloc(sizeof(char) * (dir_length + fn_length + 2));

        // concatenate path + separator + filename
        memcpy(path, dir_path, dir_length);
        path[dir_length] = PATH_SEPARATOR;
        memcpy(path + dir_length + 1, entry->d_name, fn_length + 1);

        stat_t sb;
        if (utils_stat(path, &sb))
        {
            printf("Error: Failed to stat \"%s\"", path);
			utils_print_strerror();
            closedir(dir);
            return ZPACK_FALSE;
        }

        if (utils_is_directory(&sb))
        {
            utils_get_directory_files(files, file_count, list_size, path, depth + 1);
            free(path);
        }
        else
        {
            path_filename* entry;
            if ((entry = utils_add_path_filename(files, file_count, list_size)) == NULL)
            {
                closedir(dir);
                return ZPACK_FALSE;
            }
            
            entry->path = path;
            entry->filename = utils_get_filename(path, depth + 1);
            entry->path_alloc = ZPACK_TRUE;
        }
    }
    closedir(dir);

    if (errno)
    {
        printf("Error: Failed to read directory \"%s\"", dir_path);
		utils_print_strerror();
        return ZPACK_FALSE;
    }

    return ZPACK_TRUE;
}
#else
zpack_bool utils_get_directory_files(path_filename** files, int* file_count, int* list_size, char* dir_path, int depth)
{
    printf("Warning: Ignoring directory \"%s\" (compiled without directory support)\n", path);
    return ZPACK_TRUE;
}
#endif

zpack_bool utils_prepare_file_list(char** paths, int path_count, path_filename** files, int* file_count)
{
    int list_size = 0;
    for (int i = 0; i < path_count; ++i)
    {
        utils_remove_trailing_separators(paths[i]);
        stat_t sb;
        if (utils_stat(paths[i], &sb))
        {
            printf("Error: Failed to stat \"%s\"", paths[i]);
			utils_print_strerror();
            return ZPACK_FALSE;
        }

        if (utils_is_directory(&sb))
        {
            if (!utils_get_directory_files(files, file_count, &list_size, paths[i], 0))
                return ZPACK_FALSE;
        }
        else
        {
            path_filename* entry;
            if ((entry = utils_add_path_filename(files, file_count, &list_size)) == NULL)
                return ZPACK_FALSE;

            entry->path = paths[i];
            entry->filename = utils_get_filename(paths[i], 0);
            utils_convert_separators_archive(entry->filename);
            entry->path_alloc = ZPACK_FALSE;
        }
    }
    return ZPACK_TRUE;
}

void utils_free_file_list(path_filename* files, int file_count)
{
    for (int i = 0; i < file_count; ++i)
    {
        if (files[i].path_alloc)
            free(files[i].path);
    }

    free(files);
}

char* utils_get_full_path(char* full_path, const char* path)
{
#ifdef PLATFORM_UNIX
    return realpath(path, full_path);
#elif defined(PLATFORM_WIN32)
    return _fullpath(full_path, path, PATH_MAX+1);
#endif
}

void utils_get_tmp_path(const char* path, char* tmp_path)
{
    srand((unsigned int)time(NULL));

    size_t path_len = strlen(path);
    memcpy(tmp_path, path, path_len);
    tmp_path[path_len] = '.';

    for (;;)
    {
        // Generate 5 random chars from 0-9, a-z
        for (int i = 1; i < 6; ++i)
        {
            int n = rand() % 36;
            tmp_path[path_len + i] = ((n > 9) ? ('a' + n - 10) : ('0' + n));
        }
        tmp_path[path_len + 6] = '\0';

        // Check if path does not exist
        stat_t sb;
        if (utils_stat(tmp_path, &sb))
            break;
    }
}

void utils_convert_separators(char* p)
{
#ifdef PLATFORM_WIN32
    while (*p)
    {
        if (*p == '/')
            *p = '\\';
        ++p;
    }
#endif
}

void utils_convert_separators_archive(char* p)
{
#ifdef PLATFORM_WIN32
    while (*p)
    {
        if (*p == '\\')
            *p = '/';
        ++p;
    }
#endif
}

void utils_process_path(const char* path, char* out)
{
    zpack_bool got_first_dir = ZPACK_FALSE,
               got_sep = ZPACK_FALSE,
               got_dot = ZPACK_FALSE;
    int pos = 0;
    for (int i = 0; path[i]; ++i)
    {
        // reset
        if (path[i] != '/' && got_sep)
            got_sep = ZPACK_FALSE;

        if (path[i] != '.' && got_dot)
            got_dot = ZPACK_FALSE;

        switch (path[i])
        {
#ifdef PLATFORM_WIN32
        case ':':
            // Ignore drive specifier (convert to separator)
            // e.g: C:\folder, C:folder (can be used without initial separator)
            if (i == 1)
            {
                out[pos++] = '/';
                got_sep = ZPACK_TRUE;
            }
            break;

        case '\\':
            // Convert \ to _ (to retain consistency)
            out[pos++] = '_';
            break;
#endif
        case '/':
            // Check for separators
            // Note: checking for / here only since the format specifies it as the only
            // separator allowed. \ is a valid filename character on Linux/macOS.
            if (!got_sep)
            {
                // Prevent accessing directories from root
                if (!got_first_dir) continue;

                out[pos++] = '/';
                got_sep = ZPACK_TRUE;
            }
            break;
        
        case '.':
            // Prevent path traversal
            if (!got_dot)
            {
                out[pos++] = '.';
                got_dot = ZPACK_TRUE;
            }
            break;

        default:
            // Others
            if (!got_first_dir) got_first_dir = ZPACK_TRUE;
            out[pos++] = path[i];
            break;

        }
    }

    // Terminate string
    out[pos] = '\0';
}

path_filename* utils_add_path_filename(path_filename** files, int* file_count, int* list_size)
{
    int num = (*file_count)++;
    if (*list_size < *file_count)
    {
        *list_size = utils_get_heap_size(*file_count);
        *files = (path_filename*)realloc(*files, sizeof(path_filename) * (*list_size));
        if (*files == NULL)
        {
            printf("Error: Failed to allocate memory\n");
            return NULL;
        }
    }
    return *files + num;
}

int utils_get_heap_size(int n)
{
    // get closest power of 2 that can hold n bytes
    int b = 1;
    while (b < n)
        b = b << 1;
    return b;
}

void utils_print_strerror()
{
#ifdef PLATFORM_WIN32
	char buffer[255];
	strerror_s(buffer, 255, errno);
	printf("(%s)\n", buffer);
#else
	printf("(%s)\n", strerror(errno));
#endif
}