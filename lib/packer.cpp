/*
    packer.cpp - ZPack library - File packer [ZPack::Writer]
    Copyright (c) 2020 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#include "zpack.h"
using namespace ZPack;
using namespace Detail;

void Writer::PackFiles(IStreamList &fileList, int compressionLevel)
{
    WriteHeader();
    WriteFiles(fileList, compressionLevel);
    WriteCDR();
    WriteEOCDR();
}

void Writer::PackFiles(FileList &fileList, int compressionLevel)
{
    WriteHeader();
    WriteFiles(fileList, compressionLevel);
    WriteCDR();
    WriteEOCDR();
}