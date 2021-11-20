#include "commands.h"
#include "utils.h"
#include "platform_defs.h"
#include <zpack.h>
#include <zpack_common.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef PLATFORM_WIN32
#define PRIu64 "llu"
#else
#include <inttypes.h>
#endif

#define WRITE_ERROR(writer, stream, in_buf, out_buf) \
    zpack_close_writer(writer); \
    zpack_close_stream(stream); \
    free(in_buf); \
    free(out_buf); \
    return 1

static int write_start(zpack_writer* writer, args_options* options, char* archive_path)
{
    if (options->path_count < 2)
    {
        printf("Error: Insufficient amount of files provided\n");
        return 1;
    }

    int ret;
    if ((ret = zpack_init_writer(writer, archive_path)))
    {
        printf("Error: Failed to open \"%s\" for writing (error %d)\n", archive_path, ret);
        return 1;
    }

    if ((ret = zpack_write_header(writer)))
    {
        printf("Error: Failed to write archive header (error %d)\n", ret);
        zpack_close_writer(writer);
        return 1;
    }

    if ((ret = zpack_write_data_header(writer)))
    {
        printf("Error: Failed to write data header (error %d)\n", ret);
        zpack_close_writer(writer);
        return 1;
    }

    return 0;
}

static int write_files(zpack_writer* writer, args_options* options, zpack_compress_options* comp_options, size_t* orig_size)
{
    char** paths = options->path_list + 1;
    int path_count = options->path_count - 1;

    // get archive full path
    char arc_full_path[PATH_MAX+1];
    if (utils_get_full_path(arc_full_path, options->path_list[0]) == NULL)
    {
        printf("Error: Archive path invalid\n");
        return 1;
    }

    // find files
    path_filename* files = NULL;
    int file_count = 0;
    if (!utils_prepare_file_list(paths, path_count, &files, &file_count))
        return 1;
    printf("-- Found %d files\n", file_count);

    zpack_stream stream;
    memset(&stream, 0, sizeof(stream));
    zpack_init_stream(&stream);

    size_t in_size = zpack_get_cstream_in_size(comp_options->method);
    zpack_u8* in_buf = (zpack_u8*)malloc(sizeof(zpack_u8) * in_size);

    size_t out_size = zpack_get_cstream_out_size(comp_options->method);
    zpack_u8* out_buf = (zpack_u8*)malloc(sizeof(zpack_u8) * out_size);

    if (in_buf == NULL || out_buf == NULL)
    {
        printf("Error: Failed to allocate memory\n");
        WRITE_ERROR(writer, &stream, in_buf, out_buf);
    }
    // output stream will never get updated
    stream.next_out = out_buf;
    stream.avail_out = out_size;

    printf("-- Writing files...\n");
    int ret;
    char full_path[PATH_MAX+1];
    for (int i = 0; i < file_count; ++i)
    {
        printf("  %s\n", files[i].filename);

        // check if file already exists
        if (zpack_get_file_entry(files[i].filename, writer->file_entries, writer->file_count))
        {
            printf("Warning: File already exists in archive, ignoring\n");
            continue;
        }

        // check if file is archive
        if (utils_get_full_path(full_path, files[i].path) == NULL)
        {
            printf("Error: File path invalid: %s\n", files[i].path);
            return 1;
        }
        if (strcmp(full_path, arc_full_path) == 0)
        {
            printf("Warning: File is archive, ignoring\n");
            continue;
        }

        FILE* fp = ZPACK_FOPEN(files[i].path, "rb");
        if (fp == NULL)
        {
            printf("Error: Failed to open \"%s\" for reading\n", files[i].path);
            WRITE_ERROR(writer, &stream, in_buf, out_buf);
        }

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
                    printf("Error: Failed to read \"%s\"\n", files[i].path);
                    ZPACK_FCLOSE(fp);
                    WRITE_ERROR(writer, &stream, in_buf, out_buf);
                }
            }

            // compress the data
            if (stream.avail_in > 0)
            {
                if (orig_size) *orig_size += stream.avail_in;
                if ((ret = zpack_write_file_stream(writer, comp_options, &stream, NULL)))
                {
                    printf("Error: Failed to compress \"%s\" (error %d)\n", files[i].filename, ret);
                    ZPACK_FCLOSE(fp);
                    WRITE_ERROR(writer, &stream, in_buf, out_buf);
                }
            }
        }
        zpack_write_file_stream_end(writer, files[i].filename, comp_options, &stream, NULL);
        ZPACK_FCLOSE(fp);

        // reset stream
        stream.total_in = 0;
        stream.total_out = 0;
    }

    utils_free_file_list(files, file_count);
    free(in_buf);
    free(out_buf);
    zpack_close_stream(&stream);

    return 0;
}

