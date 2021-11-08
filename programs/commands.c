#include "commands.h"
#include <zpack.h>
#include <zpack_common.h>
#include <string.h>
#include <stdlib.h>

#define WRITE_ERROR(writer, stream, in_buf, out_buf) \
    zpack_close_writer(writer); \
    zpack_close_stream(stream); \
    free(in_buf); \
    free(out_buf); \
    return 1

int write_files(zpack_writer* writer, zpack_compress_options* options, char** paths, int path_count, size_t* orig_size)
{
    zpack_stream stream;
    memset(&stream, 0, sizeof(stream));
    zpack_init_stream(&stream);

    zpack_u8* in_buf = NULL;
    zpack_u8* out_buf = NULL;

    zpack_u32 in_size = zpack_get_cstream_in_size(options->method);
    in_buf = (zpack_u8*)malloc(sizeof(zpack_u8) * in_size);
    if (in_buf == NULL)
    {
        printf("Error: Failed to allocate memory\n");
        WRITE_ERROR(writer, &stream, in_buf, out_buf);
    }

    zpack_u32 out_size = zpack_get_cstream_out_size(options->method);
    out_buf = (zpack_u8*)malloc(sizeof(zpack_u8) * out_size);
    if (out_buf == NULL)
    {
        printf("Error: Failed to allocate memory\n");
        WRITE_ERROR(writer, &stream, in_buf, out_buf);
    }
    // output stream will never get updated
    stream.next_out = out_buf;
    stream.avail_out = out_size;

    int ret;
    for (int i = 0; i < path_count; ++i)
    {
        FILE* fp = ZPACK_FOPEN(paths[i], "rb");
        if (fp == NULL)
        {
            printf("Error: Failed to open \"%s\" for reading\n", paths[i]);
            WRITE_ERROR(writer, &stream, in_buf, out_buf);
        }

        printf("  %s\n", paths[i]);
        zpack_bool is_eof = ZPACK_FALSE;
        while (!is_eof)
        {
            // read some data
            stream.next_in = in_buf;
            if ((stream.avail_in = ZPACK_FREAD(in_buf, 1, in_size, fp)) < in_size)
            {
                if (feof(fp))
                    is_eof = ZPACK_TRUE;
                else
                {
                    printf("Error: Failed to read \"%s\"\n", paths[i]);
                    WRITE_ERROR(writer, &stream, in_buf, out_buf);
                }
            }

            // compress the data
            if (stream.avail_in > 0)
            {
                if (orig_size) *orig_size += stream.avail_in;
                if ((ret = zpack_write_file_stream(writer, options, &stream, NULL)))
                {
                    printf("Error: Failed to compress \"%s\" (error %d)\n", paths[i], ret);
                    WRITE_ERROR(writer, &stream, in_buf, out_buf);
                }
            }
        }
        zpack_write_file_stream_end(writer, paths[i], options, &stream, NULL);
        ZPACK_FCLOSE(fp);

        // reset stream
        stream.total_in = 0;
        stream.total_out = 0;
    }

    free(in_buf);
    free(out_buf);
    zpack_close_stream(&stream);

    return 0;
}

int command_create(args_options* options)
{
    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    if (options->path_count < 2)
    {
        printf("Error: Insufficient amount of files provided\n");
        return 1;
    }

    char* archive_path = options->path_list[0];
    int ret;
    if ((ret = zpack_init_writer(&writer, archive_path)))
    {
        printf("Error: Failed to open \"%s\" for writing (error %d)\n", archive_path, ret);
        return 1;
    }
    printf("-- Creating archive: %s\n", archive_path);

    // Write archive start
    if ((ret = zpack_write_header(&writer)))
    {
        printf("Error: Failed to write archive header (error %d)\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    if ((ret = zpack_write_data_header(&writer)))
    {
        printf("Error: Failed to write data header (error %d)\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    // Write files
    printf("-- Writing files...\n");
    size_t orig_size = 0;
    if (write_files(&writer, &options->comp_options, options->path_list + 1, options->path_count - 1, &orig_size))
        return 1;

    // Write archive end
    if ((ret = zpack_write_cdr(&writer)))
    {
        printf("Error: Failed to write CDR (error %d)\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    if ((ret = zpack_write_eocdr(&writer)))
    {
        printf("Error: Failed to write EOCDR (error %d)\n", ret);
        zpack_close_writer(&writer);
        return 1;
    }

    printf("-- Done.\n"
           "-- Archive size: %lu bytes\n"
           "-- Compression ratio: %f%%\n",
           writer.file_size, ((float)writer.file_size / orig_size) * 100);
    zpack_close_writer(&writer);
    return 0;
}

int command_add(args_options* options)
{
    return 0;
}

int command_extract(args_options* options)
{
    return 0;
}

int command_extract_full(args_options* options)
{
    return 0;
}

int command_list(args_options* options)
{
    return 0;
}

int command_delete(args_options* options)
{
    return 0;
}

int command_move(args_options* options)
{
    return 0;
}

int command_test(args_options* options)
{
    return 0;
}