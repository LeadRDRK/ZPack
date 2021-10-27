#include "zpack.h"
#include "zpack_common.h"
#include <stdlib.h>
#include <string.h>
#include <xxhash.h>

int zpack_read_header_memory(const zpack_u8* buffer, zpack_u16* version)
{
    if (!ZPACK_VERIFY_SIGNATURE(buffer, ZPACK_HEADER_SIGNATURE))
        return ZPACK_ERROR_SIGNATURE_INVALID;

    *version = ZPACK_READ_LE16(buffer + 4);
    if (*version < ZPACK_ARCHIVE_VERSION_MIN || *version > ZPACK_ARCHIVE_VERSION_MAX)
        return ZPACK_ERROR_VERSION_INCOMPATIBLE;
    
    return ZPACK_OK;
}

int zpack_read_header(FILE* fp, zpack_u16* version)
{
    if (ZPACK_FSEEK(fp, 0, SEEK_SET) != 0)
        return ZPACK_ERROR_SEEK_FAILED;

    zpack_u8 buffer[ZPACK_HEADER_SIZE];
    if (!ZPACK_FREAD(buffer, ZPACK_HEADER_SIZE, 1, fp))
        return ZPACK_ERROR_READ_FAILED;

    return zpack_read_header_memory(buffer, version);
}

int zpack_read_data_header_memory(const zpack_u8* buffer)
{
    if (!ZPACK_VERIFY_SIGNATURE(buffer, ZPACK_DATA_SIGNATURE))
        return ZPACK_ERROR_SIGNATURE_INVALID;

    return ZPACK_OK;
}

int zpack_read_data_header(FILE* fp)
{
    // files data begins right after header
    if (ZPACK_FSEEK(fp, ZPACK_HEADER_SIZE, SEEK_SET) != 0)
        return ZPACK_ERROR_SEEK_FAILED;

    zpack_u8 buffer[ZPACK_SIGNATURE_SIZE];
    if (!ZPACK_FREAD(buffer, ZPACK_SIGNATURE_SIZE, 1, fp))
        return ZPACK_ERROR_READ_FAILED;

    return zpack_read_data_header_memory(buffer);
}

int zpack_read_eocdr_memory(const zpack_u8* buffer, zpack_u64* cdr_offset)
{
    if (!ZPACK_VERIFY_SIGNATURE(buffer, ZPACK_EOCDR_SIGNATURE))
        return ZPACK_ERROR_SIGNATURE_INVALID;

    *cdr_offset = ZPACK_READ_LE64(buffer + 4);
    return ZPACK_OK;
}

int zpack_read_eocdr(FILE* fp, zpack_u64 eocdr_offset, zpack_u64* cdr_offset)
{
    if (ZPACK_FSEEK(fp, eocdr_offset, SEEK_SET) != 0)
        return ZPACK_ERROR_SEEK_FAILED;

    zpack_u8 buffer[ZPACK_EOCDR_SIZE];
    if (!ZPACK_FREAD(buffer, ZPACK_EOCDR_SIZE, 1, fp))
        return ZPACK_ERROR_READ_FAILED;

    return zpack_read_eocdr_memory(buffer, cdr_offset);
}

int zpack_read_cdr_header_memory(const zpack_u8* buffer, zpack_u64* count, zpack_u64* block_size)
{
    if (!ZPACK_VERIFY_SIGNATURE(buffer, ZPACK_CDR_SIGNATURE))
        return ZPACK_ERROR_SIGNATURE_INVALID;
    
    *count = ZPACK_READ_LE64(buffer + 4);
    *block_size = ZPACK_READ_LE64(buffer + 12);
    return ZPACK_OK;
}

int zpack_read_file_entry_memory(const zpack_u8* buffer, zpack_file_entry* entry, size_t* entry_size)
{
    // filename
    zpack_u16 filename_len = ZPACK_READ_LE16(buffer);
    entry->filename = (char*)malloc(sizeof(char) * ((zpack_u32)filename_len + 1)); // for null terminator
    if (entry->filename == NULL) return ZPACK_ERROR_MALLOC_FAILED;
    memcpy(entry->filename, buffer + 2, filename_len);
    // null terminator
    entry->filename[filename_len] = '\0';

    // advance ptr for convenience
    buffer += 2 + filename_len;

    // fixed fields
    entry->offset      = ZPACK_READ_LE64(buffer);
    entry->comp_size   = ZPACK_READ_LE64(buffer + 8);
    entry->uncomp_size = ZPACK_READ_LE64(buffer + 16);
    entry->hash        = ZPACK_READ_LE64(buffer + 24);
    entry->comp_method = ZPACK_READ_LE8 (buffer + 32);

    *entry_size = ZPACK_FILE_ENTRY_FIXED_SIZE + filename_len;
    return ZPACK_OK;
}

