#ifndef __CLI_PLATFORM_DEFS_H__
#define __CLI_PLATFORM_DEFS_H__

#if (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#define PLATFORM_UNIX
#elif defined(_WIN32)
#define PLATFORM_WIN32
#else
#error Unsupported platform
#endif

#endif // __CLI_PLATFORM_DEFS_H__