/*
    writer.cpp - ZPack library - File writer [ZPack::Writer]
    Copyright (c) 2021 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#include "zpack.h"
#include "zpack_utils.h"
#include "zpack_crc.h"
#include <cstdint>
#include <zstd.h>
#include <climits>
using namespace ZPack;
using namespace Detail;
using std::ios;
using std::ifstream;
using std::ofstream;

// constructors
Writer::Writer()
: cStream(static_cast<void*>(ZSTD_createCStream())),
  inBuffer(static_cast<void*>(new ZSTD_inBuffer())),
  outBuffer(static_cast<void*>(new ZSTD_outBuffer()))
{
    auto inBuf = static_cast<ZSTD_inBuffer*>(inBuffer);
    auto outBuf = static_cast<ZSTD_outBuffer*>(outBuffer);
    // create the in buffer
    inBuf->size = ZSTD_CStreamInSize();
    inBuf->src = new char[inBuf->size];
    // create the out buffer
    outBuf->size = ZSTD_CStreamOutSize();
    outBuf->dst = new char[outBuf->size];
}

Writer::Writer(std::string filename)
: Writer()
{
    openFile(filename);
}

// destructors
Writer::~Writer()
{
    ZSTD_freeCStream(static_cast<ZSTD_CStream*>(cStream));
    delete static_cast<ZSTD_inBuffer*>(inBuffer);
    delete static_cast<ZSTD_outBuffer*>(outBuffer);
}

// io stuff
int Writer::openFile(std::string filename)
{
    file.open(filename, ios::binary);
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}

// low level writing operations
int Writer::writeHeader()
{
    WriteLE32(file, FILE_SIG);
    WriteLE16(file, ZPACK_VERSION_REQUIRED);
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}

int Writer::writeFile(std::string filename, std::istream* inputFile, int compressionLevel)
{
    // cast all of the void* stuff into their actual type
    auto zcs = static_cast<ZSTD_CStream*>(cStream);
    auto inBuf = static_cast<ZSTD_inBuffer*>(inBuffer);
    auto outBuf = static_cast<ZSTD_outBuffer*>(outBuffer);

    // set the compression level
    ZSTD_CCtx_setParameter(zcs, ZSTD_c_compressionLevel, compressionLevel);

    // create the file info
    FileInfo* fileInfo = new FileInfo();
    fileInfo->filename = filename;

    // seek to the end to get file size
    inputFile->seekg(0, inputFile->end);
    uint64_t fileSize = inputFile->tellg();
    inputFile->seekg(0, inputFile->beg);
    // add uncomp size
    uncompSize += fileSize;
    fileInfo->uncompSize = fileSize;

    // file processing section
    fileInfo->fileOffset = file.tellp();
    CRC32 crc;
    int compressedSize = 0;

    char* charInBuf = (char*)inBuf->src;
    char* charOutBuf = (char*)outBuf->dst;
    const size_t toRead = ZSTD_CStreamInSize();

    // file reading loop
    for (;;)
    {
        // reset both buffers' pos to 0
        inBuf->pos = 0;
        outBuf->pos = 0;

        // read the file into the buffer and change the size accordingly
        inputFile->read(charInBuf, toRead);
        size_t readSize = inputFile->gcount();
        inBuf->size = readSize;

        // calculate crc
        crc.add(charInBuf, readSize);

        // compress the file
        ZSTD_EndDirective mode = inputFile->eof() ? ZSTD_e_end : ZSTD_e_continue;
        bool finished;
        do {
            size_t remaining = ZSTD_compressStream2(zcs, outBuf, inBuf, mode);
            if (ZSTD_isError(remaining))
                return ERROR_COMPRESS_FAIL;

            file.write(charOutBuf, outBuf->pos);
            compressedSize += outBuf->pos;
            // From examples/streaming_compression.c
            /* If we're on the last chunk we're finished when zstd returns 0,
             * which means its consumed all the input AND finished the frame.
             * Otherwise, we're finished when we've consumed all the input.
             */
            finished = inputFile->eof() ? (remaining == 0) : (inBuf->pos == inBuf->size);
        } while (!finished);

        if (inputFile->eof())
        {
            break;
        }
    }
    
    fileInfo->crc = crc.digest();
    fileInfo->compSize = compressedSize;
    compSize += compressedSize;

    // add file to the entry list
    entryList.push_back(fileInfo);

    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}

int Writer::writeFile(std::string filename, std::string inputFile, int compressionLevel)
{
    ifstream fileStream(inputFile);
    return writeFile(filename, &fileStream, compressionLevel);
}

int Writer::writeCDR()
{
    cdrOffset = file.tellp();
    WriteLE32(file, CDIR_SIG);
    for (FileInfo *info: entryList) {
        size_t filenameLen = info->filename.length();
        if (filenameLen > UINT16_MAX)
            return ERROR_FILENAME_TOO_LONG;

        WriteLE16(file, filenameLen);
        file << info->filename;
        WriteLE64(file, info->fileOffset);
        WriteLE64(file, info->compSize);
        WriteLE64(file, info->uncompSize);
        WriteLE32(file, info->crc);
    }
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}

int Writer::writeEOCDR()
{
    WriteLE32(file, EOCDR_SIG);
    WriteLE64(file, cdrOffset);
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}
