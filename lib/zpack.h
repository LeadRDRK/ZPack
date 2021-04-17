/*
    zpack.h - ZPack library - Header
    Copyright (c) 2021 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#pragma once

#include "zpack_common.h"
#include <fstream>
#include <string>
#include <vector>
#define ZPACK_VERSION "1.0.0"
#define ZPACK_REVISION 0

// forward declarations
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;
typedef struct ZSTD_inBuffer_s ZSTD_inBuffer;
typedef struct ZSTD_outBuffer_s ZSTD_outBuffer;

/** @file */
namespace ZPack
{
    // stuff for internal usage
    namespace Detail
    {
        // signatures
        const uint32_t FILE_SIG = 0x084b505a; // ZPK\x08
        const uint32_t CDIR_SIG = 0x074b505a; // ZPK\x07
        const uint32_t EOCDR_SIG = 0x064b505a; // ZPK\x06
    };

    /**
     * @brief Return codes.
     */
    enum ReturnCode
    {
        OK, /**< All good. */
        ERROR_READ_FAIL, /**< The file couldn't be read. */
        ERROR_FILE_TOO_SMALL, /**< The archive is too small. */
        ERROR_INVALID_SIGNATURE, /**< The archive's signature is invalid. */
        ERROR_INVALID_FILE_RECORD, /**< One of the file records is invalid. */
        ERROR_VERSION_INSUFFICIENT, /**< The reader's version is too low to read the archive. */
        ERROR_DECOMPRESS_FAIL, /**< Failed to decompress the file. */
        ERROR_CHECKSUM_MISMATCH, /**< The decompressed file's checksum does not match the original checksum. */
        ERROR_DST_TOO_SMALL, /**< The destination buffer is too small. */
        ERROR_ILLEGAL_FILENAME, /**< One of the filenames contains an illegal path. */
        ERROR_WRITE_FAIL, /**< The file couldn't be written. */
        ERROR_COMPRESS_FAIL, /**< Failed to compress the file. */
        ERROR_FILENAME_TOO_LONG /**< The filename is too long. */
    };

    /**
     * Gets the error message for the specified error code. Returns a nullptr if the
     * code is out of range.
     * @param error The error code.
     * @return The error message corresponding to the error code.
     */
    ZPACK_API const char* getErrorMessage(int error);

    // structs

    /**
     * @brief Information about a file in an archive.
     */
    struct ZPACK_API FileInfo
    { 
        std::string filename; /**< Name of the file. */
        uint64_t fileOffset = 0; /**< Offset of the compressed file in the archive. */
        uint64_t compSize = 0; /**< Compressed size of the file. */
        uint64_t uncompSize = 0; /**< Uncompressed size of the file. */
        uint32_t crc = 0; /**< CRC32 digest of the uncompressed file. */
    };

    // types
    typedef std::vector<FileInfo> EntryList;

    /**
     * Usage example:
     * ```
     * // Open the archive
     * ZPack::Reader reader("archive.zpk");
     *
     * // Unpack files
     * std::ifstream file("file.txt");
     * reader.unpackFile("path/to/file.txt", file);
     * file.close();
     *
     * // Streaming API
     * std::ifstream file2("file2.txt");
     * reader.unpackFileStream("path/to/file2.txt", file2);
     * file2.close();
     *
     * // Close the file
     * reader.closeFile();
     *
     * ```
     * The reader can be reused as many times as you want.
     * @brief Archive reader
     */
    class ZPACK_API Reader
    {
    public:
        // constructors
        Reader();
        Reader(const std::string& filename);

        // destructor
        ~Reader();

        // io operations

        /**
         * Open an archive for reading.
         * @param filename Name of the file.
         */
        int openFile(const std::string& filename);

        /**
         * Closes the archive and also resets the reader to its initial state.
         */
        void closeFile();
        
        // low level reading operations

