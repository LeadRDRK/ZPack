#include <zpack.h>
#include <string.h>
#include "archive.h"

#define BUFFER_SIZE 350
zpack_bool read_and_verify_files(zpack_archive* archive, zpack_u8* buffer)
{
    zpack_bool passed = ZPACK_TRUE;
    int ret;

    // Oneshot decompression
    printf("* Oneshot\n");
    for (int i = 0; i < archive->file_count; ++i)
    {
        if ((ret = zpack_archive_read_file(archive, &archive->file_entries[i], buffer, BUFFER_SIZE)))
        {
            printf("Failed to read %s (error %d, last return %ld)\n", archive->file_entries[i].filename, ret, archive->last_return);
            passed = ZPACK_FALSE;
            continue;
        }

        zpack_bool valid = memcmp(buffer, _files[i], archive->file_entries[i].uncomp_size) == 0;
        passed = passed ? valid : ZPACK_FALSE;
        printf("-- %s is %s\n", archive->file_entries[i].filename, valid ? "valid" : "invalid");
    }

    // Streaming decompression
    

    return passed;
}

int read_archive(int num)
{
    printf("Archive #%d\n"
           "----------------------\n", num);
    
    zpack_archive archive;
    memset(&archive, 0, sizeof(archive));

    int ret;
    zpack_u8 buffer[BUFFER_SIZE];

    // read from disk
    printf("File read test\n");

    if ((ret = zpack_open_archive(&archive, _archive_names[num])))
    {
        printf("Failed to open archive (error %d)\n", ret);
        return 1;
    }

    zpack_bool passed1 = read_and_verify_files(&archive, buffer);
    zpack_close_archive(&archive);

    // read from buffer
    printf("Buffer read test\n");

    zpack_u64 size = _archive_sizes[num];
    if ((ret = zpack_open_archive_memory(&archive, _archive_buffers[num], size)))
    {
        printf("Failed to open archive (error %d)\n", ret);
        return 1;
    }

    zpack_bool passed2 = read_and_verify_files(&archive, buffer);
    zpack_close_archive(&archive);

    // read from buffer (shared)
    printf("Buffer read test (shared)\n");

    zpack_u8 tmp[size];
    memcpy(tmp, _archive_buffers[num], size);
    if ((ret = zpack_open_archive_memory_shared(&archive, tmp, size)))
    {
        printf("Failed to open archive (error %d)\n", ret);
        return 1;
    }

    zpack_bool passed3 = read_and_verify_files(&archive, buffer);
    zpack_close_archive(&archive);

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