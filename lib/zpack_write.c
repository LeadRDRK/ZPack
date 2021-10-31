#include "zpack.h"
#include "zpack_common.h"
#include <stdlib.h>
#include <string.h>
#include <xxhash.h>

#define ZPACK_ADD_OFFSET_AND_SIZE(writer, sz) \
    writer->write_offset += sz; \
    writer->file_size += sz

int zpack_init_writer(zpack_writer* writer, const char* path)
{
    writer->file = ZPACK_FOPEN(path, "wb");
    if (!writer->file) return ZPACK_ERROR_OPEN_FAILED;
    return ZPACK_OK;
}

// header size + data signature size
#define ZPACK_INITIAL_HEAP_SIZE ZPACK_HEADER_SIZE + ZPACK_SIGNATURE_SIZE
int zpack_init_writer_heap(zpack_writer* writer, size_t initial_size)
{
    writer->buffer_capacity = ZPACK_MAX(ZPACK_INITIAL_HEAP_SIZE, initial_size);
    writer->buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * writer->buffer_capacity);
    if (!writer->buffer) return ZPACK_ERROR_MALLOC_FAILED;
    return ZPACK_OK;
}

static void zpack_write_header_memory(zpack_u8* p, zpack_u16 version)
{
    // signature
    zpack_write_le32(p, ZPACK_HEADER_SIGNATURE);

    // version
    zpack_write_le16(p + 4, version);
}

int zpack_write_header(zpack_writer* writer)
{
    return zpack_write_header_ex(writer, ZPACK_ARCHIVE_VERSION_MAX);
}

int zpack_write_header_ex(zpack_writer* writer, zpack_u16 version)
{
    int ret;
    if (writer->file)
    {
        zpack_u8 buffer[ZPACK_HEADER_SIZE];
        zpack_write_header_memory(buffer, version);

        if ((ret = zpack_seek_and_write(writer->file, writer->write_offset, buffer, ZPACK_HEADER_SIZE)))
            return ret;
    }
    else if (writer->buffer)
    {
        if ((ret = zpack_check_and_grow_heap(&writer->buffer, &writer->buffer_capacity,
                                             writer->file_size + ZPACK_HEADER_SIZE)))
            return ret;
        
        zpack_write_header_memory(writer->buffer + writer->write_offset, version);
    }
    else
        return ZPACK_ERROR_WRITER_NOT_OPENED;

    ZPACK_ADD_OFFSET_AND_SIZE(writer, ZPACK_HEADER_SIZE);
    return ret;
}

int zpack_write_data_header(zpack_writer* writer)
{
    int ret;
    if (writer->file)
    {
        zpack_u8 buffer[ZPACK_SIGNATURE_SIZE];
        zpack_write_le32(buffer, ZPACK_DATA_SIGNATURE);
        if ((ret = zpack_seek_and_write(writer->file, writer->write_offset, buffer, ZPACK_SIGNATURE_SIZE)))
            return ret;
    }
    else if (writer->buffer)
    {
        if ((ret = zpack_check_and_grow_heap(&writer->buffer, &writer->buffer_capacity,
                                             writer->file_size + ZPACK_SIGNATURE_SIZE)))
            return ret;
        
        zpack_write_le32(writer->buffer + writer->write_offset, ZPACK_DATA_SIGNATURE);
    }
    else
        return ZPACK_ERROR_WRITER_NOT_OPENED;

    ZPACK_ADD_OFFSET_AND_SIZE(writer, ZPACK_SIGNATURE_SIZE);
    return ZPACK_OK;
}

static size_t zpack_get_compress_bound(zpack_compression_method method, size_t src_size)
{
    switch (method)
    {
    case ZPACK_COMPRESSION_ZSTD:
    #ifndef ZPACK_DISABLE_ZSTD
        return ZSTD_COMPRESSBOUND(src_size);
    #else
        return 0;
    #endif
    
    case ZPACK_COMPRESSION_LZ4:
    #ifndef ZPACK_DISABLE_LZ4
        return LZ4_COMPRESSBOUND(src_size);
    #else
        return 0;
    #endif

    case ZPACK_COMPRESSION_LZ4F:
    #ifndef ZPACK_DISABLE_LZ4
        return LZ4F_compressBound(src_size, NULL);
    #else
        return 0;
    #endif

    default:
        return 0;

    }
}