int zpack_read_file_entries_memory(const zpack_u8* buffer, zpack_file_entry** entries, zpack_u64 count, zpack_u64 block_size,
                                   zpack_u64* total_cs, zpack_u64* total_us)
{
    *entries = (zpack_file_entry*)realloc(*entries, sizeof(zpack_file_entry) * count);
    if (*entries == NULL)
        return ZPACK_ERROR_MALLOC_FAILED;

    // read file entries
    int ret;
    size_t entry_size;
    for (zpack_u64 i = 0; i < count; ++i)
    {
        zpack_file_entry* entry = *entries + i;
        if ((ret = zpack_read_file_entry_memory(buffer, entry, &entry_size)))
            return ret;
        *total_cs += entry->comp_size;
        *total_us += entry->uncomp_size;

        // continue to next entry
        buffer += entry_size;
    }

    return ZPACK_OK;
}

int zpack_read_cdr_memory(const zpack_u8* buffer, size_t size_left, zpack_file_entry** entries, zpack_u64* count,
                          zpack_u64* total_cs, zpack_u64* total_us)
{
    // read the header
    int ret;
    zpack_u64 block_size;
    if ((ret = zpack_read_cdr_header_memory(buffer, count, &block_size)))
        return ret;

    // block size check
    if (ZPACK_CDR_HEADER_SIZE + block_size > size_left)
        return ZPACK_ERROR_BLOCK_SIZE_INVALID;

    // empty archive
    if (*count == 0) return ZPACK_OK;

    // read file entries
    return zpack_read_file_entries_memory(buffer + ZPACK_CDR_HEADER_SIZE, entries, *count, block_size, total_cs, total_us);
}

int zpack_read_cdr(FILE* fp, zpack_u64 cdr_offset, zpack_file_entry** entries, zpack_u64* count, 
                   zpack_u64* total_cs, zpack_u64* total_us)
{
    if (ZPACK_FSEEK(fp, cdr_offset, SEEK_SET) != 0)
        return ZPACK_ERROR_SEEK_FAILED;

    // read the header
    zpack_u8 buffer[ZPACK_CDR_HEADER_SIZE];
    if (!ZPACK_FREAD(buffer, ZPACK_CDR_HEADER_SIZE, 1, fp))
        return ZPACK_ERROR_READ_FAILED;

    int ret;
    zpack_u64 block_size;
    if ((ret = zpack_read_cdr_header_memory(buffer, count, &block_size)))
        return ret;

    // empty archive
    if (*count == 0) return ZPACK_OK;

    // file entries buffer
    zpack_u8* fe_buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * block_size);
    if (fe_buffer == NULL)
        return ZPACK_ERROR_MALLOC_FAILED;

    // read and parse file entries
    if (!ZPACK_FREAD(fe_buffer, block_size, 1, fp))
        return ZPACK_ERROR_READ_FAILED;
    ret = zpack_read_file_entries_memory(fe_buffer, entries, *count, block_size, total_cs, total_us);

    free(fe_buffer);
    return ZPACK_OK;
}

int zpack_read_archive_memory(zpack_reader* reader)
{
    if (!reader->buffer) return ZPACK_ERROR_ARCHIVE_NOT_LOADED;

    // read sections
    int ret;
    const zpack_u8* p = reader->buffer;

    // header
    if ((ret = zpack_read_header_memory(p, &reader->version)))
        return ret;

    // files data signature
    p += ZPACK_HEADER_SIZE;
    if ((ret = zpack_read_data_header_memory(p)))
        return ret;

    // eocdr
    reader->eocdr_offset = reader->file_size - ZPACK_EOCDR_SIZE;
    p = reader->buffer + reader->eocdr_offset;
    if ((ret = zpack_read_eocdr_memory(p, &reader->cdr_offset)))
        return ret;

    // cdr
    p = reader->buffer + reader->cdr_offset;
    if ((ret = zpack_read_cdr_memory(p, reader->file_size - reader->cdr_offset, &reader->file_entries, &reader->file_count,
                                     &reader->comp_size, &reader->uncomp_size)))
        return ret;

    // all good
    return ZPACK_OK; 
}

