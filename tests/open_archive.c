#include <zpack.h>
#include <string.h>
#include "archive.h"

zpack_bool print_and_verify_archive(zpack_reader* reader)
{
    zpack_file_entry* entries = reader->file_entries;
    printf("File count: %lu\n\n", reader->file_count);

    zpack_bool passed = ZPACK_TRUE;
    for (zpack_u64 i = 0; i < reader->file_count; ++i)
    {
        zpack_bool entry_passed = (
            strcmp(entries[i].filename, _filenames[i]) == 0 &&
            entries[i].uncomp_size == _uncomp_sizes[i] &&
            entries[i].hash == _hashes[i]
        );

        printf("File #%lu\n"
               "Filename: %s\n"
               "Compressed size: %lu\n"
               "Uncompressed size: %lu\n"
               "File hash: %lx\n"
               "Compression method: %u\n"
               "* This entry is %s\n"
               "\n",
        i + 1, entries[i].filename, entries[i].comp_size, entries[i].uncomp_size,
        entries[i].hash, entries[i].comp_method, entry_passed ? "valid" : "invalid");

        passed = passed ? entry_passed : ZPACK_FALSE;
    }

    if (passed) printf("-- (GOOD) All entries are valid\n\n");
    else printf("-- (BAD) One or more entries are invalid\n\n");

    return passed;
}

int open_archive(int num)
{
    printf("Archive #%d\n"
           "----------------------\n", num);

    zpack_reader reader;
    memset(&reader, 0, sizeof(reader));
    int ret;

    // read from disk
    printf("File read test\n");

    if ((ret = zpack_init_reader(&reader, _archive_names[num])))
    {
        printf("Error %d\n", ret);
        return 1;
    }

    zpack_bool passed1 = print_and_verify_archive(&reader);
    zpack_close_reader(&reader);

    // read from buffer
    printf("Buffer read test\n");

    zpack_u64 size = _archive_sizes[num];
    if ((ret = zpack_init_reader_memory(&reader, _archive_buffers[num], size)))
    {
        printf("Error %d\n", ret);
        return 1;
    }

    zpack_bool passed2 = print_and_verify_archive(&reader);
    zpack_close_reader(&reader);

    // read from buffer (shared)
    printf("Buffer read test (shared)\n");

    zpack_u8 tmp[size];
    memcpy(tmp, _archive_buffers[num], size);
    if ((ret = zpack_init_reader_memory_shared(&reader, tmp, size)))
    {
        printf("Error %d\n", ret);
        return 1;
    }

    zpack_bool passed3 = print_and_verify_archive(&reader);
    zpack_close_reader(&reader);

    // 0 if passed, 1 if failed
    return !(passed1 && passed2 && passed3);
}

int main(int argc, char** argv)
{
    int ret;
    for (int i = 0; i < ARCHIVE_COUNT; ++i)
    {
        if ((ret = open_archive(i)))
            return ret;
    }

    return 0;
}