#define ZPACK_PROCEED_LZ4F(writer, offset) \
    if (LZ4F_isError(writer->last_return)) \
    { \
        LZ4F_freeCompressionContext(writer->lz4f_cctx); \
        writer->lz4f_cctx = NULL; \
        return ZPACK_ERROR_COMPRESS_FAILED; \
    } \
    offset += writer->last_return

static int zpack_compress_file(zpack_writer* writer, zpack_u8* buffer, size_t capacity,
                               const zpack_file* file, zpack_u64* comp_size)
{
    switch (file->options->method)
    {
    case ZPACK_COMPRESSION_ZSTD:
    #ifndef ZPACK_DISABLE_ZSTD
        // create the compression context if needed
        if (!writer->zstd_cctx)
        {
            writer->zstd_cctx = ZSTD_createCCtx();
            if (writer->zstd_cctx == NULL) return ZPACK_ERROR_MALLOC_FAILED;
        }
        
        // compress the file
        writer->last_return = ZSTD_compressCCtx(writer->zstd_cctx, buffer, capacity,
                                                file->buffer, file->size, file->options->level);

        // check for errors
        if (ZSTD_isError(writer->last_return))
            return ZPACK_ERROR_COMPRESS_FAILED;

        *comp_size = writer->last_return;
        break;
    #else
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif

    case ZPACK_COMPRESSION_LZ4:
    #ifndef ZPACK_DISABLE_LZ4
        if (file->options->level > 0)
        {
            // LZ4HC (level > 0)
            writer->last_return = LZ4_compress_HC((const char*)file->buffer, (char*)buffer,
                                                  file->size, capacity, file->options->level);
        }
        else
        {
            // Normal/Fast acceleration (level = 0 or negative)
            writer->last_return = LZ4_compress_fast((const char*)file->buffer, (char*)buffer,
                                                    file->size, capacity, -file->options->level + 1);
        }

        if (!writer->last_return)
            return ZPACK_ERROR_COMPRESS_FAILED;

        *comp_size = writer->last_return;
        break;
    #else
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif

    case ZPACK_COMPRESSION_LZ4F:
    #ifndef ZPACK_DISABLE_LZ4
        // create the compression context if needed
        if (!writer->lz4f_cctx)
        {
            if (LZ4F_createCompressionContext((LZ4F_cctx**)&writer->lz4f_cctx, LZ4F_VERSION))
                return ZPACK_ERROR_MALLOC_FAILED;
        }

        // compress the file
        LZ4F_preferences_t prefs;
        memset(&prefs, 0, sizeof(LZ4F_preferences_t));
        prefs.compressionLevel = file->options->level;
        size_t offset = 0;

        writer->last_return = LZ4F_compressBegin(writer->lz4f_cctx, buffer + offset, capacity - offset, &prefs);
        ZPACK_PROCEED_LZ4F(writer, offset);

        writer->last_return = LZ4F_compressUpdate(writer->lz4f_cctx, buffer + offset, capacity - offset, file->buffer, file->size, NULL);
        ZPACK_PROCEED_LZ4F(writer, offset);

        writer->last_return = LZ4F_compressEnd(writer->lz4f_cctx, buffer + offset, capacity - offset, NULL);
        ZPACK_PROCEED_LZ4F(writer, offset);

        *comp_size = offset;
        break;
    #else
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif

    default:
        return ZPACK_ERROR_COMP_METHOD_INVALID;

    }
    return ZPACK_OK;
}

