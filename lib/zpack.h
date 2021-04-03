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
#define ZPACK_VERSION "0.2.3"
#define ZPACK_VERSION_INT 023

// version required to read the file; not necessarily the current version
// should be increased when a file structure change is made
#define ZPACK_VERSION_REQUIRED 020

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
    typedef std::unordered_map<std::string, FileInfo *> EntryMap;

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
        void OpenFile(std::string filename);
        
        // low level reading operations

        /**
         * Read the header of an archive.
         * @param reqVersion (Output) Required version to read the file.
         */
        bool ReadHeader(uint16_t &reqVersion);

        /**
         * Read the end of central directory record of an archive.
         * @param cdrOffset (Output) Offset of the central directory record in the archive.
         */
        bool ReadEOCDR(uint64_t &cdrOffset);

        /**
         * Read central directory record of an archive.
         * @param cdrOffset Offset of the central directory record in the archive.
         * @param entryList (Output) List of entries in the CDR.
         * @param entryMap (Output) List of entries in the CDR mapped by their filenames.
         */
        bool ReadCDR(uint64_t cdrOffset, EntryList &entryList, EntryMap &entryMap);

        // unpacking operations

        /**
         * Unpack a file into a stream.
         * @param info Information about the file.
         * @param dst The ostream to write to.
         */
        void UnpackFile(FileInfo *info, std::ostream &dst);

        /**
         * Unpack a file into a buffer.
         * @param info Information about the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        void UnpackFile(FileInfo *info, char *dst, size_t dstCapacity);

        /**
         * Unpack a file into a stream.
         * @param filename Name of the file.
         * @param dst The ostream to write to.
         */
        void UnpackFile(std::string filename, std::ostream &dst);

        /**
         * Unpack a file into a char* buffer.
         * @param filename Name of the file.
         * @param dst The char* buffer to write to.
         * @param dstCapacity The size of the buffer.
         */
        void UnpackFile(std::string filename, char *dst, size_t dstCapacity);

        // getters

        /**
         * Get the file's uncompressed size. Shorthand for GetFileInfo(filename)->uncompSize.
         * Returns 0 if the file does not exist.
         * @param filename Name of the file.
         */
        uint64_t GetFileUncompSize(std::string filename);

        /**
         * Get the file's compressed size. Shorthand for GetFileInfo(filename)->compSize.
         * Returns 0 if the file does not exist.
         * @param filename Name of the file.
         */
        uint64_t GetFileCompSize(std::string filename);
        
        uint64_t GetUncompSize(); /**< Get the total uncompressed size of the archive's files. */
        uint64_t GetCompSize(); /**< Get the total compressed size of the archive's files. */
        std::ifstream& GetFileStream(); /**< Get the underlying input file stream. */
        EntryList GetEntryList(); /**< Get the list of file entries (in the CDR). */
        EntryMap GetEntryMap(); /**< Get the list of file entries mapped to their filenames 
                                     (in the CDR). */
        FileInfo* GetFileInfo(std::string filename); /**< Get a file's information. */
        bool Contains(std::string filename); /**< Check if the archive contains a file. */
        bool Bad(); /**< Check if the reader's "bad" state is set.\n
        This is true when the archive is unreadable, invalid or when an unpacked file is invalid */

    private:
        bool ReadFile();

        // properties
        // bad - set when the file is unreadable or invalid
        bool bad = false;

        // file
        std::ifstream file;
        uint64_t uncompSize = 0;
        uint64_t compSize = 0;

        // entries
        EntryList entryList;
        EntryMap entryMap;

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
        void OpenFile(std::string filename);

        // low level writing operations

        /**
         * Write the header into the archive.
         */
        void WriteHeader();

        /**
         * Compress and write the file into the archive. Also adds the file into the entry list.
         * Note that this does not write the complete archive; only the data of the file(s).
         * @see Writer#PackFiles
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile The istream of the file.
         * @param compressionLevel The compression level to use for the file. Default: 19.
         */
        void WriteFile(std::string filename, std::istream* inputFile, int compressionLevel = 19);

        /**
         * Compress and write the file into the archive. Also adds the file into the entry list.
         * Note that this does not write the complete archive; only the data of the file(s).
         * @see Writer#PackFiles
         * @param filename Name of the file (in the CDR entry).
         * @param inputFile Path to the file.
         * @param compressionLevel The compression level to use for the file. Default: 19.
         */
        void WriteFile(std::string filename, std::string inputFile, int compressionLevel = 19);

        /**
         * Write the central directory record into the archive.
         */
        void WriteCDR();

        /**
         * Write the end of central directory record into the archive.
         */
        void WriteEOCDR();

        // getters
        bool Bad(); /**< Check if the writer's "bad" state is set.\n
        This is true when the archive is unwritable. */
        uint64_t GetUncompSize(); /**< Get the total uncompressed size of the archive's files. */
        uint64_t GetCompSize(); /**< Get the total compressed size of the archive's files. */
        std::ofstream& GetFileStream(); /**< Get the underlying output file stream. */
        EntryList GetEntryList(); /**< Get the list of file entries to be written to the CDR. */

    private:
        // properties
        // bad - set when the file is unwritable
        bool bad = false;

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
