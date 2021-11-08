#include "utils.h"
#include "platform_defs.h"
#include <errno.h>
#include <string.h>

#ifdef PLATFORM_UNIX
#include <sys/stat.h>
#elif defined(PLATFORM_WIN32)
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

#ifdef PLATFORM_UNIX
#define PATH_SEPARATOR '/'
#elif defined(PLATFORM_WIN32)
#define PATH_SEPARATOR '\\'
#endif

zpack_bool utils_mkdir_p(const char* p, zpack_bool exclude_last)
{
    char tmp[strlen(p) + 1];
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
            if (!utils_mkdir(tmp))
                return ZPACK_FALSE;
        }

        tmp[pos++] = *p;
        ++p;
    }

    if (!exclude_last)
        utils_mkdir(tmp);
    
    return ZPACK_TRUE;
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
            // Convert \ to _
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

int utils_get_heap_size(int n)
{
    // get closest power of 2 that can hold n bytes
    int b = 1;
    while (b < n)
        b = b << 1;
    return b;
}