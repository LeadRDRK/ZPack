/*
    reader.cpp - ZPack library - File reader [ZPack::Reader]
    Copyright (c) 2021 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#include "zpack.h"
#include "zpack_utils.h"
#include "zpack_crc.h"
#include <cstdint>
#include <sstream>
#include <ostream>
#include <streambuf>
#include <zstd.h>
#include <climits>
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
    openFile(filename);
}

// destructors
Reader::~Reader()
{
    ZSTD_freeDStream(static_cast<ZSTD_DStream*>(dStream));
    delete static_cast<ZSTD_inBuffer*>(inBuffer);
    delete static_cast<ZSTD_outBuffer*>(outBuffer);
}

// io stuff
int Reader::openFile(std::string filename)
{
    file.open(filename, ios::binary);
    if (!file.is_open())
        return ERROR_READ_FAIL;

    // read the file
    return readFile();
}

int Reader::readFile()
{
    // get file size
    file.seekg(0, file.end);
    size_t fileSize = file.tellg();
    file.seekg(0, file.beg);

    // don't bother trying to read a file that's too small
    if (fileSize < 18)
    {
        return ERROR_FILE_TOO_SMALL;
    }

    int ret;
    uint16_t reqVersion = 0;
    // read and verify the header
    if ((ret = readHeader(reqVersion)) != OK)
        return ret;

    // check the version
    if (reqVersion > ZPACK_VERSION_INT)
        return ERROR_VERSION_INSUFFICIENT;

    // read and verify the eocdr
    uint64_t cdrOffset;
    if ((ret = readEOCDR(cdrOffset)) != OK)
        return ret;

    // read and verify the cdr
    if ((ret = readCDR(cdrOffset, entryList)) != OK)
        return ret;

    // all good
    return OK;
}

// file reader utils
bool InvalidSig(ifstream &file, uint32_t sig)
{
    uint32_t fileSig = 0;
    ReadLE32(file, fileSig);
    return fileSig != sig;
}

// section readers
int Reader::readHeader(uint16_t &reqVersion)
{
    file.seekg(0, file.beg);
    // verify the file's signature
    if (InvalidSig(file, FILE_SIG))
        return ERROR_INVALID_SIGNATURE;

    // read the required version
    ReadLE16(file, reqVersion);

    // all good
    return OK;
}

int Reader::readEOCDR(uint64_t &cdrOffset)
{
    // verify the eocdr's signature
    file.seekg(-12, file.end);
    if (InvalidSig(file, EOCDR_SIG))
        return ERROR_INVALID_SIGNATURE;

    // get the central directory offset
    ReadLE64(file, cdrOffset);

    // all good
    return OK;
}

int Reader::readCDR(uint64_t cdrOffset, EntryList &entryList)
{
    // verify the cdr's signature
    file.seekg(cdrOffset, file.beg);
    if (InvalidSig(file, CDIR_SIG))
        return ERROR_INVALID_SIGNATURE;

    // loop through the file records until the eocdr is reached
    while (InvalidSig(file, EOCDR_SIG))
    {
        if (file.eof())
        {
            // file should never reach eof while reading the cdr
            return ERROR_INVALID_FILE_RECORD;
        }
        // seek back 4 characters because signature check already moved the read pos
        file.seekg(-4, file.cur);

        FileInfo *entry = new FileInfo();

        // read the filename length
        uint16_t filenameLen = 0;
        ReadLE16(file, filenameLen);

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

        // add the uncomp size/comp size
        uncompSize += entry->uncompSize;
        compSize += entry->compSize;
    }

    return OK;
}

int Reader::unpackFile(FileInfo *info, std::ostream &dst)
{
    // cast all of the void* stuff into their actual type
    auto zds = static_cast<ZSTD_DStream*>(dStream);
    auto inBuf = static_cast<ZSTD_inBuffer*>(inBuffer);
    auto outBuf = static_cast<ZSTD_outBuffer*>(outBuffer);

    // seek to the file
    file.seekg(info->fileOffset, file.beg);
    
    char* charInBuf = (char*)inBuf->src;
    char* charOutBuf = (char*)outBuf->dst;
    size_t toRead = std::min(info->compSize, ZSTD_DStreamInSize());
    size_t lastRet = 0;
    size_t totalRead = 0;
    CRC32 crc;

    // file reading loop
    while (totalRead < info->compSize)
    {
        // reset both buffers' pos to 0
        inBuf->pos = 0;
        outBuf->pos = 0;

        // reduce reading size if needed
        toRead = std::min(toRead, info->compSize - totalRead);

        // read part of the file
        file.read(charInBuf, toRead);
        size_t read = file.gcount();
        if (read < toRead) 
            return ERROR_READ_FAIL;
        
        inBuf->size = read;
        totalRead += read;

        // frame decompression loop
        while (inBuf->pos < inBuf->size)
        {
            // reset outbuf pos to 0
            outBuf->pos = 0;

            size_t ret = ZSTD_decompressStream(zds, outBuf, inBuf);
            if (ZSTD_isError(ret))
                return ERROR_DECOMPRESS_FAIL;
            // calculate crc
            crc.add(charOutBuf, outBuf->pos);
            // write to dst
            dst.write(charOutBuf, outBuf->pos);
            lastRet = ret;
        }
    }

    if (lastRet != 0) {
        // From examples/streaming_decompression.c
        /* The last return value from ZSTD_decompressStream did not end on a
         * frame, but we reached the end of the file! We assume this is an
         * error, and the input was truncated.
         */
        return ERROR_DECOMPRESS_FAIL;
    }

    // verify crc
    if (info->crc != crc.digest())
        return ERROR_CHECKSUM_MISMATCH;
    
    // all good
    return OK;
}

int Reader::unpackFile(FileInfo *info, char *dst, size_t dstCapacity)
{
    // check if dst is too small
    if (dstCapacity < info->uncompSize)
        return ERROR_DST_TOO_SMALL;

    std::stringstream stream;
    stream.rdbuf()->pubsetbuf(dst, dstCapacity);
    unpackFile(info, stream);
    stream.rdbuf()->pubsetbuf(nullptr, 0);

    return OK;
}

int Reader::unpackFile(std::string filename, std::ostream &dst)
{
    return unpackFile(getFileInfo(filename), dst);
}

int Reader::unpackFile(std::string filename, char *dst, size_t dstCapacity)
{
    return unpackFile(getFileInfo(filename), dst, dstCapacity);
}

// getters
FileInfo* Reader::getFileInfo(std::string filename)
{
    for (int i = 0; i < entryList.size(); ++i)
    {
        auto entry = entryList[i];
        if (entry->filename == filename)
            return entry;
    }
    return nullptr;
}

bool Reader::contains(std::string filename)
{
    for (int i = 0; i < entryList.size(); ++i)
    {
        if (entryList[i]->filename == filename)
            return true;
    }
    return false;
}

uint64_t Reader::getFileUncompSize(std::string filename)
{
    FileInfo* fileInfo = getFileInfo(filename);
    return fileInfo != nullptr ? fileInfo->uncompSize : 0;
}

uint64_t Reader::getFileCompSize(std::string filename)
{
    FileInfo* fileInfo = getFileInfo(filename);
    return fileInfo != nullptr ? fileInfo->compSize : 0;
}
