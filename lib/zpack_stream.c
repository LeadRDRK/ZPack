#include "zpack.h"
#include <xxhash.h>

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

void zpack_reset_stream(zpack_stream *stream)
{
    stream->total_in  = 0;
    stream->total_out = 0;
    stream->read_back = 0;
}

void zpack_close_stream(zpack_stream *stream)
{
    XXH3_freeState(stream->xxh3_state);
    stream->xxh3_state = NULL;
}