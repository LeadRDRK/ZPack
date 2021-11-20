#include <zpack.h>
#include <stdlib.h>
#include <string.h>
#include "archive.h"

#ifdef _WIN32
#define PRId64 "lld"
#else
#include <inttypes.h>
#endif

#define MIN(a, b) ((a < b) ? a : b)

static const char* _out_names[2] = {
    "out_zstd.zpk",
    "out_lz4.zpk"
};

static const char* _out_names_s[2] = {
    "out_zstd_streaming.zpk",
    "out_lz4_streaming.zpk"
};

#define WRITE_ERROR(writer, ret, func_name) \
    printf("-- Got error %d from " func_name ", last_return %" PRId64 "\n", ret, (writer)->last_return); \
    zpack_close_writer(writer); \
    return ZPACK_FALSE;

zpack_bool write_archive_oneshot(zpack_writer* writer, zpack_file* files, zpack_compression_method method)
{
    int ret;
    if ((ret = zpack_write_archive(writer, files, 2)))
    {
        WRITE_ERROR(writer, ret, "zpack_write_archive");
    }

    zpack_close_writer(writer);
    printf("-- Archive write successful\n");
    return ZPACK_TRUE;
}

#define STREAM_IN_SIZE 16
zpack_bool write_archive_streaming(zpack_writer* writer, zpack_file* files, zpack_compression_method method)
{
    int ret;
    if ((ret = zpack_write_header(writer)))
    {
        WRITE_ERROR(writer, ret, "zpack_write_header");
    }

    if ((ret = zpack_write_data_header(writer)))
    {
        WRITE_ERROR(writer, ret, "zpack_write_data_header");
    }

    zpack_stream stream;
    memset(&stream, 0, sizeof(stream));
    if ((ret = zpack_init_stream(&stream)))
    {
        WRITE_ERROR(writer, ret, "zpack_init_stream");
    }
    zpack_u32 stream_out_size = zpack_get_cstream_out_size(ZPACK_COMPRESSION_ZSTD);
    zpack_u8 out_buf[stream_out_size];
    stream.next_out = out_buf;
    stream.avail_out = stream_out_size;

    for (int i = 0; i < FILE_COUNT; ++i)
    {
        // reset
        stream.next_in = files[i].buffer;
        stream.total_in = 0;
        stream.total_out = 0;

        while (stream.total_in < files[i].size)
        {
            stream.avail_in = MIN(STREAM_IN_SIZE, files[i].size - stream.total_in);

            if ((ret = zpack_write_file_stream(writer, files[i].options, &stream, NULL)))
            {
                zpack_close_stream(&stream);
                WRITE_ERROR(writer, ret, "zpack_write_file_stream");
            }
        }
        
        if ((ret = zpack_write_file_stream_end(writer, files[i].filename, files[i].options, &stream, NULL)))
        {
            zpack_close_stream(&stream);
            WRITE_ERROR(writer, ret, "zpack_write_file_stream_end");
        }
    }
    zpack_close_stream(&stream);

    if ((ret = zpack_write_cdr(writer)))
    {
        WRITE_ERROR(writer, ret, "zpack_write_cdr");
    }

    if ((ret = zpack_write_eocdr(writer)))
    {
        WRITE_ERROR(writer, ret, "zpack_write_eocdr");
    }

    zpack_close_writer(writer);
    printf("-- Archive write successful\n");
    return ZPACK_TRUE;
}

zpack_bool write_archives(zpack_compression_method method)
{
    FILE* fp;
    zpack_compress_options options;
    options.method = method;
    switch (method)
    {
    case ZPACK_COMPRESSION_ZSTD:
        options.level = 3;
        break;
    
    case ZPACK_COMPRESSION_LZ4:
        options.level = 1;
        break;

    }
    printf("Compression method %d\n", method);

    zpack_file files[FILE_COUNT];
    for (int i = 0; i < FILE_COUNT; ++i)
    {
        files[i].filename = _filenames[i];
        files[i].buffer = (zpack_u8*)_files[i];
        files[i].size = _uncomp_sizes[i];
        files[i].options = &options;
        files[i].cctx = NULL;
    }

    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    /**** Write to file ****/
    int ret;
    printf("Write to file\n");

    printf("* Oneshot\n");
    if ((ret = zpack_init_writer(&writer, _out_names[method])))
    {
        WRITE_ERROR(&writer, ret, "zpack_init_writer");
    }

    if (!write_archive_oneshot(&writer, files, method))
        return ZPACK_FALSE;
    
    printf("* Streaming\n");
    if ((ret = zpack_init_writer(&writer, _out_names_s[method])))
    {
        WRITE_ERROR(&writer, ret, "zpack_init_writer");
    }

    if (!write_archive_streaming(&writer, files, method))
        return ZPACK_FALSE;

    /**** Write to buffer ****/
    printf("Write to buffer\n");

    printf("* Oneshot\n");
    if ((ret = zpack_init_writer_heap(&writer, 0)))
    {
        WRITE_ERROR(&writer, ret, "zpack_init_writer_heap");
    }

    if (!write_archive_oneshot(&writer, files, method))
        return ZPACK_FALSE;
    
    printf("* Streaming\n");
    if ((ret = zpack_init_writer_heap(&writer, 0)))
    {
        WRITE_ERROR(&writer, ret, "zpack_init_writer_heap");
    }

    if (!write_archive_streaming(&writer, files, method))
        return ZPACK_FALSE;
    
    printf("\n");
    return ZPACK_TRUE;
}

int main()
{
    for (int i = 0; i < ARCHIVE_COUNT; ++i)
    {
        if (!write_archives(i))
            return 1;
    }
    
    return 0;
}