static zpack_file_entry* zpack_push_file_entry(zpack_writer* writer)
{
    if (++writer->file_count > writer->fe_capacity)
    {
        writer->fe_capacity = zpack_get_heap_size(writer->file_count);
        writer->file_entries = (zpack_file_entry*)realloc(writer->file_entries,
                                                          sizeof(zpack_file_entry) * writer->fe_capacity);
        if (writer->file_entries == NULL)
            return NULL;
    }

    return writer->file_entries + (writer->file_count - 1);
}

static int zpack_add_written_file_entry(zpack_writer* writer, zpack_file* file, zpack_u64 comp_size)
{
    zpack_file_entry* entry = zpack_push_file_entry(writer);
    if (entry == NULL) return ZPACK_ERROR_MALLOC_FAILED;

    // filename
    size_t str_size = strlen(file->filename) + 1;
    entry->filename = (char*)malloc(sizeof(char) * str_size);
    memcpy(entry->filename, file->filename, str_size);

    // others
    entry->offset = writer->write_offset;
    entry->comp_size = comp_size;
    entry->uncomp_size = file->size;
    entry->hash = XXH3_64bits(file->buffer, file->size);
    entry->comp_method = file->options->method;

    return ZPACK_OK;
}

static int zpack_copy_file_entry(zpack_writer* writer, zpack_file_entry* src_entry, zpack_u64 new_offset)
{
    zpack_file_entry* entry = zpack_push_file_entry(writer);
    if (entry == NULL) return ZPACK_ERROR_MALLOC_FAILED;

    memcpy(entry, src_entry, sizeof(zpack_file_entry));

    // deep copy filename
    size_t str_size = strlen(src_entry->filename) + 1;
    entry->filename = (char*)malloc(sizeof(char) * str_size);
    memcpy(entry->filename, src_entry->filename, str_size);

    entry->offset = new_offset;

    return ZPACK_OK;
}

int zpack_write_files(zpack_writer* writer, zpack_file* files, zpack_u64 file_count)
{
    zpack_u8* buffer = NULL;
    size_t buffer_capacity = 0;

    int ret;
    zpack_u64 comp_size;
    for (zpack_u64 i = 0; i < file_count; ++i)
    {
        // resize buffer if needed
        size_t compress_bound = zpack_get_compress_bound(files[i].options->method, files[i].size);
        if (buffer_capacity < compress_bound)
        {
            buffer = (zpack_u8*)realloc(buffer, sizeof(zpack_u8) * compress_bound);
            if (buffer == NULL) return ZPACK_ERROR_MALLOC_FAILED;
            buffer_capacity = compress_bound;
        }

        // compress the file
        if ((ret = zpack_compress_file(writer, buffer, buffer_capacity, files + i, &comp_size)))
        {
            free(buffer);
            return ret;
        }

        // write the compressed file
        if (writer->file)
        {
            if ((ret = zpack_seek_and_write(writer->file, writer->write_offset, buffer, comp_size)))
            {
                free(buffer);
                return ret;
            }
        }
        else if (writer->buffer)
        {
            if ((ret = zpack_check_and_grow_heap(&writer->buffer, &writer->buffer_capacity,
                                                 writer->file_size + comp_size)))
            {
                free(buffer);
                return ret;
            }

            memcpy(writer->buffer + writer->write_offset, buffer, comp_size);
        }
        else
        {
            free(buffer);
            return ZPACK_ERROR_WRITER_NOT_OPENED;
        }

        // add file to entry list
        if ((ret = zpack_add_written_file_entry(writer, files + i, comp_size)))
        {
            free(buffer);
            return ret;
        }

        ZPACK_ADD_OFFSET_AND_SIZE(writer, comp_size);
    }

    free(buffer);
    return ZPACK_OK;
}

