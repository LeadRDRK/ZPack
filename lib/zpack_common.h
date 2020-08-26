#pragma once

// export helpers

#if defined(_WIN32) || defined(__CYGWIN__)
    #define ZPACK_CALL __stdcall
    #define ZPACK_EXPORT __declspec(dllexport)
#else
    #define ZPACK_CALL
    #define ZPACK_EXPORT
#endif

#ifdef zpack_EXPORTS
    #define ZPACK_API ZPACK_EXPORT ZPACK_CALL
#else
    #define ZPACK_API ZPACK_CALL
#endif
