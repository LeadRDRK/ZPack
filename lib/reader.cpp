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
: dCtx(ZSTD_createDStream()),
  inBuffer(new ZSTD_inBuffer()),
  outBuffer(new ZSTD_outBuffer())
{
    // create the in buffer
    inBuffer->size = ZSTD_DStreamInSize();
    inBuffer->src = new char[inBuffer->size];
    // create the out buffer
    outBuffer->size = ZSTD_DStreamOutSize();
    outBuffer->dst = new char[outBuffer->size];
}

Reader::Reader(const std::string& filename)
: Reader()
{
    openFile(filename);
}

// destructor
Reader::~Reader()
{
    ZSTD_freeDStream(dCtx);
    delete inBuffer;
    delete outBuffer;
}

// io stuff
int Reader::openFile(const std::string& filename)
{
    if (file.is_open())
        closeFile();

    file.open(filename, ios::binary);
    if (!file.is_open())
        return ERROR_READ_FAIL;

    // read the file
    return readFile();
}

void Reader::closeFile()
{
    if (file.is_open())
        file.close();

    // also clear the state
    file.clear();

    // reset values
    uncompSize = 0;
    compSize = 0;
    entryList.clear();
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
    if (reqVersion > ZPACK_REVISION)
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
bool invalidSig(ifstream &file, uint32_t sig)
{
    uint32_t fileSig = 0;
    readLE32(file, fileSig);
    return fileSig != sig;
}

// section readers
int Reader::readHeader(uint16_t &reqVersion)
{
    file.seekg(0, file.beg);
    // verify the file's signature
    if (invalidSig(file, FILE_SIG))
        return ERROR_INVALID_SIGNATURE;

    // read the required version
    readLE16(file, reqVersion);

    // all good
    return OK;
}

int Reader::readEOCDR(uint64_t &cdrOffset)
{
    // verify the eocdr's signature
    file.seekg(-12, file.end);
    if (invalidSig(file, EOCDR_SIG))
        return ERROR_INVALID_SIGNATURE;

    // get the central directory offset
    readLE64(file, cdrOffset);

    // all good
    return OK;
}

int Reader::readCDR(uint64_t cdrOffset, EntryList &entryList)
{
    // verify the cdr's signature
    file.seekg(cdrOffset, file.beg);
    if (invalidSig(file, CDIR_SIG))
        return ERROR_INVALID_SIGNATURE;

    // loop through the file records until the eocdr is reached
    while (invalidSig(file, EOCDR_SIG))
    {
        if (file.eof())
        {
            // file should never reach eof while reading the cdr
            return ERROR_INVALID_FILE_RECORD;
        }
        // seek back 4 characters because signature check already moved the read pos
        file.seekg(-4, file.cur);

        FileInfo entry;

        // read the filename length
        uint16_t filenameLen = 0;
        readLE16(file, filenameLen);

        // read the filename
        char *filename = new char[filenameLen];
        file.read(filename, filenameLen);
        entry.filename.assign(filename, filenameLen);
        if (illegalFilename(entry.filename))
            return ERROR_ILLEGAL_FILENAME;

        // read the entries
        readLE64(file, entry.fileOffset);
        readLE64(file, entry.compSize);
        readLE64(file, entry.uncompSize);
        readLE32(file, entry.crc);

        // add the new file entry
        entryList.push_back(entry);

        // add the uncomp size/comp size
        uncompSize += entry.uncompSize;
        compSize += entry.compSize;
    }

    return OK;
}

int Reader::unpackFile(const FileInfo *info, std::ostream &dst)
{
    char* buffer = new char[info->uncompSize];
    int ret;
    if ((ret = unpackFile(info, buffer, info->uncompSize)) != OK)
        return ret;
    
    dst.write(buffer, info->uncompSize);
    delete[] buffer;

    if (!dst)
        return ERROR_DECOMPRESS_FAIL;

    return OK;
}

int Reader::unpackFile(const FileInfo *info, char *dst, size_t dstCapacity)
{
    // check if dst is too small
    if (dstCapacity < info->uncompSize)
        return ERROR_DST_TOO_SMALL;
    
    CRC32 crc;
    char* src = new char[info->compSize];

    // seek to the file
    file.seekg(info->fileOffset, file.beg);
    // and read the whole thing in
    file.read(src, info->compSize);

    // decompress the file
    size_t size = ZSTD_decompressDCtx(dCtx, dst, dstCapacity, src, info->compSize);
    delete[] src;
    if (ZSTD_isError(size))
        return ERROR_DECOMPRESS_FAIL;

    // verify crc
    crc.add(dst, size);
    if (info->crc != crc.digest())
        return ERROR_CHECKSUM_MISMATCH;

    // all good
    return OK;
}

int Reader::unpackFile(const std::string& filename, std::ostream &dst)
{
    return unpackFile(getFileInfo(filename), dst);
}

int Reader::unpackFile(const std::string& filename, char *dst, size_t dstCapacity)
{
    return unpackFile(getFileInfo(filename), dst, dstCapacity);
}

int Reader::unpackFileStream(const FileInfo *info, std::ostream &dst)
{
    // seek to the file
    file.seekg(info->fileOffset, file.beg);
    
    char* charInBuf = (char*)inBuffer->src;
    char* charOutBuf = (char*)outBuffer->dst;
    size_t toRead = std::min(info->compSize, (uint64_t)ZSTD_DStreamInSize());
    size_t lastRet = 0;
    size_t totalRead = 0;
    CRC32 crc;

    // file reading loop
    while (totalRead < info->compSize)
    {
        // reset both buffers' pos to 0
        inBuffer->pos = 0;
        outBuffer->pos = 0;

        // reduce reading size if needed
        toRead = std::min(toRead, (size_t)(info->compSize - totalRead));

        // read part of the file
        file.read(charInBuf, toRead);
        size_t read = file.gcount();
        if (read < toRead) 
            return ERROR_READ_FAIL;
        
        inBuffer->size = read;
        totalRead += read;

        // frame decompression loop
        while (inBuffer->pos < inBuffer->size)
        {
            // reset outBuffer pos to 0
            outBuffer->pos = 0;

            size_t ret = ZSTD_decompressStream(dCtx, outBuffer, inBuffer);
            if (ZSTD_isError(ret))
                return ERROR_DECOMPRESS_FAIL;
            // calculate crc
            crc.add(charOutBuf, outBuffer->pos);
            // write to dst
            dst.write(charOutBuf, outBuffer->pos);
            if (!dst) return ERROR_DECOMPRESS_FAIL;
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

int Reader::unpackFileStream(const FileInfo *info, char *dst, size_t dstCapacity)
{
    // check if dst is too small
    if (dstCapacity < info->uncompSize)
        return ERROR_DST_TOO_SMALL;

    std::stringstream stream;
    stream.rdbuf()->pubsetbuf(dst, dstCapacity);
    unpackFileStream(info, stream);
    stream.rdbuf()->pubsetbuf(nullptr, 0);

    return OK;
}

int Reader::unpackFileStream(const std::string& filename, std::ostream &dst)
{
    return unpackFileStream(getFileInfo(filename), dst);
}

int Reader::unpackFileStream(const std::string& filename, char *dst, size_t dstCapacity)
{
    return unpackFileStream(getFileInfo(filename), dst, dstCapacity);
}

// getters
const FileInfo* Reader::getFileInfo(const std::string& filename)
{
    for (int i = 0; i < entryList.size(); ++i)
    {
        if (entryList[i].filename == filename)
            return &entryList[i];
    }
    return nullptr;
}

bool Reader::contains(const std::string& filename)
{
    for (int i = 0; i < entryList.size(); ++i)
    {
        if (entryList[i].filename == filename)
            return true;
    }
    return false;
}

uint64_t Reader::getFileUncompSize(const std::string& filename)
{
    auto fileInfo = getFileInfo(filename);
    return fileInfo != nullptr ? fileInfo->uncompSize : 0;
}

uint64_t Reader::getFileCompSize(const std::string& filename)
{
    auto fileInfo = getFileInfo(filename);
    return fileInfo != nullptr ? fileInfo->compSize : 0;
}
