#pragma once

// export helpers

#if (defined(_WIN32) || defined(__CYGWIN__)) && zpack_EXPORTS
    #define ZPACK_API __declspec(dllexport)
#else
    #define ZPACK_API
#endif