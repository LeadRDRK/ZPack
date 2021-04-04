/*
    zpack.h - ZPack library - Header
    Copyright (c) 2020 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#pragma once

#include "zpack_common.h"
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#define ZPACK_VERSION "0.3.0"
#define ZPACK_VERSION_INT 030
#define ZPACK_VERSION_REQUIRED 030

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

    // enums
    /**
     * @brief Return codes.
     */
    enum ReturnCode
    {
        OK, /**< All good. */

        // reader
        ERROR_READ_FAIL, /**< The archive couldn't be read. */
        ERROR_FILE_TOO_SMALL, /**< The archive is too small. */
        ERROR_INVALID_SIGNATURE, /**< The archive's signature is invalid. */
        ERROR_VERSION_INSUFFICIENT, /**< The reader's version is too low to read the archive. */
        ERROR_DECOMPRESS_FAIL, /**< Failed to decompress the file. */
        ERROR_CHECKSUM_MISMATCH, /**< The decompressed file's checksum does not match the original checksum. */
        ERROR_DST_TOO_SMALL, /**< The destination buffer is too small. */

        // reader/writer
        ERROR_ILLEGAL_FILENAME, /**< One of the filenames in the archive contains an illegal pathname. */

        // writer
        ERROR_WRITE_FAIL, /**< The archive couldn't be written. */
        ERROR_COMPRESS_FAIL /**< Failed to compress the file. */
    };

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
    typedef std::vector<FileInfo *> EntryList;

    /**
     * @brief Archive reader
     */
    class ZPACK_API Reader
    {
    public:
        // constructors
        Reader();
        Reader(std::string filename);

        // destructor
        ~Reader();

        // io operations

        /**
         * Open an archive for reading.
         * @param filename Name of the file.
         */
        int openFile(std::string filename);
        
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
        int unpackFile(FileInfo *info, std::ostream &dst);

        /**
         * Unpack a file into a buffer.
         * @param info Information about the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        int unpackFile(FileInfo *info, char *dst, size_t dstCapacity);

        /**
         * Unpack a file into a stream.
         * @param filename Name of the file.
         * @param dst The ostream to write to.
         */
        int unpackFile(std::string filename, std::ostream &dst);

        /**
         * Unpack a file into a char* buffer.
         * @param filename Name of the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        int unpackFile(std::string filename, char *dst, size_t dstCapacity);

        // getters
        /**
         * Get a file's information.
         * @param filename Name of the file.
         */
        FileInfo* getFileInfo(std::string filename);

        /**
         * Check if the archive contains a file.
         * @param filename Name of the file.
         */
        bool contains(std::string filename);

        /**
         * Get the file's uncompressed size. Shorthand for GetFileInfo(filename)->uncompSize.
         * Returns 0 if the file does not exist.
         * @param filename Name of the file.
         */
        uint64_t getFileUncompSize(std::string filename);

        /**
         * Get the file's compressed size. Shorthand for GetFileInfo(filename)->compSize.
         * Returns 0 if the file does not exist.
         * @param filename Name of the file.
         */
        uint64_t getFileCompSize(std::string filename);
        
        inline uint64_t getUncompSize() { return uncompSize; }; /**< Get the total uncompressed size of the archive's files. */
        inline uint64_t getCompSize() { return compSize; }; /**< Get the total compressed size of the archive's files. */
        inline std::ifstream& getFileStream() { return file; }; /**< Get the underlying input file stream. */
        inline EntryList getEntryList() { return entryList; }; /**< Get the list of file entries. */

    private:
        int readFile();

        // file
        std::ifstream file;
        uint64_t uncompSize = 0;
        uint64_t compSize = 0;

        // entries
        EntryList entryList;

        // zstd
        void* dStream; // actual type: ZSTD_DStream*
        void* inBuffer; // actual type: ZSTD_inBuffer*
        void* outBuffer; // actual type: ZSTD_outBuffer*
    };

    /**
     * @brief Archive writer
     */
    class ZPACK_API Writer
    {
    public:
        // constructors
        Writer();
        Writer(std::string filename);

        // destructor
        ~Writer();

        // io operations

        /**
         * Open an archive for writing.
         * @param filename Name of the file.
         */
        int openFile(std::string filename);

        // low level writing operations

        /**
         * Write the header into the archive.
         */
        int writeHeader();

        /**
         * Compress and write the file into the archive. Also adds the file into the entry list.
         * Note that this does not write the complete archive; only the data of the file(s).
         * @see Writer#PackFiles
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile The istream of the file.
         * @param compressionLevel The compression level to use for the file. Default: 3.
         */
        int writeFile(std::string filename, std::istream* inputFile, int compressionLevel = 3);

        /**
         * Compress and write the file into the archive. Also adds the file into the entry list.
         * Note that this does not write the complete archive; only the data of the file(s).
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile Path to the file.
         * @param compressionLevel The compression level to use for the file. Default: 3.
         */
        int writeFile(std::string filename, std::string inputFile, int compressionLevel = 3);

        /**
         * Write the central directory record into the archive.
         */
        int writeCDR();

        /**
         * Write the end of central directory record into the archive.
         */
        int writeEOCDR();

        // getters
        inline uint64_t getUncompSize() { return uncompSize; }; /**< Get the total uncompressed size of the archive's files. */
        inline uint64_t getCompSize() { return compSize; }; /**< Get the total compressed size of the archive's files. */
        inline std::ofstream& getFileStream() { return file; }; /**< Get the underlying output file stream. */
        inline EntryList getEntryList() { return entryList; }; /**< Get the list of file entries. */

    private:
        // file
        std::ofstream file;
        uint64_t uncompSize = 0;
        uint64_t compSize = 0;
        uint64_t cdrOffset = 0;

        // entries
        EntryList entryList;

        // zstd
        void* cStream; // actual type: ZSTD_CStream*
        void* inBuffer; // actual type: ZSTD_inBuffer*
        void* outBuffer; // actual type: ZSTD_outBuffer*
    };
} // namespace ZPack
