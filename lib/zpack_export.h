#ifndef __ZPACK_EXPORT_H__
#define __ZPACK_EXPORT_H__

#ifdef zpack_EXPORTS
    #if defined(_WIN32) || defined(__CYGWIN__) || defined(__ORBIS__)
        #define ZPACK_EXPORT __declspec(dllexport)
    #else
        #define ZPACK_EXPORT
    #endif
#else
    #define ZPACK_EXPORT
#endif

#endif // __ZPACK_EXPORT_H__