static int write_end(zpack_writer* writer, size_t orig_size)
{
    int ret;
    if ((ret = zpack_write_cdr(writer)))
    {
        printf("Error: Failed to write CDR (error %d)\n", ret);
        zpack_close_writer(writer);
        return 1;
    }

    if ((ret = zpack_write_eocdr(writer)))
    {
        printf("Error: Failed to write EOCDR (error %d)\n", ret);
        zpack_close_writer(writer);
        return 1;
    }

    printf("-- Done.\n"
           "-- Archive size: %" PRIu64 " bytes\n"
           "-- Compression ratio: %f%%\n",
           writer->file_size, ((float)writer->file_size / orig_size) * 100);
    zpack_close_writer(writer);

    return 0;
}

int command_create(args_options* options)
{
    printf("-- Creating archive: %s\n", options->path_list[0]);

    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    // Init writer/Write archive start (header + data signature)
    int ret;
    if ((ret = write_start(&writer, options, options->path_list[0])))
        return ret;

    // Write files
    size_t orig_size = 0;
    if ((ret = write_files(&writer, options, &options->comp_options, &orig_size)))
        return ret;

    // Write archive end (cdr + eocdr)
    if ((ret = write_end(&writer, orig_size)))
        return ret;
    
    return 0;
}

static int open_archive_rw(args_options* options, zpack_reader* reader, zpack_writer* writer, char* tmp_path)
{
    char* archive_path = options->path_list[0];
    int ret;
    // Open file in reader
    if ((ret = zpack_init_reader(reader, archive_path)))
    {
        printf("Error: Failed to open \"%s\" for reading (error %d)\n", archive_path, ret);
        return 1;
    }

    // Generate temporary filename
    utils_remove_trailing_separators(archive_path);
    utils_get_tmp_path(archive_path, tmp_path);

    // Init writer/Write archive start (header + data signature)
    if ((ret = write_start(writer, options, tmp_path)))
    {
        zpack_close_reader(reader);
        zpack_close_writer(writer);
        return ret;
    }

    return 0;
}

int command_add(args_options* options)
{
    int ret;

    zpack_reader reader;
    memset(&reader, 0, sizeof(zpack_reader));

    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    char* archive_path = options->path_list[0];
    char* tmp_path = (char*)malloc(sizeof(char) * (strlen(archive_path) + 7));

    // Open archive in rw mode (original archive + temporary archive)
    if ((ret = open_archive_rw(options, &reader, &writer, tmp_path)))
        return ret;

    // Write files from old archive
    if ((ret = zpack_write_files_from_archive(&writer, &reader, reader.file_entries, reader.file_count)))
    {
        printf("Error: Failed to copy data from archive (error %d)\n", ret);
        zpack_close_reader(&reader);
        zpack_close_writer(&writer);
		free(tmp_path);
        return 1;
    }

    size_t orig_size = reader.uncomp_size;
    zpack_close_reader(&reader);

    // Write new files
    if ((ret = write_files(&writer, options, &options->comp_options, &orig_size)))
	{
		free(tmp_path);
        return ret;
	}

    // Write archive end (cdr + eocdr)
    if ((ret = write_end(&writer, orig_size)))
	{
		free(tmp_path);
        return ret;
	}
    
    // Move file back to original path
	ret = utils_move(tmp_path, archive_path);
	free(tmp_path);
    if (!ret)
    {
        printf("Error: Failed to move temporary archive back to original file\n");
        return 1;
    }

    return 0;
}

static int init_decompress_stream(zpack_stream* stream)
{
    zpack_init_stream(stream);

    // Using Zstd stream sizes since it's larger and can also hold LZ4 data properly
    stream->avail_in = zpack_get_dstream_in_size(ZPACK_COMPRESSION_ZSTD);
    stream->next_in = (zpack_u8*)malloc(sizeof(zpack_u8) * stream->avail_in);

    stream->avail_out = zpack_get_dstream_out_size(ZPACK_COMPRESSION_ZSTD);
    stream->next_out = (zpack_u8*)malloc(sizeof(zpack_u8) * stream->avail_out);

    if (stream->next_in == NULL || stream->next_out == NULL)
    {
        printf("Error: Failed to allocate memory\n");
        free(stream->next_in);
        free(stream->next_out);
        return 1;
    }

    return 0;
}