int zpack_write_files_from_archive(zpack_writer* writer, zpack_reader* reader, zpack_file_entry* entries, zpack_u64 file_count)
{
    zpack_u8* buffer = NULL;
    size_t buffer_capacity = 0;
    zpack_bool buffer_alloc = ZPACK_FALSE;

    int ret;
    for (zpack_u64 i = 0; i < file_count; ++i)
    {
        if (reader->file)
        {
            if (buffer_capacity < entries[i].comp_size)
            {
                buffer_capacity = entries[i].comp_size;
                buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * buffer_capacity);
                buffer_alloc = ZPACK_TRUE;
            }

            if ((ret = zpack_read_raw_file(reader, entries + i, buffer, buffer_capacity)))
            {
                free(buffer);
                return ret;
            }
        }
        else if (reader->buffer)
        {
            if (entries[i].offset >= reader->file_size)
            {
                if (buffer_alloc) free(buffer);
                return ZPACK_ERROR_FILE_OFFSET_INVALID;
            }
            buffer = reader->buffer + entries[i].offset;
        }
        else
            return ZPACK_ERROR_ARCHIVE_NOT_LOADED;
        

        // write the compressed file
        if (writer->file)
        {
            if (ZPACK_FSEEK(writer->file, writer->write_offset, SEEK_SET) != 0)
            {
                if (buffer_alloc) free(buffer);
                return ZPACK_ERROR_SEEK_FAILED;
            }

            if (ZPACK_FWRITE(buffer, 1, entries[i].comp_size, writer->file) != entries[i].comp_size)
            {
                if (buffer_alloc) free(buffer);
                return ZPACK_ERROR_WRITE_FAILED;
            }
        }
        else if (writer->buffer)
        {
            if ((ret = zpack_check_and_grow_heap(&writer->buffer, &writer->buffer_capacity,
                                                 writer->file_size + entries[i].comp_size)))
            {
                if (buffer_alloc) free(buffer);
                return ret;
            }

            memcpy(writer->buffer + writer->write_offset, buffer, entries[i].comp_size);
        }
        else
        {
            if (buffer_alloc) free(buffer);
            return ZPACK_ERROR_WRITER_NOT_OPENED;
        }

        // add file to entry list
        if ((ret = zpack_copy_file_entry(writer, entries + i, writer->write_offset)))
        {
            if (buffer_alloc) free(buffer);
            return ret;
        }

        ZPACK_ADD_OFFSET_AND_SIZE(writer, entries[i].comp_size);
    }

    if (buffer_alloc) free(buffer);
    return ZPACK_OK;
}

static void zpack_write_cdr_memory(zpack_u8* p, zpack_file_entry* entries, zpack_u64 file_count, zpack_u16* fn_lengths, zpack_u64 block_size)
{
    // header
    zpack_write_le32(p, ZPACK_CDR_SIGNATURE); // signature
    zpack_write_le64(p + 4, file_count);      // file count
    zpack_write_le64(p + 12, block_size);     // block size

    // file entries
    p += ZPACK_CDR_HEADER_SIZE;
    for (zpack_u64 i = 0; i < file_count; ++i)
    {
        zpack_write_le16(p, fn_lengths[i]);                // filename length
        memcpy(p + 2, entries[i].filename, fn_lengths[i]); // filename
        p += 2 + fn_lengths[i];

        zpack_write_le64(p, entries[i].offset);            // offset
        zpack_write_le64(p + 8,  entries[i].comp_size);    // compressed size
        zpack_write_le64(p + 16, entries[i].uncomp_size);  // uncompressed size
        zpack_write_le64(p + 24, entries[i].hash);         // file hash
        p[32] = entries[i].comp_method;                    // compression method

        // advance ptr
        p += ZPACK_FILE_ENTRY_FIXED_SIZE - 2;
    }
}

int zpack_write_cdr(zpack_writer* writer)
{
    return zpack_write_cdr_ex(writer, writer->file_entries, writer->file_count);
}

