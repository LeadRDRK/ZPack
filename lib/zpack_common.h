#ifndef __ZPACK_COMMON_H__
#define __ZPACK_COMMON_H__

#include "zpack.h"
#include <stdio.h>

#define ZPACK_READ_LE8(p)  *((const zpack_u8 *)(p))

#if ZPACK_LITTLE_ENDIAN
#define ZPACK_READ_LE16(p) *((const zpack_u16 *)(p))
#define ZPACK_READ_LE32(p) *((const zpack_u32 *)(p))
#else
#define ZPACK_READ_LE16(p) ((zpack_u32)(((const zpack_u8 *)(p))[0]) | ((zpack_u32)(((const zpack_u8 *)(p))[1]) << 8U))
#define ZPACK_READ_LE32(p) ((zpack_u32)(((const zpack_u8 *)(p))[0]) | ((zpack_u32)(((const zpack_u8 *)(p))[1]) << 8U) | ((zpack_u32)(((const zpack_u8 *)(p))[2]) << 16U) | ((zpack_u32)(((const zpack_u8 *)(p))[3]) << 24U))
#endif

#define ZPACK_READ_LE64(p) (((zpack_u64)ZPACK_READ_LE32(p)) | (((zpack_u64)ZPACK_READ_LE32((const zpack_u8 *)(p) + sizeof(zpack_u32))) << 32U))
#define ZPACK_VERIFY_SIGNATURE(p, sig) (ZPACK_READ_LE32(p) == sig)

void zpack_write_le16(zpack_u8 *p, zpack_u16 v);
void zpack_write_le32(zpack_u8 *p, zpack_u32 v);
void zpack_write_le64(zpack_u8 *p, zpack_u64 v);
int zpack_seek_and_write(FILE* fp, size_t offset, const zpack_u8* buffer, size_t size);

#define ZPACK_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define ZPACK_MIN(x, y) (((x) < (y)) ? (x) : (y))

zpack_u64 zpack_get_heap_size(zpack_u64 n);
int zpack_check_and_grow_heap(zpack_u8** buffer, zpack_u64* capacity, zpack_u64 needed);

// Platform specific stuff

// MSVC and Mingw
#if defined(_MSC_VER) || defined(__MINGW32__)
ZPACK_EXPORT FILE* zpack_fopen(const char* filename, const char* mode);
#define ZPACK_FOPEN zpack_fopen
#define ZPACK_FCLOSE fclose
#define ZPACK_FREAD fread
#define ZPACK_FWRITE fwrite
#define ZPACK_FTELL _ftelli64
#define ZPACK_FSEEK _fseeki64
#define ZPACK_FFLUSH fflush

// GNU-compatible compilers with large file support enabled
#elif defined(__GNUC__) && defined(_LARGEFILE64_SOURCE)
#define ZPACK_FOPEN fopen64
#define ZPACK_FCLOSE fclose
#define ZPACK_FREAD fread
#define ZPACK_FWRITE fwrite
#define ZPACK_FTELL ftello64
#define ZPACK_FSEEK fseeko64
#define ZPACK_FFLUSH fflush

// Apple and everything else
#else
// Apple's standard library functions have 64bit support by default
#if !defined(__APPLE__)
#pragma message("Using standard library functions for file I/O. Large files might not be supported.")
#endif

#define ZPACK_FOPEN fopen
#define ZPACK_FCLOSE fclose
#define ZPACK_FREAD fread
#define ZPACK_FWRITE fwrite
#ifdef __STRICT_ANSI__
#define ZPACK_FTELL ftell
#define ZPACK_FSEEK fseek
#else
#define ZPACK_FTELL ftello
#define ZPACK_FSEEK fseeko
#endif
#define ZPACK_FFLUSH fflush

#endif

#endif // __ZPACK_COMMON_H__