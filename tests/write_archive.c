#include <zpack.h>
#include <zpack_common.h>
#include <stdlib.h>
#include <string.h>
#include "archive.h"

#define OUT_FILE "out.zpk"

int main()
{
    FILE* fp;
    zpack_file files[2];
    zpack_compress_options options;
    options.method = ZPACK_COMPRESSION_ZSTD;
    options.level = 3;
    for (int i = 0; i < 2; ++i)
    {
        files[i].filename = _filenames[i];

        fp = ZPACK_FOPEN(_filenames[i], "rb");
        if (ZPACK_FSEEK(fp, 0, SEEK_END) != 0)
        {
            printf("Error while reading %s\n", _filenames[i]);
            return 1;
        }
        
        files[i].size = ZPACK_FTELL(fp);
        files[i].buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * files[i].size);
        if (ZPACK_FSEEK(fp, 0, SEEK_SET) != 0)
        {
            printf("Error while reading %s\n", _filenames[i]);
            return 1;
        }

        if (!ZPACK_FREAD(files[i].buffer, files[i].size, 1, fp))
        {
            printf("Error while reading %s\n", _filenames[i]);
            return 1;
        }

        files[i].options = &options;
        ZPACK_FCLOSE(fp);
    }

    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    /**** Write to file ****/
    int ret;
    printf("Write to file\n");
    if ((ret = zpack_init_writer(&writer, OUT_FILE)))
    {
        printf("-- Got error %d from zpack_init_writer\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    if ((ret = zpack_write_archive(&writer, files, 2)))
    {
        printf("-- Got error %d from zpack_init_writer\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    zpack_close_writer(&writer);

    // Verify archive
    zpack_u8 buffer[sizeof(_archive_buffer)];
    fp = ZPACK_FOPEN(OUT_FILE, "rb");
    if (!ZPACK_FREAD(buffer, sizeof(_archive_buffer), 1, fp))
    {
        printf("-- Failed to read written archive\n");
        zpack_close_writer(&writer);
        return 1;
    }
    ZPACK_FCLOSE(fp);
    if (memcmp(_archive_buffer, buffer, sizeof(_archive_buffer)) != 0)
    {
        printf("-- Written archive is invalid\n");
        zpack_close_writer(&writer);
        return 1;
    }

    zpack_close_writer(&writer);
    printf("-- Archive write to " OUT_FILE " successful\n");

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
    if (memcmp(_archive_buffer, writer.buffer, writer.file_size) != 0)
    {
        printf("-- Written archive is invalid\n");
        zpack_close_writer(&writer);
        return 1;
    }

    printf("-- Archive write to buffer successful\n");
    zpack_close_writer(&writer);

    // Free buffers
    for (int i = 0; i < 2; ++i)
        free(files[i].buffer);

    return 0;
}