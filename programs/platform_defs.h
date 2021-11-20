#ifndef __CLI_PLATFORM_DEFS_H__
#define __CLI_PLATFORM_DEFS_H__

#include <stdlib.h>

#if (defined(__APPLE__) && defined(__MACH__))
    #define PLATFORM_MACOS
#endif

#if (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#define PLATFORM_UNIX
#include <limits.h>
#elif defined(_WIN32)
#define PLATFORM_WIN32
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif
#else
#error Unsupported platform
#endif

#endif // __CLI_PLATFORM_DEFS_H__