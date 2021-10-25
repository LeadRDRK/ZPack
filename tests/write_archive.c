#include <zpack.h>
#include <zpack_common.h>
#include <stdlib.h>
#include <string.h>
#include "archive.h"

static const char* _out_names[2] = {
    "out_zstd.zpk",
    "out_lz4.zpk"
};

int write_archive(zpack_compression_method method)
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

    zpack_file files[2];
    for (int i = 0; i < 2; ++i)
    {
        files[i].filename = _filenames[i];
        files[i].buffer = (unsigned char*)_files[i];
        files[i].size = _uncomp_sizes[i];
        files[i].options = &options;
    }

    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    /**** Write to file ****/
    int ret;
    printf("Write to file\n");
    if ((ret = zpack_init_writer(&writer, _out_names[method])))
    {
        printf("-- Got error %d from zpack_init_writer\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    if ((ret = zpack_write_archive(&writer, files, 2)))
    {
        printf("-- Got error %d from zpack_write_archive\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    zpack_close_writer(&writer);

    // Verify archive
    zpack_u8 buffer[_archive_sizes[method]];
    fp = ZPACK_FOPEN(_out_names[method], "rb");
    if (!ZPACK_FREAD(buffer, _archive_sizes[method], 1, fp))
    {
        printf("-- Failed to read written archive\n");
        zpack_close_writer(&writer);
        return 1;
    }
    ZPACK_FCLOSE(fp);
    if (memcmp(_archive_buffers[method], buffer, _archive_sizes[method]) != 0)
    {
        printf("-- Written archive is invalid\n");
        zpack_close_writer(&writer);
        return 1;
    }

    zpack_close_writer(&writer);
    printf("-- Archive write to %s successful\n", _out_names[method]);

    /**** Write to buffer ****/
    printf("Write to buffer\n");
    if ((ret = zpack_init_writer_heap(&writer, 0)))
    {
        printf("Got error %d from zpack_init_writer\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    if ((ret = zpack_write_archive(&writer, files, 2)))
    {
        printf("Got error %d from zpack_init_writer\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    // Verify archive
    /*
    if (memcmp(_archive_buffer, writer.buffer, writer.file_size) != 0)
    {
        printf("-- Written archive is invalid\n");
        zpack_close_writer(&writer);
        return 1;
    }
    */

    printf("-- Archive write to buffer successful\n\n");
    zpack_close_writer(&writer);
    
    return 0;
}

int main()
{
    int ret;
    if ((ret = write_archive(ZPACK_COMPRESSION_ZSTD)))
        return ret;

    if ((ret = write_archive(ZPACK_COMPRESSION_LZ4)))
        return ret;

    return 0;
}