static int extract_file(zpack_reader* reader, zpack_stream* stream, zpack_file_entry* entry, const char* filename, const char* output)
{
    size_t output_length = output ? strlen(output) : 0;
    size_t fn_length = strlen(filename);
    char* path = (char*)malloc(sizeof(char) * (output_length + fn_length + 2));
    if (output)
    {
        memcpy(path, output, output_length);
        path[output_length] = '/';
        memcpy(path + output_length + 1, filename, fn_length + 1);
    }
    else memcpy(path, filename, fn_length + 1);
    utils_convert_separators(path);

    if (!utils_mkdir_p(path, ZPACK_TRUE))
    {
        printf("Error: Failed to create output directory for \"%s\" ", path);
		utils_print_strerror();
		free(path);
        return 1;
    }

    FILE* fp = ZPACK_FOPEN(path, "wb");
    if (fp == NULL)
    {
        printf("Failed to open \"%s\" for writing\n", path);
		free(path);
        return 1;
    }

    zpack_u8* in_buf = stream->next_in;
    zpack_u8* out_buf = stream->next_out;
    size_t in_size = stream->avail_in;
    size_t out_size = stream->avail_out;

    // Reset stream
    stream->total_out = 0;
    stream->total_in = 0;

    printf("  %s\n", entry->filename);
    int ret;
    for (;;)
    {
        if ((ret = zpack_read_file_stream(reader, entry, stream, NULL)))
        {
            if (ret == ZPACK_ERROR_FILE_HASH_MISMATCH)
                printf("Warning: File is corrupted (file hash mismatch)\n");
            else
            {
                printf("Error: Failed to extract \"%s\" (error %d)\n", entry->filename, ret);
                ZPACK_FCLOSE(fp);
                return 1;
            }
        }

        size_t write_size = stream->next_out - out_buf;
        if (ZPACK_FWRITE(out_buf, 1, write_size, fp) != write_size)
        {
            printf("Error: Failed to write data to \"%s\"", path);
			free(path);
            ZPACK_FCLOSE(fp);
            return 1;
        }

        // Reset buffers
        // The input buffer might not be fully decompressed in one go, in which case
        // we need to copy what's left to continue decompressing.
        // ZPack expects that data to be at the beginning of the next buffer.
        if (stream->read_back)
            memmove(in_buf, stream->next_in - stream->read_back, stream->read_back);
        stream->next_in = in_buf;
        stream->avail_in = in_size;

        stream->next_out = out_buf;
        stream->avail_out = out_size;

        if (stream->total_out == entry->uncomp_size) break;
    }
	
	free(path);
    ZPACK_FCLOSE(fp);
    return 0;
}

static int extract_files_i(args_options* options, zpack_bool full_path)
{
    char* archive_path = options->path_list[0];
    printf("-- Reading archive: %s\n", archive_path);

    zpack_reader reader;
    memset(&reader, 0, sizeof(zpack_reader));

    int ret;
    if ((ret = zpack_init_reader(&reader, archive_path)))
    {
        printf("Error: Failed to open \"%s\" for reading (error %d)\n", archive_path, ret);
        return 1;
    }
    printf("-- Found %" PRIu64 " files\n", reader.file_count);

    zpack_stream stream;
    memset(&stream, 0, sizeof(zpack_stream));
    if ((ret = init_decompress_stream(&stream)))
        return ret;
    zpack_u8* in_buf = stream.next_in;
    zpack_u8* out_buf = stream.next_out;

    printf("-- Extracting files...\n");
    ret = 0;
	char* fn_buf = NULL;
	zpack_u32 fn_buf_size = 0;
    for (zpack_u64 i = 0; i < reader.file_count; ++i)
    {
        zpack_file_entry* entry = reader.file_entries + i;
        zpack_bool exclude = ZPACK_FALSE;
        for (int x = 0; x < options->exclude_count; ++x)
        {
            if (strcmp(options->exclude_list[x], entry->filename) == 0)
            {
                exclude = ZPACK_TRUE;
                break;
            }
        }
        if (exclude) continue;

        if (full_path)
        {
			zpack_u32 size = (zpack_u32)strlen(entry->filename) + 1;
			if (fn_buf_size < size)
			{
				fn_buf = (char*)realloc(fn_buf, sizeof(char) * size);
				fn_buf_size = size;
			}
            utils_process_path(entry->filename, fn_buf);
            ret = extract_file(&reader, &stream, entry, fn_buf, options->output);
        }
        else ret = extract_file(&reader, &stream, entry, utils_get_filename(entry->filename, 0), options->output);

        if (ret)
        {
            ret = 1;
            break;
        }
    }

    if (ret == 0) printf("-- Done.\n");
	free(fn_buf);
    free(in_buf);
    free(out_buf);
    zpack_close_stream(&stream);
    zpack_close_reader(&reader);
    return ret;
}

