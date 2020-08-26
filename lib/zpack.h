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
#define ZPACK_VERSION "0.2.1"
#define ZPACK_VERSION_INT 021

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
        void ZPACK_API OpenFile(std::string filename);
        
        // low level reading operations
        bool ZPACK_API ReadHeader(uint16_t &reqVersion);
        bool ZPACK_API ReadEOCDR(uint64_t &cdrOffset);
        bool ZPACK_API ReadCDR(uint64_t cdrOffset, EntryList &entryList, EntryMap &entryMap);
        // unpacking operations
        void ZPACK_API UnpackFile(FileInfo *info, std::ostream &dst);
        void ZPACK_API UnpackFile(FileInfo *info, char *dst, size_t dstCapacity);
        void ZPACK_API UnpackFile(std::string filename, std::ostream &dst);
        void ZPACK_API UnpackFile(std::string filename, char *dst, size_t dstCapacity);
        void ZPACK_API UnpackFiles(EntryList &fileList, BufferList &bufferList);
        void ZPACK_API UnpackFiles(EntryList &fileList, OStreamList &streamList);
        void ZPACK_API UnpackFiles(FileNameList &fileList, BufferList &bufferList);
        void ZPACK_API UnpackFiles(FileNameList &fileList, OStreamList &streamList);

        // getters
        uint64_t ZPACK_API GetFileUncompSize(FileInfo *info);
        uint64_t ZPACK_API GetFileUncompSize(std::string filename);
        uint64_t ZPACK_API GetFileCompSize(FileInfo *info);
        uint64_t ZPACK_API GetFileCompSize(std::string filename);

        uint64_t ZPACK_API GetUncompSize();
        uint64_t ZPACK_API GetCompSize();
        std::ifstream& ZPACK_API GetFileStream();
        EntryList ZPACK_API GetEntryList();
        EntryMap ZPACK_API GetEntryMap();
        FileInfo* ZPACK_API GetFileInfo(std::string filename);
        bool ZPACK_API Contains(std::string filename);
        bool ZPACK_API Bad();

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
        void ZPACK_API OpenFile(std::string filename);

        // low level writing operations
        void ZPACK_API WriteHeader();
        void ZPACK_API WriteFile(std::string filename, std::istream* inputFile, int compressionLevel = 19);
        void ZPACK_API WriteFile(std::string filename, std::string inputFile, int compressionLevel = 19);
        void ZPACK_API WriteFiles(IStreamList &fileList, int compressionLevel = 19);
        void ZPACK_API WriteFiles(FileList &fileList, int compressionLevel = 19);
        void ZPACK_API WriteCDR();
        void ZPACK_API WriteEOCDR();
        // packing operations
        void ZPACK_API PackFiles(IStreamList &fileList, int compressionLevel = 19);
        void ZPACK_API PackFiles(FileList &fileList, int compressionLevel = 19);

        // getters
        bool ZPACK_API Bad();
        uint64_t ZPACK_API GetUncompSize();
        uint64_t ZPACK_API GetCompSize();
        std::ofstream& ZPACK_API GetFileStream();
        EntryList ZPACK_API GetEntryList();

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
