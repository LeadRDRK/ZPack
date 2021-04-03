/*
    unpacker.cpp - ZPack library - File unpacker [ZPack::Reader]
    Copyright (c) 2020 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#include "zpack.h"
#include "zpack_utils.h"
#include "zpack_crc.h"
#include <algorithm>
#include <ostream>
#include <sstream>
#include <zstd.h>
using namespace ZPack;

void Reader::UnpackFile(FileInfo *info, std::ostream &dst)
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
        {
            // this should never happen (but it could happen)
            throw;
        }
        inBuf->size = read;
        totalRead += read;

        // frame decompression loop
        while (inBuf->pos < inBuf->size)
        {
            // reset outbuf pos to 0
            outBuf->pos = 0;

            size_t ret = ZSTD_decompressStream(zds, outBuf, inBuf);
            if (ZSTD_isError(ret))
            {
                bad = true;
                return;
            }
            // calculate crc
            crc.Add(charOutBuf, outBuf->pos);
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
        bad = true;
        return;
    }

    // verify crc
    if (info->crc != crc)
    {
        bad = true;
        return;
    }
}

void Reader::UnpackFile(FileInfo *info, char *dst, size_t dstCapacity)
{
    // check if dst is too small
    if (dstCapacity < info->uncompSize)
    {
        throw;
    }
    // TODO: don't use a string stream ? (is that possible)
    std::stringstream strStream;
    UnpackFile(info, strStream);
    strStream.read(dst, info->uncompSize);
    strStream.str(std::string());
}

void Reader::UnpackFile(std::string filename, std::ostream &dst)
{
    UnpackFile(GetFileInfo(filename), dst);
}

void Reader::UnpackFile(std::string filename, char *dst, size_t dstCapacity)
{
    UnpackFile(GetFileInfo(filename), dst, dstCapacity);
}