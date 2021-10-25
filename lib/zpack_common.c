#include "zpack_common.h"
#include <stdlib.h>

#if defined(_MSC_VER) || defined(__MINGW32__)

FILE* zpack_fopen(const char* filename, const char* mode)
{
    FILE *fp = NULL;
    fopen_s(&fp, filename, mode);
    return fp;
}

#endif

void zpack_write_le16(zpack_u8 *p, zpack_u16 v)
{
    p[0] = (zpack_u8)v;
    p[1] = (zpack_u8)(v >> 8);
}

void zpack_write_le32(zpack_u8 *p, zpack_u32 v)
{
    p[0] = (zpack_u8)v;
    p[1] = (zpack_u8)(v >> 8);
    p[2] = (zpack_u8)(v >> 16);
    p[3] = (zpack_u8)(v >> 24);
}

void zpack_write_le64(zpack_u8 *p, zpack_u64 v)
{
    zpack_write_le32(p, (zpack_u32)v);
    zpack_write_le32(p + sizeof(zpack_u32), (zpack_u32)(v >> 32));
}

int zpack_seek_and_write(FILE* fp, size_t offset, const zpack_u8* buffer, size_t size)
{
    if (ZPACK_FSEEK(fp, offset, SEEK_SET) != 0) 
        return ZPACK_ERROR_SEEK_FAILED;
    
    if (ZPACK_FWRITE(buffer, 1, size, fp) != size)
        return ZPACK_ERROR_WRITE_FAILED;
    
    return ZPACK_OK;
}

int zpack_get_heap_size(int n)
{
    // get closest power of 2 that can hold n bytes
    int b = 1;
    while (b < n)
        b = b << 1;
    return b;
}

int zpack_check_and_grow_heap(zpack_u8** buffer, zpack_u64* capacity, zpack_u64 needed)
{
    if (*capacity < needed)
    {
        *capacity = zpack_get_heap_size(needed);
        *buffer = (zpack_u8*)realloc(*buffer, sizeof(zpack_u8) * (*capacity));
        if (*buffer == NULL) return ZPACK_ERROR_MALLOC_FAILED;
    }
    return ZPACK_OK;
}

#ifndef ZPACK_DISABLE_ZSTD
int zpack_get_zstd_result(size_t code)
{
    code = ZSTD_getErrorCode(code);
    switch (code)
    {
    case ZSTD_error_no_error:
        return ZPACK_OK;

    case ZSTD_error_memory_allocation:
        return ZPACK_ERROR_MALLOC_FAILED;
    
    case ZSTD_error_dstSize_tooSmall:
        return ZPACK_ERROR_BUFFER_TOO_SMALL;
    
    default:
        return -1;
    }
}
#endif