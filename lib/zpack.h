#ifndef __ZPACK_H__
#define __ZPACK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "zpack_export.h"

typedef uint8_t  zpack_u8;
typedef uint16_t zpack_u16;
typedef uint32_t zpack_u32;
typedef uint64_t zpack_u64;

typedef zpack_u8 zpack_bool;
#define ZPACK_FALSE 0
#define ZPACK_TRUE 1

// file format details
#define ZPACK_HEADER_SIGNATURE 0x154b505a // ZPK\x15
#define ZPACK_DATA_SIGNATURE   0x144b505a // ZPK\x14
#define ZPACK_CDR_SIGNATURE    0x134b505a // ZPK\x13
#define ZPACK_EOCDR_SIGNATURE  0x124b505a // ZPK\x12

#define ZPACK_SIGNATURE_SIZE 4
#define ZPACK_HEADER_SIZE 6
#define ZPACK_CDR_HEADER_SIZE 20
#define ZPACK_FILE_ENTRY_FIXED_SIZE 35 // size of fixed fields in file entry
#define ZPACK_EOCDR_SIZE 12

// archive versions supported
#define ZPACK_ARCHIVE_VERSION_MIN 1
#define ZPACK_ARCHIVE_VERSION_MAX 1

// forward decl
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_inBuffer_s ZSTD_inBuffer;
typedef struct ZSTD_outBuffer_s ZSTD_outBuffer;

typedef enum zpack_compression_method_e
{
    ZPACK_COMPRESSION_ZSTD,
    ZPACK_COMPRESSION_LZ4

} zpack_compression_method;

typedef struct zpack_file_entry_s
{
    char*     filename;
    zpack_u64 offset;
    zpack_u64 comp_size;
    zpack_u64 uncomp_size;
    zpack_u64 hash;
    zpack_u8  comp_method;
    
} zpack_file_entry;

typedef enum zpack_action_e
{
    // these enums are listed in order of processing
    ZPACK_ACTION_REMOVE_FILE,
    ZPACK_ACTION_WRITE_FILE,

    ZPACK_ACTION_LAST_ENUM

} zpack_action;

typedef struct zpack_archive_s
{
    zpack_u16 version;
    zpack_file_entry* file_entries;
    zpack_u64 file_count;
    zpack_u64 comp_size;
    zpack_u64 uncomp_size;
    size_t file_size;

    // zstd
    ZSTD_DCtx* zstd_dctx;

    size_t last_return; // last compression library return value

    // offsets
    zpack_u64 cdr_offset;
    zpack_u64 eocdr_offset;

    zpack_u8* buffer;
    zpack_bool buffer_shared;
    FILE* file;

} zpack_archive;

typedef struct zpack_compress_options_s
{
    zpack_compression_method method;
    int level;

} zpack_compress_options;

typedef struct zpack_file_s
{
    char*     filename;
    zpack_u8* buffer;
    zpack_u64 size;
    zpack_compress_options* options;

} zpack_file;

typedef struct zpack_writer_s
{
    zpack_u8* buffer;
    zpack_u64 buffer_capacity;
    FILE* file;
    size_t file_size;

    size_t write_offset;

    zpack_file_entry* file_entries;
    zpack_u64 fe_capacity;
    zpack_u64 file_count;

    // zstd
    ZSTD_CCtx* zstd_cctx;

    size_t last_return; // last compression library return value

    // offsets
    zpack_u64 cdr_offset;
    zpack_u64 eocdr_offset;

} zpack_writer;

typedef struct zpack_streaming_state_s
{
    ZSTD_inBuffer* zstd_in_buffer;
    ZSTD_outBuffer* zstd_out_buffer;

} zpack_streaming_state;

enum zpack_result
{
    ZPACK_OK,                         // No errors
    ZPACK_DONE,                       // (Streaming) decompression is done

    ZPACK_ERROR_ARCHIVE_NOT_LOADED,   // Archive has not been loaded
    ZPACK_ERROR_WRITER_NOT_OPENED,    // Writer has not been opened
    ZPACK_ERROR_OPEN_FAILED,          // Failed to open file
    ZPACK_ERROR_SEEK_FAILED,          // Failed to seek file
    ZPACK_ERROR_SIGNATURE_INVALID,    // Invalid archive signature
    ZPACK_ERROR_READ_FAILED,          // An archive section is invalid
    ZPACK_ERROR_BLOCK_SIZE_INVALID,   // Invalid block size
    ZPACK_ERROR_VERSION_INCOMPATIBLE, // Archive version is not supported
    ZPACK_ERROR_OUT_OF_MEMORY,        // Could not allocate enough memory
    ZPACK_ERROR_FILE_NOT_FOUND,       // Could not find file in archive
    ZPACK_ERROR_BUFFER_TOO_SMALL,     // Buffer size is too small
    ZPACK_ERROR_DECOMPRESS_FAILED,    // Decompression error (check last_return for the compression library's error code)
    ZPACK_ERROR_COMPRESS_FAILED,      // Compression error (check last_return for the compression library's error code)
    ZPACK_ERROR_FILE_HASH_MISMATCH,   // The decompressed file's hash does not match the original file's hash
    ZPACK_ERROR_COMP_METHOD_INVALID,  // Invalid compression method
    ZPACK_ERROR_FILE_OFFSET_INVALID,  // File offset invalid
    ZPACK_ERROR_WRITE_FAILED          // Failed to write data to file

};