int zpack_write_cdr_ex(zpack_writer* writer, zpack_file_entry* entries, zpack_u64 file_count)
{
    // calculate block size
    zpack_u64 block_size = file_count * ZPACK_FILE_ENTRY_FIXED_SIZE;
    zpack_u16 fn_lengths[file_count];
    for (zpack_u64 i = 0; i < file_count; ++i)
    {
        fn_lengths[i] = strlen(entries[i].filename);
        block_size += fn_lengths[i];
    }
    zpack_u64 size = ZPACK_CDR_HEADER_SIZE + block_size;
    
    int ret;
    if (writer->file)
    {
        zpack_u8* buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * size);
        if (buffer == NULL) return ZPACK_ERROR_MALLOC_FAILED;
        zpack_write_cdr_memory(buffer, entries, file_count, fn_lengths, block_size);

        if ((ret = zpack_seek_and_write(writer->file, writer->write_offset, buffer, size)))
        {
            free(buffer);
            return ret;
        }

        free(buffer);
    }
    else if (writer->buffer)
    {
        int ret;
        if ((ret = zpack_check_and_grow_heap(&writer->buffer, &writer->buffer_capacity,
                                             writer->file_size + size)))
            return ret;
        
        zpack_write_cdr_memory(writer->buffer + writer->write_offset, entries, file_count,
                               fn_lengths, block_size);
    }
    else
        return ZPACK_ERROR_WRITER_NOT_OPENED;
    
    writer->cdr_offset = writer->write_offset;
    ZPACK_ADD_OFFSET_AND_SIZE(writer, size);
    return ZPACK_OK;
}

static void zpack_write_eocdr_memory(zpack_u8* p, zpack_u64 cdr_offset)
{
    // signature
    zpack_write_le32(p, ZPACK_EOCDR_SIGNATURE);

    // offset
    zpack_write_le64(p + 4, cdr_offset);
}

int zpack_write_eocdr(zpack_writer* writer)
{
    return zpack_write_eocdr_ex(writer, writer->cdr_offset);
}

int zpack_write_eocdr_ex(zpack_writer* writer, zpack_u64 cdr_offset)
{
    int ret;
    if (writer->file)
    {
        zpack_u8 buffer[ZPACK_EOCDR_SIZE];
        zpack_write_eocdr_memory(buffer, cdr_offset);

        if ((ret = zpack_seek_and_write(writer->file, writer->write_offset, buffer, ZPACK_EOCDR_SIZE)))
            return ret;
    }
    else if (writer->buffer)
    {
        if ((ret = zpack_check_and_grow_heap(&writer->buffer, &writer->buffer_capacity,
                                             writer->file_size + ZPACK_EOCDR_SIZE)))
            return ret;
        
        zpack_write_eocdr_memory(writer->buffer + writer->write_offset, cdr_offset);
    }
    else
        return ZPACK_ERROR_WRITER_NOT_OPENED;

    ZPACK_ADD_OFFSET_AND_SIZE(writer, ZPACK_EOCDR_SIZE);
    return ret;
}

int zpack_write_archive(zpack_writer* writer, zpack_file* files, zpack_u64 file_count)
{
    int ret;

    if ((ret = zpack_write_header(writer))) return ret;
    if ((ret = zpack_write_data_header(writer))) return ret;
    if ((ret = zpack_write_files(writer, files, file_count))) return ret;
    if ((ret = zpack_write_cdr(writer))) return ret;
    if ((ret = zpack_write_eocdr(writer))) return ret;
    
    return ZPACK_OK;
}

void zpack_close_writer(zpack_writer* writer)
{
    if (writer->file)
        ZPACK_FCLOSE(writer->file);
    
    free(writer->buffer);

    if (writer->file_entries)
    {
        for (zpack_u64 i = 0; i < writer->file_count; ++i)
            free(writer->file_entries[i].filename);

        free(writer->file_entries);
    }

    // compression contexts
#ifndef ZPACK_DISABLE_ZSTD
    ZSTD_freeCCtx(writer->zstd_cctx);
#endif

#ifndef ZPACK_DISABLE_LZ4
    LZ4F_freeCompressionContext(writer->lz4f_cctx);
#endif

    memset(writer, 0, sizeof(zpack_writer));
}