        /**
         * Read the header of an archive.
         * @param reqVersion (Output) Required version to read the file.
         */
        int readHeader(uint16_t &reqVersion);

        /**
         * Read the end of central directory record of an archive.
         * @param cdrOffset (Output) Offset of the central directory record in the archive.
         */
        int readEOCDR(uint64_t &cdrOffset);

        /**
         * Read central directory record of an archive.
         * @param cdrOffset Offset of the central directory record in the archive.
         * @param entryList (Output) List of entries in the CDR.
         */
        int readCDR(uint64_t cdrOffset, EntryList &entryList);

        // unpacking operations

        /**
         * Unpack a file into a stream.
         * @param info Information about the file.
         * @param dst The ostream to write to.
         */
        int unpackFile(const FileInfo *info, std::ostream &dst);

        /**
         * Unpack a file into a buffer.
         * @param info Information about the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        int unpackFile(const FileInfo *info, char *dst, size_t dstCapacity);

        /**
         * Unpack a file into a stream.
         * @param filename Name of the file.
         * @param dst The ostream to write to.
         */
        int unpackFile(const std::string& filename, std::ostream &dst);

        /**
         * Unpack a file into a char* buffer.
         * @param filename Name of the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        int unpackFile(const std::string& filename, char *dst, size_t dstCapacity);

        /**
         * Unpack a file into a stream.\n
         * This is the streaming variant which uses ZSTD's streaming API.
         * @param info Information about the file.
         * @param dst The ostream to write to.
         */
        int unpackFileStream(const FileInfo *info, std::ostream &dst);

        /**
         * Unpack a file into a buffer.\n
         * This is the streaming variant which uses ZSTD's streaming API.
         * @param info Information about the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        int unpackFileStream(const FileInfo *info, char *dst, size_t dstCapacity);

        /**
         * Unpack a file into a stream.\n
         * This is the streaming variant which uses ZSTD's streaming API.
         * @param filename Name of the file.
         * @param dst The ostream to write to.
         */
        int unpackFileStream(const std::string& filename, std::ostream &dst);

        /**
         * Unpack a file into a char* buffer.\n
         * This is the streaming variant which uses ZSTD's streaming API.
         * @param filename Name of the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        int unpackFileStream(const std::string& filename, char *dst, size_t dstCapacity);

        // getters

        /**
         * Get a file's information.
         * @param filename Name of the file.
         */
        const FileInfo* getFileInfo(const std::string& filename);

        /**
         * Check if the archive contains a file.
         * @param filename Name of the file.
         */
        bool contains(const std::string& filename);

        /**
         * Get the file's uncompressed size. Shorthand for GetFileInfo(filename)->uncompSize.
         * Returns 0 if the file does not exist.
         * @param filename Name of the file.
         */
        uint64_t getFileUncompSize(const std::string& filename);

        /**
         * Get the file's compressed size. Shorthand for GetFileInfo(filename)->compSize.
         * Returns 0 if the file does not exist.
         * @param filename Name of the file.
         */
        uint64_t getFileCompSize(const std::string& filename);
        
        /**< Get the total uncompressed size of the archive's files. */
        inline uint64_t getUncompSize() { return uncompSize; };
        /**< Get the total compressed size of the archive's files. */
        inline uint64_t getCompSize() { return compSize; };
        /**< Get the underlying input file stream. */
        inline std::ifstream& getFileStream() { return file; };
        /**< Get the list of file entries. */
        inline EntryList getEntryList() { return entryList; };

    private:
        int readFile();

        // file
        std::ifstream file;
        uint64_t uncompSize = 0;
        uint64_t compSize = 0;

        // entries
        EntryList entryList;

        // zstd
        ZSTD_DCtx* dCtx;
        ZSTD_inBuffer* inBuffer;
        ZSTD_outBuffer* outBuffer;
    };

