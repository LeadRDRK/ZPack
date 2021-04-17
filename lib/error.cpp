/*
    error.cpp - ZPack library - Error messages
    Copyright (c) 2021 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#include "zpack.h"
using namespace ZPack;

const char* ZPack::getErrorMessage(int error)
{
    switch (error)
    {
    case OK: return "All good.";

    case ERROR_READ_FAIL:            return "The file couldn't be read.";
    case ERROR_FILE_TOO_SMALL:       return "The archive is too small.";
    case ERROR_INVALID_SIGNATURE:    return "The archive's signature is invalid.";
    case ERROR_INVALID_FILE_RECORD:  return "One of the file records is invalid.";
    case ERROR_VERSION_INSUFFICIENT: return "The program's version is too low to read the archive.";
    case ERROR_DECOMPRESS_FAIL:      return "Failed to decompress the file.";
    case ERROR_CHECKSUM_MISMATCH:    return "The decompressed file's checksum does not match the original checksum.";
    case ERROR_DST_TOO_SMALL:        return "The destination buffer is too small.";
    case ERROR_ILLEGAL_FILENAME:     return "One of the filenames contains an illegal path.";
    case ERROR_WRITE_FAIL:           return "The file couldn't be written.";
    case ERROR_COMPRESS_FAIL:        return "Failed to compress the file.";
    case ERROR_FILENAME_TOO_LONG:    return "The filename is too long.";

    default: return nullptr;
    }
}