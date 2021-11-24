#include "zpack.h"
#include "zpack_common.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <xxhash.h>

#ifndef ZPACK_DISABLE_ZSTD
#include <zstd.h>
#include <zstd_errors.h>
#endif

#ifndef ZPACK_DISABLE_LZ4
#include <lz4frame.h>
#endif

#define ZPACK_CHECK_DCTX_ZSTD(dctx, reader) \
    if (!dctx) \
    { \
        if (!reader->zstd_dctx) \
            reader->zstd_dctx = ZSTD_createDCtx(); \
        dctx = reader->zstd_dctx; \
    }

#define ZPACK_CHECK_DCTX_LZ4(dctx, reader) \
    if (!dctx) \
    { \
        if (!reader->lz4f_dctx) \
            LZ4F_createDecompressionContext((LZ4F_dctx**)&reader->lz4f_dctx, LZ4F_VERSION); \
        dctx = reader->lz4f_dctx; \
    }

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

int zpack_read_file_entry_memory(const zpack_u8* buffer, zpack_u64* size_left, zpack_file_entry* entry, size_t* entry_size)
{
    // filename
    zpack_u16 filename_len = ZPACK_READ_LE16(buffer);
    *entry_size = ZPACK_FILE_ENTRY_FIXED_SIZE + filename_len;
    if (*entry_size > *size_left)
        return ZPACK_ERROR_BLOCK_SIZE_INVALID;
    *size_left -= *entry_size;

    entry->filename = (char*)malloc(sizeof(char) * ((zpack_u32)filename_len + 1)); // for null terminator
    if (entry->filename == NULL) return ZPACK_ERROR_MALLOC_FAILED;
    memcpy(entry->filename, buffer + 2, filename_len);
    // null terminator
    entry->filename[filename_len] = '\0';

    buffer += 2 + filename_len;

    // fixed fields
    entry->offset      = ZPACK_READ_LE64(buffer);
    entry->comp_size   = ZPACK_READ_LE64(buffer + 8);
    entry->uncomp_size = ZPACK_READ_LE64(buffer + 16);
    entry->hash        = ZPACK_READ_LE64(buffer + 24);
    entry->comp_method = ZPACK_READ_LE8 (buffer + 32);

    return ZPACK_OK;
}