    /**
     * Usage example:
     * ```
     * // Open the archive
     * ZPack::Writer writer("archive.zpk");
     *
     * // Write the header
     * writer.writeHeader();
     *
     * // Write the files
     * writer.writeFile("file1.txt", "path/to/file1.txt", 3) // normal api
     * writer.writeFileStream("file2.txt", "path/to/file2.txt", 12) // streaming api
     *
     * // Write the central directory record
     * writer.writeCDR();
     *
     * // Write the end of central directory record
     * writer.writeEOCDR();
     *
     * // Close the file and flush the writer
     * writer.closeFile();
     * writer.flush();
     *
     * ```
     * The writer can be reused as many times as you want.
     * @brief Archive writer
     */
    class ZPACK_API Writer
    {
    public:
        // constructors
        Writer();
        Writer(const std::string& filename);

        // destructor
        ~Writer();

        // io operations

        /**
         * Opens an archive for writing.
         * @param filename Name of the file.
         */
        int openFile(const std::string& filename);

        /**
         * Closes the archive.
         */
        void closeFile();

        /**
         * Erases all of the current file entries and resets the writer to its initial state.
         */
        void flush();

        // writing operations

        /**
         * Write the header into the archive.
         */
        int writeHeader();

        /**
         * Compress and write the file into the archive. Also appends the file to the entry list.\n
         * @see Writer#writeFileStream
         * @param filename Name of the file (in the CDR entry).
         * @param src Buffer containing the data of the file.
         * @param srcSize The buffer's size.
         * @param compressionLevel The compression level to use for the file.
         */
        int writeFile(const std::string& filename, const char* src, size_t srcSize, int compressionLevel = 3);

        /**
         * Compress and write the file into the archive. Also appends the file to the entry list.\n
         * @see Writer#writeFileStream
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile The istream of the file.
         * @param compressionLevel The compression level to use for the file.
         */
        int writeFile(const std::string& filename, std::istream* inputFile, int compressionLevel = 3);

        /**
         * Compress and write the file into the archive. Also appends the file to the entry list.\n
         * @see Writer#writeFileStream
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile Path to the file.
         * @param compressionLevel The compression level to use for the file.
         */
        int writeFile(const std::string& filename, const std::string& inputFile, int compressionLevel = 3);

        /**
         * Compress and write the file into the archive. Also appends the file to the entry list.\n
         * This is the streaming variant which uses ZSTD's streaming API.
         * @see Writer#writeFile
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile The istream of the file.
         * @param compressionLevel The compression level to use for the file.
         */
        int writeFileStream(const std::string& filename, std::istream* inputFile, int compressionLevel = 3);

        /**
         * Compress and write the file into the archive. Also appends the file to the entry list.\n
         * This is the streaming variant which uses ZSTD's streaming API.
         * @see Writer#writeFile
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile Path to the file.
         * @param compressionLevel The compression level to use for the file.
         */
        int writeFileStream(const std::string& filename, const std::string& inputFile, int compressionLevel = 3);

        /**
         * Write the central directory record into the archive.
         */
        int writeCDR();

        /**
         * Write the end of central directory record into the archive.
         */
        int writeEOCDR();

        // getters
        /**< Get the total uncompressed size of the archive's files. */
        inline uint64_t getUncompSize() { return uncompSize; };
        /**< Get the total compressed size of the archive's files. */
        inline uint64_t getCompSize() { return compSize; };
        /**< Get the underlying output file stream. */
        inline std::ofstream& getFileStream() { return file; };
        /**< Get the list of file entries. */
        inline EntryList getEntryList() { return entryList; };

    private:
        // file
        std::ofstream file;
        uint64_t uncompSize = 0;
        uint64_t compSize = 0;
        uint64_t cdrOffset = 0;

        // entries
        EntryList entryList;

        // zstd
        ZSTD_CCtx* cCtx;
        ZSTD_inBuffer* inBuffer;
        ZSTD_outBuffer* outBuffer;
    };
} // namespace ZPack
