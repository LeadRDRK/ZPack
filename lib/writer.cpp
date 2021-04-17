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
: cCtx(ZSTD_createCStream()),
  inBuffer(new ZSTD_inBuffer()),
  outBuffer(new ZSTD_outBuffer())
{
    // create the in buffer
    inBuffer->size = ZSTD_CStreamInSize();
    inBuffer->src = new char[inBuffer->size];
    // create the out buffer
    outBuffer->size = ZSTD_CStreamOutSize();
    outBuffer->dst = new char[outBuffer->size];
}

Writer::Writer(const std::string& filename)
: Writer()
{
    openFile(filename);
}

// destructors
Writer::~Writer()
{
    file.close();
    ZSTD_freeCStream(static_cast<ZSTD_CStream*>(cCtx));
    delete static_cast<ZSTD_inBuffer*>(inBuffer);
    delete static_cast<ZSTD_outBuffer*>(outBuffer);
}

// io stuff
int Writer::openFile(const std::string& filename)
{
    file.open(filename, ios::binary);
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}

void Writer::closeFile()
{
    if (file.is_open())
        file.close();
    
    // also clear any current error state
    file.clear();
} 

void Writer::flush()
{
    // reset the values
    uncompSize = 0;
    compSize = 0;
    cdrOffset = 0;

    // clear the entries
    entryList.clear();
}

// writing operations
int Writer::writeHeader()
{
    writeLE32(file, FILE_SIG);
    writeLE16(file, ZPACK_REVISION);
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}

int Writer::writeFile(const std::string& filename, const char* src, size_t srcSize, int compressionLevel)
{
    // filename checks
    if (filename.length() > UINT16_MAX)
        return ERROR_FILENAME_TOO_LONG;
    if (illegalFilename(filename))
        return ERROR_ILLEGAL_FILENAME;

    // set the compression level
    ZSTD_CCtx_setParameter(cCtx, ZSTD_c_compressionLevel, compressionLevel);

    // create the file info
    FileInfo fileInfo;
    fileInfo.filename = filename;

    // add uncomp size
    uncompSize += srcSize;
    fileInfo.uncompSize = srcSize;
    // set the file offset
    fileInfo.fileOffset = file.tellp();

    // and compress the file
    size_t dstSize = ZSTD_compressBound(srcSize);
    char* dst = new char[dstSize];

    size_t compressedSize = ZSTD_compress2(cCtx, dst, dstSize, src, srcSize);
    if (ZSTD_isError(compressedSize))
    {
        delete[] dst;
        return ERROR_COMPRESS_FAIL;
    }

    // write the compressed data
    file.write(dst, dstSize);
    delete[] dst;
    if (!file) return ERROR_WRITE_FAIL;
    
    // calculate the hash
    CRC32 crc;
    crc.add(src, srcSize);
    fileInfo.crc = crc.digest();
    fileInfo.compSize = compressedSize;
    compSize += compressedSize;

    // add file to the entry list
    entryList.push_back(fileInfo);

    return OK;
}

int Writer::writeFile(const std::string& filename, std::istream* inputFile, int compressionLevel)
{
    // seek to the end to get file size
    inputFile->seekg(0, inputFile->end);
    uint64_t srcSize = inputFile->tellg();
    inputFile->seekg(0, inputFile->beg);

    // load the entire file into memory
    char* src = new char[srcSize];
    inputFile->read(src, srcSize);
    if (!inputFile) return ERROR_READ_FAIL;

    int ret = writeFile(filename, src, srcSize, compressionLevel);
    delete[] src;
    return ret;
}

int Writer::writeFile(const std::string& filename, const std::string& inputFile, int compressionLevel)
{
    ifstream fileStream(inputFile);
    return writeFile(filename, &fileStream, compressionLevel);
}

int Writer::writeFileStream(const std::string& filename, std::istream* inputFile, int compressionLevel)
{
    // filename checks
    if (filename.length() > UINT16_MAX)
        return ERROR_FILENAME_TOO_LONG;
    if (illegalFilename(filename))
        return ERROR_ILLEGAL_FILENAME;

    // set the compression level
    ZSTD_CCtx_setParameter(cCtx, ZSTD_c_compressionLevel, compressionLevel);

    // create the file info
    FileInfo fileInfo;
    fileInfo.filename = filename;

    // seek to the end to get file size
    inputFile->seekg(0, inputFile->end);
    uint64_t fileSize = inputFile->tellg();
    inputFile->seekg(0, inputFile->beg);
    // add uncomp size
    uncompSize += fileSize;
    fileInfo.uncompSize = fileSize;

    // file processing section
    fileInfo.fileOffset = file.tellp();
    CRC32 crc;
    int compressedSize = 0;

    char* charInBuf = (char*)inBuffer->src;
    char* charOutBuf = (char*)outBuffer->dst;
    const size_t toRead = ZSTD_CStreamInSize();

    // file reading loop
    for (;;)
    {
        // reset both buffers' pos to 0
        inBuffer->pos = 0;
        outBuffer->pos = 0;

        // read the file into the buffer and change the size accordingly
        inputFile->read(charInBuf, toRead);
        size_t readSize = inputFile->gcount();
        inBuffer->size = readSize;

        // calculate crc
        crc.add(charInBuf, readSize);

        // compress the file
        ZSTD_EndDirective mode = inputFile->eof() ? ZSTD_e_end : ZSTD_e_continue;
        bool finished;
        do {
            size_t remaining = ZSTD_compressStream2(cCtx, outBuffer, inBuffer, mode);
            if (ZSTD_isError(remaining))
                return ERROR_COMPRESS_FAIL;

            file.write(charOutBuf, outBuffer->pos);
            if (!file) return ERROR_WRITE_FAIL;
            compressedSize += outBuffer->pos;
            // From examples/streaming_compression.c
            /* If we're on the last chunk we're finished when zstd returns 0,
             * which means its consumed all the input AND finished the frame.
             * Otherwise, we're finished when we've consumed all the input.
             */
            finished = inputFile->eof() ? (remaining == 0) : (inBuffer->pos == inBuffer->size);
        } while (!finished);

        if (inputFile->eof())
        {
            break;
        }
    }
    
    fileInfo.crc = crc.digest();
    fileInfo.compSize = compressedSize;
    compSize += compressedSize;

    // add file to the entry list
    entryList.push_back(fileInfo);

    return OK;
}

int Writer::writeFileStream(const std::string& filename, const std::string& inputFile, int compressionLevel)
{
    ifstream fileStream(inputFile);
    return writeFile(filename, &fileStream, compressionLevel);
}

int Writer::writeCDR()
{
    cdrOffset = file.tellp();
    writeLE32(file, CDIR_SIG);
    for (FileInfo info: entryList) {
        writeLE16(file, info.filename.length());
        file << info.filename;
        writeLE64(file, info.fileOffset);
        writeLE64(file, info.compSize);
        writeLE64(file, info.uncompSize);
        writeLE32(file, info.crc);
    }
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}

int Writer::writeEOCDR()
{
    writeLE32(file, EOCDR_SIG);
    writeLE64(file, cdrOffset);
    if (!file) return ERROR_WRITE_FAIL;
    return OK;
}