int zpack_read_archive(zpack_reader* reader)
{
    if (!reader->file) return ZPACK_ERROR_ARCHIVE_NOT_LOADED;

    // get size
    if (ZPACK_FSEEK(reader->file, 0, SEEK_END) != 0)
        return ZPACK_ERROR_SEEK_FAILED;
    
    reader->file_size = ZPACK_FTELL(reader->file);

    // read sections
    int ret;

    // header
    if ((ret = zpack_read_header(reader->file, &reader->version)))
        return ret;

    // files data signature
    if ((ret = zpack_read_data_header(reader->file)))
        return ret;

    // eocdr
    reader->eocdr_offset = reader->file_size - ZPACK_EOCDR_SIZE;
    if ((ret = zpack_read_eocdr(reader->file, reader->eocdr_offset, &reader->cdr_offset)))
        return ret;

    // cdr
    if ((ret = zpack_read_cdr(reader->file, reader->cdr_offset, &reader->file_entries, 
                              &reader->file_count, &reader->comp_size, &reader->uncomp_size)))
        return ret;

    // all good
    return ZPACK_OK;
}

int zpack_get_file_entry(zpack_reader* reader, const char* filename, zpack_file_entry** entry)
{
    for (int i = 0; i < reader->file_count; ++i)
    {
        if (strcmp(reader->file_entries[i].filename, filename) == 0)
        {
            *entry = reader->file_entries + i;
            return ZPACK_OK;
        }
    }

    return ZPACK_ERROR_FILE_NOT_FOUND;
}

int zpack_read_raw_file(zpack_reader* reader, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size)
{
    // offset check
    if (entry->offset >= reader->file_size)
        return ZPACK_ERROR_FILE_OFFSET_INVALID;

    // shrink read size to max size
    zpack_u64 read_size = ZPACK_MIN(max_size, entry->comp_size);
    if (reader->file)
    {
        if (ZPACK_FSEEK(reader->file, entry->offset, SEEK_SET) != 0)
            return ZPACK_ERROR_SEEK_FAILED;
        
        if (ZPACK_FREAD(buffer, 1, read_size, reader->file) != read_size)
            return ZPACK_ERROR_READ_FAILED;
    }
    else if (reader->buffer)
    {
        // Note: This function just copies data from the already loaded buffer.
        // It might be better to just read straight from the buffer itself if possible.
        memcpy(buffer, reader->buffer + entry->offset, read_size);
    }
    else
        return ZPACK_ERROR_ARCHIVE_NOT_LOADED;

    return ZPACK_OK;
}

