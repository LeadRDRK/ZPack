#include "zpack.h"
#include "zpack_common.h"
#include <zstd.h>
#include <xxhash.h>
#include <string.h>
#include <stdlib.h>

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
    if (entry->filename == NULL) return ZPACK_ERROR_OUT_OF_MEMORY;
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
        return ZPACK_ERROR_OUT_OF_MEMORY;

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
        return ZPACK_ERROR_OUT_OF_MEMORY;

    // read and parse file entries
    if (!ZPACK_FREAD(fe_buffer, block_size, 1, fp))
        return ZPACK_ERROR_READ_FAILED;
    ret = zpack_read_file_entries_memory(fe_buffer, entries, *count, block_size, total_cs, total_us);

    free(fe_buffer);
    return ZPACK_OK;
}

int zpack_read_archive_memory(zpack_archive *archive)
{
    if (!archive->buffer) return ZPACK_ERROR_ARCHIVE_NOT_LOADED;

    // read sections
    int ret;
    const zpack_u8* p = archive->buffer;

    // header
    if ((ret = zpack_read_header_memory(p, &archive->version)))
        return ret;

    // files data signature
    p += ZPACK_HEADER_SIZE;
    if ((ret = zpack_read_data_header_memory(p)))
        return ret;

    // eocdr
    archive->eocdr_offset = archive->file_size - ZPACK_EOCDR_SIZE;
    p = archive->buffer + archive->eocdr_offset;
    if ((ret = zpack_read_eocdr_memory(p, &archive->cdr_offset)))
        return ret;

    // cdr
    p = archive->buffer + archive->cdr_offset;
    if ((ret = zpack_read_cdr_memory(p, archive->file_size - archive->cdr_offset, &archive->file_entries, &archive->file_count,
                                     &archive->comp_size, &archive->uncomp_size)))
        return ret;

    // all good
    return ZPACK_OK; 
}

int zpack_read_archive(zpack_archive* archive)
{
    if (!archive->file) return ZPACK_ERROR_ARCHIVE_NOT_LOADED;

    // get size
    if (ZPACK_FSEEK(archive->file, 0, SEEK_END) != 0)
        return ZPACK_ERROR_SEEK_FAILED;
    
    archive->file_size = ZPACK_FTELL(archive->file);

    // read sections
    int ret;

    // header
    if ((ret = zpack_read_header(archive->file, &archive->version)))
        return ret;

    // files data signature
    if ((ret = zpack_read_data_header(archive->file)))
        return ret;

    // eocdr
    archive->eocdr_offset = archive->file_size - ZPACK_EOCDR_SIZE;
    if ((ret = zpack_read_eocdr(archive->file, archive->eocdr_offset, &archive->cdr_offset)))
        return ret;

    // cdr
    if ((ret = zpack_read_cdr(archive->file, archive->cdr_offset, &archive->file_entries, 
                              &archive->file_count, &archive->comp_size, &archive->uncomp_size)))
        return ret;

    // all good
    return ZPACK_OK;
}

int zpack_archive_get_file_entry(zpack_archive* archive, const char* filename, zpack_file_entry** entry)
{
    for (int i = 0; i < archive->file_count; ++i)
    {
        if (strcmp(archive->file_entries[i].filename, filename) == 0)
        {
            *entry = archive->file_entries + i;
            return ZPACK_OK;
        }
    }

    return ZPACK_ERROR_FILE_NOT_FOUND;
}

int zpack_archive_read_raw_file(zpack_archive* archive, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size)
{
    // offset check
    if (entry->offset >= archive->file_size)
        return ZPACK_ERROR_FILE_OFFSET_INVALID;

    // shrink read size to max size
    zpack_u64 read_size = ZPACK_MIN(max_size, entry->comp_size);
    if (archive->file)
    {
        if (ZPACK_FSEEK(archive->file, entry->offset, SEEK_SET) != 0)
            return ZPACK_ERROR_SEEK_FAILED;
        
        if (ZPACK_FREAD(buffer, 1, read_size, archive->file) != read_size)
            return ZPACK_ERROR_READ_FAILED;
    }
    else if (archive->buffer)
    {
        // Note: This function just copies data from the already loaded buffer.
        // It might be better to just read straight from the buffer itself if possible.
        memcpy(buffer, archive->buffer + entry->offset, read_size);
    }
    else
        return ZPACK_ERROR_ARCHIVE_NOT_LOADED;

    return ZPACK_OK;
}

int zpack_archive_read_file(zpack_archive* archive, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size)
{
    if (max_size < entry->uncomp_size) return ZPACK_ERROR_BUFFER_TOO_SMALL;

    // read the compressed data
    zpack_u8* comp_data;
    if (archive->file)
    {
        comp_data = (zpack_u8*)malloc(sizeof(zpack_u8) * entry->comp_size);
        if (comp_data == NULL) return ZPACK_ERROR_OUT_OF_MEMORY;
        int ret;
        if ((ret = zpack_archive_read_raw_file(archive, entry, comp_data, entry->comp_size)))
            return ret;
    }
    else if (archive->buffer)
        comp_data = archive->buffer + entry->offset;
    else
        return ZPACK_ERROR_ARCHIVE_NOT_LOADED;
    
    switch (entry->comp_method)
    {
    case ZPACK_COMPRESSION_ZSTD:
        // create the decompression context if needed
        if (!archive->zstd_dctx)
            archive->zstd_dctx = ZSTD_createDCtx();
        
        // and decompress the file
        archive->last_return = ZSTD_decompressDCtx(archive->zstd_dctx, buffer, max_size, comp_data, entry->comp_size);
        if (archive->file) free(comp_data);

        // check for errors
        if (ZSTD_isError(archive->last_return))
            return ZPACK_ERROR_DECOMPRESS_FAILED;

        break;
    }

    // verify hash
    XXH64_hash_t hash = XXH3_64bits(buffer, entry->uncomp_size);
    if (hash != entry->hash)
        return ZPACK_ERROR_FILE_HASH_MISMATCH;

    return ZPACK_OK;
}

int zpack_open_archive_memory(zpack_archive* archive, const zpack_u8* buffer, size_t size)
{
    archive->buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * size);
    if (archive->buffer == NULL) return ZPACK_ERROR_OUT_OF_MEMORY;
    memcpy(archive->buffer, buffer, size);

    archive->file_size = size;
    archive->buffer_shared = ZPACK_FALSE;

    return zpack_read_archive_memory(archive);
}

int zpack_open_archive_memory_shared(zpack_archive* archive, zpack_u8* buffer, size_t size)
{
    archive->buffer = buffer;
    archive->file_size = size;
    archive->buffer_shared = ZPACK_TRUE;

    return zpack_read_archive_memory(archive);
}

int zpack_open_archive(zpack_archive* archive, const char* path)
{
    FILE* fp = ZPACK_FOPEN(path, "rb");
    if (!fp) return ZPACK_ERROR_OPEN_FAILED;
    if (archive->file) ZPACK_FCLOSE(archive->file);
    archive->file = fp;

    return zpack_read_archive(archive);
}

void zpack_close_archive(zpack_archive* archive)
{
    if (archive->file)
        ZPACK_FCLOSE(archive->file);

    if (!archive->buffer_shared && archive->buffer)
        free(archive->buffer);

    if (archive->file_entries)
    {
        for (zpack_u64 i = 0; i < archive->file_count; ++i)
            free(archive->file_entries[i].filename);

        free(archive->file_entries);
    }

    if (archive->zstd_dctx)
        ZSTD_freeDCtx(archive->zstd_dctx);

    memset(archive, 0, sizeof(zpack_archive));
}