// Reading
ZPACK_EXPORT int zpack_read_header_memory(const zpack_u8* buffer, zpack_u16* version);
ZPACK_EXPORT int zpack_read_header(FILE* fp, zpack_u16* version);
ZPACK_EXPORT int zpack_read_data_header_memory(const zpack_u8* buffer);
ZPACK_EXPORT int zpack_read_data_header(FILE* fp);
ZPACK_EXPORT int zpack_read_eocdr_memory(const zpack_u8* buffer, zpack_u64* cdr_offset);
ZPACK_EXPORT int zpack_read_eocdr(FILE* fp, zpack_u64 eocdr_offset, zpack_u64* cdr_offset);
ZPACK_EXPORT int zpack_read_cdr_header_memory(const zpack_u8* buffer, zpack_u64* count, zpack_u64* block_size);
ZPACK_EXPORT int zpack_read_file_entry_memory(const zpack_u8* buffer, zpack_file_entry* entry, size_t* entry_size);
ZPACK_EXPORT int zpack_read_file_entries_memory(const zpack_u8* buffer, zpack_file_entry** entries, zpack_u64 count, zpack_u64 block_size, zpack_u64* total_cs, zpack_u64* total_us);
ZPACK_EXPORT int zpack_read_cdr_memory(const zpack_u8* buffer, size_t size_left, zpack_file_entry** entries, zpack_u64* count, zpack_u64* total_cs, zpack_u64* total_us);
ZPACK_EXPORT int zpack_read_cdr(FILE* fp, zpack_u64 cdr_offset, zpack_file_entry** entries, zpack_u64* count, zpack_u64* total_cs, zpack_u64* total_us);
ZPACK_EXPORT int zpack_read_archive_memory(zpack_archive* archive);
ZPACK_EXPORT int zpack_read_archive(zpack_archive* archive);

ZPACK_EXPORT int zpack_archive_read_raw_file(zpack_archive* archive, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size);
ZPACK_EXPORT int zpack_archive_read_file(zpack_archive* archive, zpack_file_entry* entry, zpack_u8* buffer, size_t max_size);
ZPACK_EXPORT int zpack_archive_read_file_stream(zpack_archive* archive, zpack_file_entry* entry, zpack_streaming_state* state);

ZPACK_EXPORT int zpack_archive_get_file_entry(zpack_archive* archive, const char* filename, zpack_file_entry** entry);

ZPACK_EXPORT int zpack_open_archive_memory(zpack_archive* archive, const zpack_u8* buffer, size_t size);
ZPACK_EXPORT int zpack_open_archive_memory_shared(zpack_archive* archive, zpack_u8* buffer, size_t size);
ZPACK_EXPORT int zpack_open_archive(zpack_archive* archive, const char* path);
ZPACK_EXPORT void zpack_close_archive(zpack_archive* archive);

// Writing
ZPACK_EXPORT int zpack_init_writer(zpack_writer* writer, const char* path);
ZPACK_EXPORT int zpack_init_writer_heap(zpack_writer* writer, size_t initial_size);

ZPACK_EXPORT int zpack_write_header(zpack_writer* writer);
ZPACK_EXPORT int zpack_write_header_ex(zpack_writer* writer, zpack_u16 version);
ZPACK_EXPORT int zpack_write_data_header(zpack_writer* writer);
ZPACK_EXPORT int zpack_write_files(zpack_writer* writer, zpack_file* files, zpack_u64 file_count);
ZPACK_EXPORT int zpack_write_files_from_archive(zpack_writer* writer, zpack_archive* archive, zpack_file_entry* entries, zpack_u64 file_count);
ZPACK_EXPORT int zpack_write_cdr(zpack_writer* writer);
ZPACK_EXPORT int zpack_write_cdr_ex(zpack_writer* writer, zpack_file_entry* entries, zpack_u64 file_count);
ZPACK_EXPORT int zpack_write_eocdr(zpack_writer* writer);
ZPACK_EXPORT int zpack_write_eocdr_ex(zpack_writer* writer, zpack_u64 cdr_offset);

ZPACK_EXPORT int zpack_write_archive(zpack_writer* writer, zpack_file* files, zpack_u64 file_count);
ZPACK_EXPORT void zpack_close_writer(zpack_writer* writer);

#ifdef __cplusplus
}
#endif

#endif // __ZPACK_H__