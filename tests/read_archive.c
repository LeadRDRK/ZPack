#include <zpack.h>
#include <string.h>
#include "archive.h"

#define BUFFER_SIZE 350
zpack_bool read_and_verify_files(zpack_reader* reader, zpack_u8* buffer)
{
    zpack_bool passed = ZPACK_TRUE;
    int ret;

    // Oneshot decompression
    printf("* Oneshot\n");
    for (int i = 0; i < reader->file_count; ++i)
    {
        if ((ret = zpack_read_file(reader, &reader->file_entries[i], buffer, BUFFER_SIZE)))
        {
            printf("Failed to read %s (error %d, last return %ld)\n", reader->file_entries[i].filename, ret, reader->last_return);
            passed = ZPACK_FALSE;
            continue;
        }

        zpack_bool valid = memcmp(buffer, _files[i], reader->file_entries[i].uncomp_size) == 0;
        passed = passed ? valid : ZPACK_FALSE;
        printf("-- %s is %s\n", reader->file_entries[i].filename, valid ? "valid" : "invalid");
    }

    // Streaming decompression
    

    return passed;
}

int read_archive(int num)
{
    printf("Archive #%d\n"
           "----------------------\n", num);
    
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

    // read from buffer (shared)
    printf("Buffer read test (shared)\n");

    zpack_u8 tmp[size];
    memcpy(tmp, _archive_buffers[num], size);
    if ((ret = zpack_init_reader_memory_shared(&reader, tmp, size)))
    {
        printf("Failed to open archive (error %d)\n", ret);
        return 1;
    }

    zpack_bool passed3 = read_and_verify_files(&reader, buffer);
    zpack_close_reader(&reader);

    // 0 if passed, 1 if failed
    return !(passed1 && passed2 && passed3);
}

int main(int argc, char** argv)
{
    int ret;
    for (int i = 0; i < ARCHIVE_COUNT; ++i)
    {
        if ((ret = read_archive(i)))
            return ret;
    }

    return 0;
}