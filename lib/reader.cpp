/*
    reader.cpp - ZPack library - File reader [ZPack::Reader]
    Copyright (c) 2020 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#include "zpack.h"
#include "zpack_utils.h"
#include <ostream>
#include <zstd.h>
using namespace ZPack;
using namespace Detail;
using std::ios;
using std::ifstream;

// constructors
Reader::Reader()
: dStream(static_cast<void*>(ZSTD_createDStream())),
  inBuffer(static_cast<void*>(new ZSTD_inBuffer())),
  outBuffer(static_cast<void*>(new ZSTD_outBuffer()))
{
    auto inBuf = static_cast<ZSTD_inBuffer*>(inBuffer);
    auto outBuf = static_cast<ZSTD_outBuffer*>(outBuffer);
    // create the in buffer
    inBuf->size = ZSTD_DStreamInSize();
    inBuf->src = new char[inBuf->size];
    // create the out buffer
    outBuf->size = ZSTD_DStreamOutSize();
    outBuf->dst = new char[outBuf->size];
}

Reader::Reader(std::string filename)
: Reader()
{
    OpenFile(filename);
}

// destructors
Reader::~Reader()
{
    ZSTD_freeDStream(static_cast<ZSTD_DStream*>(dStream));
    delete static_cast<ZSTD_inBuffer*>(inBuffer);
    delete static_cast<ZSTD_outBuffer*>(outBuffer);
}

// io stuff
void Reader::OpenFile(std::string filename)
{
    file.open(filename, ios::binary);
    if (!file.is_open())
    {
        bad = true;
        return;
    }

    // read the file
    bad = ReadFile();
}

bool Reader::ReadFile()
{
    // get file size
    file.seekg(0, file.end);
    size_t fileSize = file.tellg();
    file.seekg(0, file.beg);

    // don't bother trying to read a file that's too small
    if (fileSize < 18)
    {
        return true;
    }

    uint16_t reqVersion = 0;
    // read and verify the header
    if (!ReadHeader(reqVersion))
        return true;

    // check the version
    if (reqVersion > ZPACK_VERSION_INT)
        return true;

    uint64_t cdrOffset = 0;
    // read and verify the eocdr
    if (!ReadEOCDR(cdrOffset))
        return true;

    // read and verify the cdr
    if (!ReadCDR(cdrOffset, entryList, entryMap))
        return true;

    // all good
    return false;
}

// file reader utils
bool InvalidSig(ifstream &file, uint32_t sig)
{
    uint32_t fileSig = 0;
    ReadLE32(file, fileSig);
    return fileSig != sig;
}

// section readers
bool Reader::ReadHeader(uint16_t &reqVersion)
{
    file.seekg(0, file.beg);
    // verify the file's signature
    if (InvalidSig(file, FILE_SIG)) {
        return false;
    }

    // read the required version
    ReadLE16(file, reqVersion);

    // all good
    return true;
}

bool Reader::ReadEOCDR(uint64_t &cdrOffset)
{
    // verify the eocdr's signature
    file.seekg(-12, file.end);
    if (InvalidSig(file, EOCDR_SIG)) {
        return false;
    }

    // get the central directory offset
    ReadLE64(file, cdrOffset);

    // all good
    return true;
}

bool Reader::ReadCDR(uint64_t cdrOffset, EntryList &entryList, EntryMap &entryMap)
{
    // verify the cdr's signature
    file.seekg(cdrOffset, file.beg);
    if (InvalidSig(file, CDIR_SIG))
    {
        return false;
    }

    // loop through the file records until the eocdr is reached
    while (InvalidSig(file, EOCDR_SIG))
    {
        if (file.eof())
        {
            // file should never reach eof while reading the cdr
            return false;
        }
        // seek back 4 characters because signature check already moved the read pos
        file.seekg(-4, file.cur);

        FileInfo *entry = new FileInfo();

        // read the filename length
        uint32_t filenameLen = 0;
        ReadLE32(file, filenameLen);

        // read the filename
        char *filename = new char[filenameLen];
        file.read(filename, filenameLen);
        entry->filename.assign(filename, filenameLen);

        // read the entries
        ReadLE64(file, entry->fileOffset);
        ReadLE64(file, entry->compSize);
        ReadLE64(file, entry->uncompSize);
        ReadLE32(file, entry->crc);

        // add the new file entry
        entryList.push_back(entry);
        entryMap[entry->filename] = entry;

        // add the uncomp size/comp size
        uncompSize += entry->uncompSize;
        compSize += entry->compSize;
    }

    return true;
}

// getters
FileInfo* Reader::GetFileInfo(std::string filename)
{
    auto it = entryMap.find(filename);
    if (it == entryMap.end())
    {
        // not found
        return nullptr;
    }
    else
    {
        // found
        return it->second;
    }
}

bool Reader::Contains(std::string filename)
{
    return entryMap.count(filename) == 1;
}

// one liners
uint64_t Reader::GetFileUncompSize(FileInfo *info) { return info->uncompSize; }
uint64_t Reader::GetFileUncompSize(std::string filename) { return GetFileInfo(filename)->uncompSize; }
uint64_t Reader::GetFileCompSize(FileInfo *info) { return info->compSize; }
uint64_t Reader::GetFileCompSize(std::string filename) { return GetFileInfo(filename)->compSize; }

uint64_t Reader::GetUncompSize()  { return uncompSize; }
uint64_t Reader::GetCompSize()    { return compSize; }
ifstream& Reader::GetFileStream() { return file; }
EntryList Reader::GetEntryList()  { return entryList; }
EntryMap Reader::GetEntryMap()    { return entryMap; }
bool Reader::Bad()                { return bad; }