int zpack_read_file_entries_memory(const zpack_u8* buffer, zpack_file_entry** entries, zpack_u64 header_count,
                                   zpack_u64 block_size, zpack_u64* count, zpack_u64* total_cs, zpack_u64* total_us)
{
    // basic fixed size check
    if (header_count * ZPACK_FILE_ENTRY_FIXED_SIZE > block_size)
        return ZPACK_ERROR_BLOCK_SIZE_INVALID;

    zpack_u64 eb_size = sizeof(zpack_file_entry) * header_count;
    if (eb_size > SIZE_MAX) return ZPACK_ERROR_MALLOC_FAILED;
    *entries = (zpack_file_entry*)realloc(*entries, eb_size);
    if (*entries == NULL) return ZPACK_ERROR_MALLOC_FAILED;
    memset(*entries, 0, eb_size);

    // read file entries
    int ret;
    size_t entry_size;
    for (zpack_u64 i = 0; i < header_count; ++i)
    {
        zpack_file_entry* entry = *entries + i;
        if ((ret = zpack_read_file_entry_memory(buffer, &block_size, entry, &entry_size)))
            return ret;
        ++(*count);
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
    zpack_u64 file_count;
    if ((ret = zpack_read_cdr_header_memory(buffer, &file_count, &block_size)))
        return ret;

    // block size check
    if (ZPACK_CDR_HEADER_SIZE + block_size > size_left)
        return ZPACK_ERROR_BLOCK_SIZE_INVALID;

    // empty archive
    if (file_count == 0) return ZPACK_OK;

    // read file entries
    return zpack_read_file_entries_memory(buffer + ZPACK_CDR_HEADER_SIZE, entries, file_count, block_size, count, total_cs, total_us);
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
    zpack_u64 file_count;
    if ((ret = zpack_read_cdr_header_memory(buffer, &file_count, &block_size)))
        return ret;
    if (block_size > SIZE_MAX)
        return ZPACK_ERROR_MALLOC_FAILED;

    // empty archive
    if (file_count == 0) return ZPACK_OK;

    // file entries buffer
    zpack_u8* fe_buffer = (zpack_u8*)malloc(sizeof(zpack_u8) * block_size);
    if (fe_buffer == NULL)
        return ZPACK_ERROR_MALLOC_FAILED;

    // read and parse file entries
    if (!ZPACK_FREAD(fe_buffer, block_size, 1, fp))
        return ZPACK_ERROR_READ_FAILED;
    ret = zpack_read_file_entries_memory(fe_buffer, entries, file_count, block_size, count, total_cs, total_us);

    free(fe_buffer);
    return ret;
}

int zpack_read_archive_memory(zpack_reader* reader)
{
    if (!reader->buffer) return ZPACK_ERROR_ARCHIVE_NOT_LOADED;
    if (reader->file_size < ZPACK_MINIMUM_ARCHIVE_SIZE) return ZPACK_ERROR_FILE_TOO_SMALL;

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
    
    if (reader->cdr_offset >= reader->file_size)
        return ZPACK_ERROR_READ_FAILED;

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
    
    if (!reader->file_size) reader->file_size = ZPACK_FTELL(reader->file);
    if (reader->file_size < ZPACK_MINIMUM_ARCHIVE_SIZE) return ZPACK_ERROR_FILE_TOO_SMALL;

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

int zpack_read_raw_file(zpack_reader* reader, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size)
{
    // offset check
    if (entry->offset + entry->comp_size > reader->file_size)
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

int zpack_read_file(zpack_reader* reader, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size, void* dctx)
{
    if (entry->comp_size == 0) return ZPACK_OK;
    if (max_size < entry->uncomp_size) return ZPACK_ERROR_BUFFER_TOO_SMALL;

    if (entry->offset + entry->comp_size >= reader->file_size)
        return ZPACK_ERROR_FILE_OFFSET_INVALID;

    // read the compressed data
    zpack_u8* comp_data;
    if (reader->file)
    {
        if (entry->comp_size > SIZE_MAX) return ZPACK_ERROR_MALLOC_FAILED;
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
    case ZPACK_COMPRESSION_NONE:
        // reading less than the compressed size is allowed
        if (entry->uncomp_size > entry->comp_size)
        {
            if (reader->file) free(comp_data);
            return ZPACK_ERROR_FILE_SIZE_INVALID;
        }

        if (max_size < entry->uncomp_size)
        {
            if (reader->file) free(comp_data);
            return ZPACK_ERROR_BUFFER_TOO_SMALL;
        }

        memcpy(buffer, comp_data, entry->uncomp_size);
        if (reader->file) free(comp_data);
        break;

    case ZPACK_COMPRESSION_ZSTD:
    #ifndef ZPACK_DISABLE_ZSTD
        ZPACK_CHECK_DCTX_ZSTD(dctx, reader);
        if (!dctx)
        {
            if (reader->file) free(comp_data);
            return ZPACK_ERROR_MALLOC_FAILED;
        }
        
        // and decompress the file
        reader->last_return = ZSTD_decompressDCtx(dctx, buffer, max_size, comp_data, entry->comp_size);
        if (reader->file) free(comp_data);

        // check for errors
        if (ZSTD_isError(reader->last_return))
        {
            ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
            return ZPACK_ERROR_DECOMPRESS_FAILED;
        }

        break;
    #else
        if (reader->file) free(comp_data);
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif
    
    case ZPACK_COMPRESSION_LZ4:
    #ifndef ZPACK_DISABLE_LZ4
    {
        ZPACK_CHECK_DCTX_LZ4(dctx, reader);
        if (!dctx)
        {
            if (reader->file) free(comp_data);
            return ZPACK_ERROR_MALLOC_FAILED;
        }
        
        zpack_u8* dst = buffer;
        const zpack_u8* src = comp_data;

        size_t avail_out = max_size;
        size_t avail_in  = entry->comp_size;

        // decompress the file
        size_t dst_size, src_size;
        while (avail_out > 0 && avail_in > 0)
        {
            dst_size = avail_out;
            src_size = avail_in;

            reader->last_return = LZ4F_decompress(dctx, dst, &dst_size, src, &src_size, NULL);

            if (LZ4F_isError(reader->last_return))
            {
                if (reader->file) free(comp_data);
                LZ4F_resetDecompressionContext(dctx);
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
            LZ4F_resetDecompressionContext(dctx);
            if (avail_out > 0)
                return ZPACK_ERROR_FILE_INCOMPLETE;
            else
                return ZPACK_ERROR_BUFFER_TOO_SMALL;
        }
        
        break;
    }
    #else
        if (reader->file) free(comp_data);
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif

    default:
        if (reader->file) free(comp_data);
        return ZPACK_ERROR_COMP_METHOD_INVALID;

    }

    // verify hash
    XXH64_hash_t hash = XXH3_64bits(buffer, entry->uncomp_size);
    if (hash != entry->hash)
        return ZPACK_ERROR_FILE_HASH_MISMATCH;

    return ZPACK_OK;
}

int zpack_read_raw_file_stream(zpack_reader* reader, zpack_file_entry* entry, zpack_stream* stream, size_t* in_size)
{
    if (entry->comp_size == 0)
        return ZPACK_OK;

    if (entry->offset + entry->comp_size > reader->file_size)
        return ZPACK_ERROR_FILE_OFFSET_INVALID;

    if (!stream->next_in || !stream->avail_in || (stream->total_in > entry->comp_size))
        return ZPACK_ERROR_STREAM_INVALID;

    size_t read_size = ZPACK_MIN(stream->avail_in, entry->comp_size - stream->total_in);
    if (read_size == 0) return ZPACK_OK;
    size_t offset = entry->offset + stream->total_in;

    // read the compressed data
    if (reader->file)
    {
        if (ZPACK_FSEEK(reader->file, offset, SEEK_SET) != 0)
            return ZPACK_ERROR_SEEK_FAILED;
        
        if (ZPACK_FREAD(stream->next_in, 1, read_size, reader->file) != read_size)
            return ZPACK_ERROR_READ_FAILED;
    }
    else if (reader->buffer)
        memcpy(stream->next_in, reader->buffer + offset, read_size);
    else
        return ZPACK_ERROR_ARCHIVE_NOT_LOADED;
    
    stream->next_in  += read_size;
    stream->avail_in -= read_size;
    stream->total_in += read_size;
    *in_size = read_size;

    return ZPACK_OK;
}

#define ZPACK_ADVANCE_STREAM_OUT(stream, size) \
    stream->next_out  += size; \
    stream->avail_out -= size; \
    stream->total_out += size;

int zpack_read_file_stream(zpack_reader* reader, zpack_file_entry* entry, zpack_stream* stream, void* dctx)
{
    if (entry->comp_size == 0 || ZPACK_READ_STREAM_DONE(stream, entry))
        return ZPACK_OK;

    if (!stream->next_out || !stream->avail_out)
        return ZPACK_ERROR_STREAM_INVALID;
    
    // reset xxh3 state at start
    if (stream->total_in == 0)
        XXH3_64bits_reset(stream->xxh3_state);

    // set src/apply read back
    zpack_u8* src = stream->next_in;
    size_t in_size = stream->read_back;

    if (stream->read_back)
    {
        stream->next_in += stream->read_back;
        stream->avail_in -= stream->read_back;
        stream->read_back = 0;
    }

    // check if everything is already read
    int ret;
    if (stream->total_in < entry->comp_size)
    {
        // then read the compressed data
        size_t tmp;
        if ((ret = zpack_read_raw_file_stream(reader, entry, stream, &tmp)))
            return ret;
        in_size += tmp;
    }

    // decompress it
    switch (entry->comp_method)
    {
    case ZPACK_COMPRESSION_NONE:
    {
        size_t write_size = ZPACK_MIN(stream->avail_out, in_size);
        memcpy(stream->next_out, src, write_size);
        XXH3_64bits_update(stream->xxh3_state, stream->next_out, write_size);

        ZPACK_ADVANCE_STREAM_OUT(stream, write_size);
        stream->read_back = in_size - write_size;
        break;
    }

    case ZPACK_COMPRESSION_ZSTD:
    #ifndef ZPACK_DISABLE_ZSTD
    {
        ZPACK_CHECK_DCTX_ZSTD(dctx, reader);
        if (!dctx) return ZPACK_ERROR_MALLOC_FAILED;

        ZSTD_outBuffer out = { stream->next_out, stream->avail_out, 0 };
        ZSTD_inBuffer  in  = { src, in_size, 0 };

        reader->last_return = ZSTD_decompressStream(dctx, &out, &in);
        if (ZSTD_isError(reader->last_return))
        {
            ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
            return ZPACK_ERROR_DECOMPRESS_FAILED;
        }

        XXH3_64bits_update(stream->xxh3_state, stream->next_out, out.pos);
        ZPACK_ADVANCE_STREAM_OUT(stream, out.pos);
        stream->read_back = in.size - in.pos;
        break;
    }
    #else
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif

    case ZPACK_COMPRESSION_LZ4:
    #ifndef ZPACK_DISABLE_LZ4
    {
        ZPACK_CHECK_DCTX_LZ4(dctx, reader);
        if (!dctx) return ZPACK_ERROR_MALLOC_FAILED;

        size_t dst_size, src_size;
        dst_size = stream->avail_out;
        src_size = in_size;

        reader->last_return = LZ4F_decompress(dctx, stream->next_out, &dst_size,
                                              src, &src_size, NULL);

        if (LZ4F_isError(reader->last_return))
        {
            LZ4F_resetDecompressionContext(dctx);
            return ZPACK_ERROR_DECOMPRESS_FAILED;
        }

        if (dst_size)
        {
            XXH3_64bits_update(stream->xxh3_state, stream->next_out, dst_size);
            ZPACK_ADVANCE_STREAM_OUT(stream, dst_size);
        }

        if (src_size)
        {
            src     += src_size;
            in_size -= src_size;
        }

        stream->read_back = in_size;
        break;
    }
    #else
        return ZPACK_ERROR_NOT_AVAILABLE;
    #endif
    
    default:
        return ZPACK_ERROR_COMP_METHOD_INVALID;
    }

    // check if the entire file has been read and decompressed
    if (ZPACK_READ_STREAM_DONE(stream, entry))
    {
        // verify hash
        zpack_u64 hash = XXH3_64bits_digest(stream->xxh3_state);
        if (entry->hash != hash)
            return ZPACK_ERROR_FILE_HASH_MISMATCH;
    }

    return ZPACK_OK;
}

int zpack_init_reader(zpack_reader* reader, const char* path)
{
    FILE* fp = ZPACK_FOPEN(path, "rb");
    if (!fp) return ZPACK_ERROR_OPEN_FAILED;
    if (reader->file) ZPACK_FCLOSE(reader->file);
    reader->file = fp;

    return zpack_read_archive(reader);
}

int zpack_init_reader_cfile(zpack_reader* reader, FILE* fp)
{
    reader->file = fp;
    return zpack_read_archive(reader);
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

void zpack_reset_reader_dctx(zpack_reader* reader)
{
#ifndef ZPACK_DISABLE_ZSTD
    if (reader->zstd_dctx)
        ZSTD_DCtx_reset(reader->zstd_dctx, ZSTD_reset_session_only);
#endif

#ifndef ZPACK_DISABLE_LZ4
    if (reader->lz4f_dctx)
        LZ4F_resetDecompressionContext(reader->lz4f_dctx);
#endif
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
    LZ4F_freeDecompressionContext(reader->lz4f_dctx);
#endif

    memset(reader, 0, sizeof(zpack_reader));
}

size_t zpack_get_dstream_in_size(zpack_compression_method method)
{
    switch (method)
    {
    // This will fallthrough to the largest size available
    case ZPACK_COMPRESSION_NONE:

    #ifndef ZPACK_DISABLE_ZSTD
    case ZPACK_COMPRESSION_ZSTD:
        return ZSTD_DStreamInSize();
    #endif

    #ifndef ZPACK_DISABLE_LZ4
    case ZPACK_COMPRESSION_LZ4:
        return LZ4F_compressBound(0, NULL);
    #endif

    default: return 0;
    }
};

size_t zpack_get_dstream_out_size(zpack_compression_method method)
{
    switch (method)
    {
    case ZPACK_COMPRESSION_NONE:

    #ifndef ZPACK_DISABLE_ZSTD
    case ZPACK_COMPRESSION_ZSTD:
        return ZSTD_DStreamOutSize();
    #endif

    #ifndef ZPACK_DISABLE_LZ4
    case ZPACK_COMPRESSION_LZ4:
        return (1 << 16); // 64kb
    #endif

    default: return 0;
    }
}

zpack_file_entry* zpack_get_file_entry(const char* filename, zpack_file_entry* file_entries, zpack_u64 file_count)
{
    for (int i = 0; i < file_count; ++i)
    {
        if (strcmp(file_entries[i].filename, filename) == 0)
            return file_entries + i;
    }

    return NULL;
}