int command_extract(args_options* options)
{
    return extract_files_i(options, ZPACK_FALSE);
}

int command_extract_full(args_options* options)
{
    return extract_files_i(options, ZPACK_TRUE);
}

#define PRINT_LIST_ROW(s1, s2, method, name) printf("%12" PRIu64 " %12" PRIu64 " %8s  %s\n", s1, s2, method, name)
#define ROW_SEPARATOR "------------ ------------ --------  ------------------------\n"
int command_list(args_options* options)
{
    char* archive_path = options->path_list[0];
    printf("-- Reading archive: %s\n", archive_path);

    zpack_reader reader;
    memset(&reader, 0, sizeof(zpack_reader));

    int ret;
    if ((ret = zpack_init_reader(&reader, archive_path)))
    {
        printf("Error: Failed to open \"%s\" for reading (error %d)\n", archive_path, ret);
        return 1;
    }

    printf("%12s %12s %8s  %s\n" ROW_SEPARATOR, "Size", "Compressed", "Method", "Name");
    for (zpack_u64 i = 0; i < reader.file_count; ++i)
    {
        zpack_file_entry* entry = reader.file_entries + i;
        char* method;
        switch (entry->comp_method)
        {
        case ZPACK_COMPRESSION_ZSTD:
            method = "zstd";
            break;

        case ZPACK_COMPRESSION_LZ4:
            method = "lz4";
            break;

        }
        PRINT_LIST_ROW(entry->uncomp_size, entry->comp_size, method, entry->filename);
    }
    printf(ROW_SEPARATOR "%12" PRIu64 " %12" PRIu64 " %8s  %" PRIu64 " files\n",
           reader.uncomp_size, reader.comp_size, "", reader.file_count);

    zpack_close_reader(&reader);
    return 0;
}

int command_delete(args_options* options)
{
    int ret;

    zpack_reader reader;
    memset(&reader, 0, sizeof(zpack_reader));

    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    char* archive_path = options->path_list[0];
    char* tmp_path = (char*)malloc(sizeof(char) * (strlen(archive_path) + 7));

    // Open archive in rw mode (original archive + temporary archive)
    if ((ret = open_archive_rw(options, &reader, &writer, tmp_path)))
	{
		free(tmp_path);
        return ret;
	}

    // Write files from old archive, deleting specified files
    printf("-- Deleting files...\n");
    size_t orig_size = reader.uncomp_size;
    zpack_bool file_deleted = ZPACK_FALSE;
    for (zpack_u64 i = 0; i < reader.file_count; ++i)
    {
        zpack_bool delete = ZPACK_FALSE;
        for (int x = 1; x < options->path_count; ++x)
        {
            if (strcmp(options->path_list[x], reader.file_entries[i].filename) == 0)
            {
                printf("  %s\n", options->path_list[x]);
                delete = ZPACK_TRUE;
                orig_size -= reader.file_entries[i].uncomp_size;
                if (!file_deleted) file_deleted = ZPACK_TRUE;
                break;
            }
        }
        if (delete) continue;

        if ((ret = zpack_write_files_from_archive(&writer, &reader, reader.file_entries + i, 1)))
        {
            printf("Error: Failed to copy data from archive (error %d)\n", ret);
			free(tmp_path);
            zpack_close_reader(&reader);
            zpack_close_writer(&writer);
            return 1;
        }
    }

    if (!file_deleted)
        printf("Warning: No files were deleted\n");

    zpack_close_reader(&reader);

    // Write archive end (cdr + eocdr)
    if ((ret = write_end(&writer, orig_size)))
	{
		free(tmp_path);
        return ret;
	}

	ret = utils_move(tmp_path, archive_path);
	free(tmp_path);
    if (!ret)
    {
        printf("Error: Failed to move temporary archive back to original file\n");
        return 1;
    }
    return 0;
}

