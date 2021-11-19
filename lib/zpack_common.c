#include "zpack_common.h"
#include "zpack.h"
#include <xxhash.h>
#include <stdlib.h>

// Windows specific
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Based on stbi__fopen
FILE* zpack_fopen(const char* filename, const char* mode)
{
    FILE *f;
    wchar_t w_mode[64];
    wchar_t w_filename[1024];
	if (0 == MultiByteToWideChar(65001 /* UTF8 */, 0, filename, -1, w_filename, sizeof(w_filename)/sizeof(*w_filename)))
        return 0;

	if (0 == MultiByteToWideChar(65001 /* UTF8 */, 0, mode, -1, w_mode, sizeof(w_mode)/sizeof(*w_mode)))
        return 0;

#if defined(_MSC_VER) && _MSC_VER >= 1400
	if (0 != _wfopen_s(&f, w_filename, w_mode))
		f = 0;
#else
    f = _wfopen(w_filename, w_mode);
#endif

    return f;
}

int zpack_convert_wchar_to_utf8(char *buffer, size_t len, const wchar_t* input)
{
    return WideCharToMultiByte(65001 /* UTF8 */, 0, input, -1, buffer, (int)len, NULL, NULL);
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

zpack_u64 zpack_get_heap_size(zpack_u64 n)
{
    // get closest power of 2 that can hold n bytes
    zpack_u64 b = 1;
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

int zpack_init_stream(zpack_stream* stream)
{
    if (!stream->xxh3_state)
    {
        if ((stream->xxh3_state = XXH3_createState()) == NULL)
            return ZPACK_ERROR_MALLOC_FAILED;
        
        // init
        if (XXH3_64bits_reset(stream->xxh3_state) == XXH_ERROR)
            return ZPACK_ERROR_HASH_FAILED;
    }
    return ZPACK_OK;
}

void zpack_close_stream(zpack_stream *stream)
{
    XXH3_freeState(stream->xxh3_state);
    stream->xxh3_state = NULL;
}