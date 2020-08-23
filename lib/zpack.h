/*
    zpack.h - ZPack library - Header
    Copyright (c) 2020 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#define ZPACK_VERSION "0.2.0"
#define ZPACK_VERSION_INT 020

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
    struct FileInfo
    { 
        std::string filename;
        uint64_t fileOffset = 0;
        uint64_t compSize = 0;
        uint64_t uncompSize = 0;
        uint32_t crc = 0;
    };

    // types
    typedef std::vector<FileInfo *> EntryList;
    typedef std::unordered_map<std::string, FileInfo *> EntryMap;
    typedef std::vector<std::pair<std::string, std::string>> FileList;
    typedef std::vector<std::string> FileNameList;
    typedef std::vector<std::pair<std::string, std::istream *>> IStreamList;
    typedef std::vector<std::ostream *> OStreamList;
    typedef std::pair<char *, size_t> Buffer;
    typedef std::vector<Buffer> BufferList;
    typedef std::vector<char *> FileDataList;

    class Reader
    {
    public:
        // constructors
        Reader();
        Reader(std::string filename);

        // destructor
        ~Reader();

        // io operations
        void OpenFile(std::string filename);
        
        // low level reading operations
        bool ReadHeader(uint16_t &reqVersion);
        bool ReadEOCDR(uint64_t &cdrOffset);
        bool ReadCDR(uint64_t cdrOffset, EntryList &entryList, EntryMap &entryMap);
        // unpacking operations
        void UnpackFile(FileInfo *info, std::ostream &dst);
        void UnpackFile(FileInfo *info, char *dst, size_t dstCapacity);
        void UnpackFile(std::string filename, std::ostream &dst);
        void UnpackFile(std::string filename, char *dst, size_t dstCapacity);
        void UnpackFiles(EntryList &fileList, BufferList &bufferList);
        void UnpackFiles(EntryList &fileList, OStreamList &streamList);
        void UnpackFiles(FileNameList &fileList, BufferList &bufferList);
        void UnpackFiles(FileNameList &fileList, OStreamList &streamList);

        // getters
        uint64_t GetFileUncompSize(FileInfo *info);
        uint64_t GetFileUncompSize(std::string filename);
        uint64_t GetFileCompSize(FileInfo *info);
        uint64_t GetFileCompSize(std::string filename);

        uint64_t GetUncompSize();
        uint64_t GetCompSize();
        std::ifstream& GetFileStream();
        EntryList GetEntryList();
        EntryMap GetEntryMap();
        FileInfo* GetFileInfo(std::string filename);
        bool Contains(std::string filename);
        bool Bad();

    private:
        // file reader (calls all of the low level reading functions)
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
        // but we're not gonna include zstd.h here
        void* inBuffer; // actual type: ZSTD_inBuffer*
        void* outBuffer; // actual type: ZSTD_outBuffer*
    };

    class Writer
    {
    public:
        // constructors
        Writer();
        Writer(std::string filename);

        // destructor
        ~Writer();

        // io operations
        void OpenFile(std::string filename);

        // low level writing operations
        void WriteHeader();
        void WriteFile(std::string filename, std::istream* inputFile, int compressionLevel = 19);
        void WriteFile(std::string filename, std::string inputFile, int compressionLevel = 19);
        void WriteFiles(IStreamList &fileList, int compressionLevel = 19);
        void WriteFiles(FileList &fileList, int compressionLevel = 19);
        void WriteCDR();
        void WriteEOCDR();
        // packing operations
        void PackFiles(IStreamList &fileList, int compressionLevel = 19);
        void PackFiles(FileList &fileList, int compressionLevel = 19);

        // getters
        bool Bad();
        uint64_t GetUncompSize();
        uint64_t GetCompSize();
        std::ofstream& GetFileStream();
        EntryList GetEntryList();

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