int zpack_read_file(zpack_reader* reader, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size)
{
    if (entry->comp_size == 0) return ZPACK_OK;
    if (max_size < entry->uncomp_size) return ZPACK_ERROR_BUFFER_TOO_SMALL;

    // read the compressed data
    zpack_u8* comp_data;
    if (reader->file)
    {
        comp_data = (zpack_u8*)malloc(sizeof(zpack_u8) * entry->comp_size);
        if (comp_data == NULL) return ZPACK_ERROR_MALLOC_FAILED;
        int ret;
        if ((ret = zpack_read_raw_file(reader, entry, comp_data, entry->comp_size)))
            return ret;
    }
    else if (reader->buffer)
        comp_data = reader->buffer + entry->offset;
    else
        return ZPACK_ERROR_ARCHIVE_NOT_LOADED;
    
    switch (entry->comp_method)
    {
    case ZPACK_COMPRESSION_ZSTD:
    #ifndef ZPACK_DISABLE_ZSTD
        // create the decompression context if needed
        if (!reader->zstd_dctx)
        {
            reader->zstd_dctx = ZSTD_createDCtx();
            if (reader->zstd_dctx == NULL)
            {
                if (reader->file) free(comp_data);
                return ZPACK_ERROR_MALLOC_FAILED;
            }
        }
        
        // and decompress the file
        reader->last_return = ZSTD_decompressDCtx(reader->zstd_dctx, buffer, max_size, comp_data, entry->comp_size);
        if (reader->file) free(comp_data);

        // check for errors
        if (ZSTD_isError(reader->last_return))
        {
            // ctx needs to be freed after an error
            ZSTD_freeDCtx(reader->zstd_dctx);
            reader->zstd_dctx = NULL;

            int ret = zpack_get_zstd_result(reader->last_return);
            return (ret != -1) ? ret : ZPACK_ERROR_DECOMPRESS_FAILED;
        }

        break;
    #else
        if (reader->file) free(comp_data);
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif
    
    case ZPACK_COMPRESSION_LZ4:
    #ifndef ZPACK_DISABLE_LZ4
        if (!reader->lz4_dctx)
            LZ4F_createDecompressionContext((LZ4F_dctx**)&reader->lz4_dctx, LZ4F_VERSION);
        
        void* dst = buffer;
        const void* src = comp_data;

        size_t avail_out = max_size;
        size_t avail_in = entry->comp_size;

        // decompress the file
        // even though this is meant to one-shot decompress everything,
        // LZ4F_decompress might not read the entire thing in one go(?)
        while (avail_out > 0 && avail_in > 0)
        {
            size_t dst_size = avail_out;
            size_t src_size = avail_in;

            reader->last_return = LZ4F_decompress(reader->lz4_dctx, dst, &dst_size, src, &src_size, NULL);

            if (LZ4F_isError(reader->last_return))
            {
                if (reader->file) free(comp_data);
                // ctx needs to be freed after an error
                LZ4F_freeDecompressionContext(reader->lz4_dctx);
                reader->lz4_dctx = NULL;
                return ZPACK_ERROR_DECOMPRESS_FAILED;
            }

            if (src_size)
            {
                src += src_size;
                avail_in -= src_size;
            }

            if (dst_size)
            {
                dst += dst_size;
                avail_out -= dst_size;
            }
        }
        if (reader->file) free(comp_data);

        // check if the decompression is complete
        if (reader->last_return != 0)
        {
            if (avail_out > 0)
                return ZPACK_ERROR_FILE_INCOMPLETE;
            else
                return ZPACK_ERROR_BUFFER_TOO_SMALL;
        }
        
        break;
    #else
        if (reader->file) free(comp_data);
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif

    }

    // verify hash
    XXH64_hash_t hash = XXH3_64bits(buffer, entry->uncomp_size);
    if (hash != entry->hash)
        return ZPACK_ERROR_FILE_HASH_MISMATCH;

    return ZPACK_OK;
}

int zpack_read_file_stream(zpack_reader* reader, zpack_file_entry* entry, zpack_stream* stream, size_t read_size)
{
    if (entry->comp_size == 0) return ZPACK_OK;

    return ZPACK_OK;
}

int zpack_init_reader_memory(zpack_reader* reader, const zpack_u8* buffer, size_t size)
{
    reader->buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * size);
    if (reader->buffer == NULL) return ZPACK_ERROR_MALLOC_FAILED;
    memcpy(reader->buffer, buffer, size);

    reader->file_size = size;
    reader->buffer_shared = ZPACK_FALSE;

    return zpack_read_archive_memory(reader);
}

int zpack_init_reader_memory_shared(zpack_reader* reader, zpack_u8* buffer, size_t size)
{
    reader->buffer = buffer;
    reader->file_size = size;
    reader->buffer_shared = ZPACK_TRUE;

    return zpack_read_archive_memory(reader);
}

int zpack_init_reader(zpack_reader* reader, const char* path)
{
    FILE* fp = ZPACK_FOPEN(path, "rb");
    if (!fp) return ZPACK_ERROR_OPEN_FAILED;
    if (reader->file) ZPACK_FCLOSE(reader->file);
    reader->file = fp;

    return zpack_read_archive(reader);
}

void zpack_close_reader(zpack_reader* reader)
{
    if (reader->file)
        ZPACK_FCLOSE(reader->file);

    if (!reader->buffer_shared)
        free(reader->buffer);

    if (reader->file_entries)
    {
        for (zpack_u64 i = 0; i < reader->file_count; ++i)
            free(reader->file_entries[i].filename);

        free(reader->file_entries);
    }

#ifndef ZPACK_DISABLE_ZSTD
    ZSTD_freeDCtx(reader->zstd_dctx);
#endif

#ifndef ZPACK_DISABLE_LZ4
    LZ4F_freeDecompressionContext(reader->lz4_dctx);
#endif

    memset(reader, 0, sizeof(zpack_reader));
}