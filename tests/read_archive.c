#include <zpack.h>
#include <string.h>
#include "archive.h"

#ifdef _WIN32
#define PRId64 "lld"
#else
#include <inttypes.h>
#endif

#define BUFFER_SIZE 350
#define STREAM_IN_SIZE 16
#define STREAM_OUT_SIZE BUFFER_SIZE
zpack_bool read_and_verify_files(zpack_reader* reader, zpack_u8* buffer)
{
    zpack_bool passed = ZPACK_TRUE;
    int ret;

    // Oneshot decompression
    printf("* Oneshot\n");
    for (int i = 0; i < reader->file_count; ++i)
    {
        if ((ret = zpack_read_file(reader, reader->file_entries + i, buffer, BUFFER_SIZE, NULL)))
        {
            printf("Failed to read %s (error %d, last return %" PRId64 ")\n", reader->file_entries[i].filename, ret, (int64_t)reader->last_return);
            passed = ZPACK_FALSE;
            continue;
        }

        zpack_bool valid = memcmp(buffer, _files[i], _uncomp_sizes[i]) == 0;
        passed = passed ? valid : ZPACK_FALSE;
        printf("-- %s is %s\n", reader->file_entries[i].filename, valid ? "valid" : "invalid");

        memset(buffer, 0, BUFFER_SIZE);
    }

    // Streaming decompression
    printf("* Streaming\n");
    zpack_u8 in_buf[STREAM_IN_SIZE];
    zpack_stream stream;
    memset(&stream, 0, sizeof(zpack_stream));
    if (zpack_init_stream(&stream))
    {
        printf("Failed to init stream\n");
        return ZPACK_FALSE;
    }
    for (int i = 0; i < reader->file_count; ++i)
    {
        zpack_reset_stream(&stream);
        stream.next_out = buffer;
        stream.avail_out = STREAM_OUT_SIZE;

        int passes = 0;
        for (;;)
        {
            if (stream.read_back)
                memmove(in_buf, stream.next_in - stream.read_back, stream.read_back);

            stream.next_in = in_buf;
            stream.avail_in = STREAM_IN_SIZE;
            stream.avail_out = STREAM_OUT_SIZE;

            if ((ret = zpack_read_file_stream(reader, reader->file_entries + i, &stream, NULL)))
            {
                printf("Failed to read %s (error %d, last return %" PRId64 ")\n", reader->file_entries[i].filename, ret, (int64_t)reader->last_return);
                passed = ZPACK_FALSE;
                break;
            }

            ++passes;
            //printf("pass %d, total_in %lu, total_out %lu\n", passes, stream.total_in, stream.total_out);

            if (ZPACK_READ_STREAM_DONE(&stream, reader->file_entries + i)) break;
        }

        zpack_bool valid = memcmp(buffer, _files[i], _uncomp_sizes[i]) == 0;
        passed = passed ? valid : ZPACK_FALSE;
        printf("-- %s is %s\n", reader->file_entries[i].filename, valid ? "valid" : "invalid");

        memset(buffer, 0, BUFFER_SIZE);
    }
    zpack_close_stream(&stream);

    return passed;
}

int read_archive(int num)
{
    printf("Archive #%d (%s)\n"
           "----------------------\n",
            num, _archive_names[num]);
    
    zpack_reader reader;
    memset(&reader, 0, sizeof(reader));

    int ret;
    zpack_u8 buffer[BUFFER_SIZE];

    // read from disk
    printf("File read test\n");

    if ((ret = zpack_init_reader(&reader, _archive_names[num])))
    {
        printf("Failed to open archive (error %d)\n", ret);
        return 1;
    }

    zpack_bool passed1 = read_and_verify_files(&reader, buffer);
    zpack_close_reader(&reader);

    // read from buffer
    printf("Buffer read test\n");

    zpack_u64 size = _archive_sizes[num];
    if ((ret = zpack_init_reader_memory(&reader, _archive_buffers[num], size)))
    {
        printf("Failed to open archive (error %d)\n", ret);
        return 1;
    }

    zpack_bool passed2 = read_and_verify_files(&reader, buffer);
    zpack_close_reader(&reader);

    // 0 if passed, 1 if failed
    return !(passed1 && passed2);
}

int main(int argc, char** argv)
{
    int ret = 0;
    for (int i = 0; i < ARCHIVE_COUNT; ++i)
    {
        int tmp = read_archive(i);
        ret = ret ? ret : tmp;
        printf("\n");
    }

    return ret;
}