int command_move(args_options* options)
{
    // File count check
    if (options->path_count % 2 == 0)
    {
        printf("Error: Insufficient destination to source count\n");
        return 1;
    }

    int ret;

    zpack_reader reader;
    memset(&reader, 0, sizeof(zpack_reader));

    zpack_writer writer;
    memset(&writer, 0, sizeof(zpack_writer));

    char* archive_path = options->path_list[0];
    char* tmp_path = (char*)malloc(sizeof(char) * (strlen(archive_path) + 7));

    // Open archive in rw mode (original archive + temporary archive)
    if ((ret = open_archive_rw(options, &reader, &writer, tmp_path)))
    {
		free(tmp_path);
        return ret;
	}

    // Write files from old archive, moving specified files
    printf("-- Moving files...\n");
    size_t orig_size = reader.uncomp_size;
    zpack_bool file_moved = ZPACK_FALSE;
    for (zpack_u64 i = 0; i < reader.file_count; ++i)
    {
        zpack_file_entry* entry = reader.file_entries + i;
        zpack_bool moved = ZPACK_FALSE;
        for (int x = 1; x < options->path_count; x += 2)
        {
            if (strcmp(options->path_list[x], entry->filename) == 0)
            {
                printf("  %s -> %s\n", options->path_list[x], options->path_list[x + 1]);
                // free the original filename
                free(entry->filename);
                entry->filename = options->path_list[x + 1];

                moved = ZPACK_TRUE;
                if (!file_moved) file_moved = ZPACK_TRUE;
                break;
            }
        }

        if ((ret = zpack_write_files_from_archive(&writer, &reader, entry, 1)))
        {
            printf("Error: Failed to copy data from archive (error %d)\n", ret);
			free(tmp_path);
            zpack_close_reader(&reader);
            zpack_close_writer(&writer);
            return 1;
        }

        if (moved) entry->filename = NULL;
    }

    if (!file_moved)
        printf("Warning: No files were moved\n");

    zpack_close_reader(&reader);

    // Write archive end (cdr + eocdr)
    if ((ret = write_end(&writer, orig_size)))
	{
		free(tmp_path);
        return ret;
	}

    ret = utils_move(tmp_path, archive_path);
	free(tmp_path);
    if (!ret)
    {
        printf("Error: Failed to move temporary archive back to original file\n");
        return 1;
    }
    return 0;
}

int command_test(args_options* options)
{
    char* archive_path = options->path_list[0];
    printf("-- Reading archive: %s\n", archive_path);

    zpack_reader reader;
    memset(&reader, 0, sizeof(zpack_reader));

    int ret;
    if ((ret = zpack_init_reader(&reader, archive_path)))
    {
        printf("Error: Failed to open \"%s\" for reading (error %d)\n", archive_path, ret);
        return 1;
    }
    printf("-- Found %" PRIu64 " files\n", reader.file_count);

    zpack_stream stream;
    memset(&stream, 0, sizeof(zpack_stream));
    if ((ret = init_decompress_stream(&stream)))
        return ret;
    zpack_u8* in_buf = stream.next_in;
    zpack_u8* out_buf = stream.next_out;
    size_t in_size = stream.avail_in;
    size_t out_size = stream.avail_out;

    printf("-- Testing files...\n");
    zpack_u64 corrupt_count = 0;
    for (zpack_u64 i = 0; i < reader.file_count; ++i)
    {
        zpack_file_entry* entry = reader.file_entries + i;
        printf("  %s\n", entry->filename);

        // Reset stream
        stream.total_out = 0;
        stream.total_in = 0;
        for (;;)
        {
            if ((ret = zpack_read_file_stream(&reader, entry, &stream, NULL)))
            {
                if (ret == ZPACK_ERROR_FILE_HASH_MISMATCH)
                {
                    printf("-- File is corrupted!\n");
                    ++corrupt_count;
                    break;
                }
                
                printf("Error: Failed to decompress \"%s\" (error %d)\n", entry->filename, ret);
                zpack_close_reader(&reader);
                return 1;
            }

            if (stream.read_back)
                memmove(in_buf, stream.next_in - stream.read_back, stream.read_back);
            stream.next_in = in_buf;
            stream.avail_in = in_size;

            stream.next_out = out_buf;
            stream.avail_out = out_size;

            if (stream.total_out == entry->uncomp_size) break;
        }
    }

    printf("-- Done.\n"
           "-- Corrupted files: %" PRIu64 "/%" PRIu64 "\n", corrupt_count, reader.file_count);
    zpack_close_reader(&reader);
    return 0;
}