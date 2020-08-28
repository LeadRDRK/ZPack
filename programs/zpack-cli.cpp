/*
    zpack-cli.cpp - ZPack command line interface
    Copyright (c) 2020 LeadRDRK
    Licensed under the BSD 3-Clause license.
    Check the LICENSE file for more information.
*/

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <vector>
#include <zpack.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
using namespace std::chrono;

typedef std::vector<std::string> PathList;
typedef std::vector<std::pair<std::string, std::ifstream *>> FileList;

#define AUTHOR "LeadRDRK"
#define PROGRAM_NAME "ZPack command line interface v" ZPACK_VERSION
#define LINE_SEPARATOR "-----------------------------\n"

void printUsage()
{
    std::cout <<
        PROGRAM_NAME ", by " AUTHOR "\n"
        "\n"
        "Usage: zpack [options] ... <file>\n"
        "\n"
        "Available options:\n"
        "  -1 ... -22   : compression level\n"
        "  -c, --create : create a new archive\n"
        "  -u, --unpack : unpack an archive\n"
        "  -o, --output : specify the output folder\n"
        "  -h, --help   : prints the usage and exit\n"
        "  -v, --version: prints the version and exit\n"
        "\n"
        "Examples:\n"
        "  zpack -c /path/folder /path/file.txt archive.zpk\n"
        "  zpack -c /path/folder/* /path/folder2/*.txt archive.zpk\n"
        "  zpack -u -o /path/folder archive.zpk\n"
    << std::endl;
}

void missingArgs()
{
    std::cout << "Missing arguments.\n";
    printUsage();
}

void printCreateStats(float timeElapsed, size_t numFiles, uint64_t uncompSize, uint64_t compSize)
{
    float compressionRatio = (float)compSize/uncompSize;
    std::cout <<
        LINE_SEPARATOR <<
        "Done! (" << timeElapsed << "s)\n" <<
        "\n" <<
        "Files packed:       " << numFiles << "\n" <<
        "Uncompressed:       " << uncompSize << "\n" <<
        "Compressed:         " << compSize << "\n" <<
        "Compression ratio:  " << (float)compressionRatio*100 << "%\n" <<
    std::endl;
}

void printUnpackStats(float timeElapsed, size_t numFiles)
{
    std::cout <<
        LINE_SEPARATOR <<
        "Done! (" << timeElapsed << "s)\n" <<
        "\n" <<
        "Files unpacked:     " << numFiles << "\n" <<
    std::endl;
}

// file name utils
std::string getFileName(const std::string& fullPath)
{
    const size_t lastSlashIndex = fullPath.find_last_of("/\\");
    return fullPath.substr(lastSlashIndex + 1);
}

std::string getFileDir(const std::string& fullPath)
{
    const size_t lastSlashIndex = fullPath.find_last_of("/\\");
    return fullPath.substr(0, lastSlashIndex);
}

std::string getFileExt(const std::string& fullPath)
{
    const size_t lastDotIndex = fullPath.find_last_of(".");
    std::string ext = fullPath.substr(lastDotIndex + 1);
    if (ext == fullPath)
        ext = "";
    return ext;
}

void processDirEntry(const fs::directory_entry &entry, FileList &fileList,
                     const std::string &ext, bool appendDirName)
{
    const fs::path &path = entry.path();
    // skip over directories
    if (fs::is_directory(path))
        return;

    // extension check
    std::string filename = path.filename().string();
    if (!ext.empty())
    {
        if (getFileExt(filename) != ext)
            return;
    }

    if (appendDirName)
    {
        filename = getFileName(path.parent_path().string()) + "/" + filename;
    }
    std::ifstream* file = new std::ifstream(path.string());
    if (!file->is_open())
    {
        std::cerr << "FATAL: Failed to open " << path << "\n";
        exit(1);
    }
    fileList.push_back({filename, file});
}

void getDirFileList(const std::string &directory, const std::string &ext,
                    FileList &fileList, bool appendDirName = false,
                    bool recursive = false)
{
    if (recursive) {
        for (auto& entry: fs::recursive_directory_iterator(directory))
        {
            processDirEntry(entry, fileList, ext, appendDirName);
        }
    } else {
        for (auto& entry: fs::directory_iterator(directory))
        {
            processDirEntry(entry, fileList, ext, appendDirName);
        }
    }
}

void parsePathList(PathList &pathList, FileList &fileList)
{
    for (auto path: pathList)
    {
        std::string filename = getFileName(path);
        if (filename.rfind("*", 0) == 0)
        {
            // directory wildcard
            std::string directory = getFileDir(path);
            std::string ext = getFileExt(path);
            bool recursive = filename.rfind("**", 0) == 0;
            getDirFileList(directory, ext, fileList, false, recursive);
            continue;
        }
        else if (fs::is_directory(path))
        {
            getDirFileList(path, "", fileList, true, true);
            continue;
        }
        else
        {
            std::string filename = getFileName(path);
            std::ifstream *file = new std::ifstream(path);
            fileList.push_back({filename, file});
        }
    }
}

int createArchive(PathList &pathList, std::string &filename, int compressionLevel)
{
    steady_clock::time_point startTime = steady_clock::now();
    if (pathList.size() < 1)
    {
        missingArgs();
        return 1;
    }

    FileList fileList;
    parsePathList(pathList, fileList);

    if (getFileExt(filename).empty())
    {
        // automatically add extension
        filename += ".zpk";
    }
    ZPack::Writer zpkWriter(filename);
    if (zpkWriter.Bad())
    {
        std::cerr << "FATAL: Failed to open " << filename << "\n";
        return 1;
    }
        
    std::cout << "-- Writing header..." << std::endl;
    zpkWriter.WriteHeader();
    std::cout << "-- Compressing files..." << std::endl;
    for (auto pair: fileList)
    {
        auto filename = pair.first;
        auto inputFile = pair.second;
        std::cout << " " << filename << std::endl;
        zpkWriter.WriteFile(filename, inputFile, compressionLevel);
        if (zpkWriter.Bad())
        {
            std::cerr << "FATAL: Failed to compress " << filename << "\n";
            return 1;
        }
        // writefile does not close the file automatically
        inputFile->close();
    }
    std::cout << "-- Writing CDR..." << std::endl;
    zpkWriter.WriteCDR();
    std::cout << "-- Writing EOCDR..." << std::endl;
    zpkWriter.WriteEOCDR();

    steady_clock::time_point endTime = steady_clock::now();
    duration<float> timeElapsed = duration_cast<milliseconds>(endTime - startTime);
    printCreateStats(timeElapsed.count(), fileList.size(),
                     zpkWriter.GetUncompSize(), zpkWriter.GetCompSize());

    return 0;
}

int unpackArchive(std::string &filename, std::string &outputFolder)
{
    steady_clock::time_point startTime = steady_clock::now();

    ZPack::Reader zpkReader(filename);
    if (zpkReader.Bad())
    {
        std::cerr << "FATAL: File unreadable or invalid." << "\n";
        return 1;
    }

    std::cout << "-- Unpacking files..." << std::endl;
    std::ofstream outputFile;
    ZPack::EntryList entryList = zpkReader.GetEntryList();
    for (auto fileInfo: entryList)
    {
        std::cout << " " << fileInfo->filename << std::endl;

        fs::path filePath(outputFolder + "/" + fileInfo->filename);
        fs::create_directories(filePath.parent_path());
        outputFile.open(filePath.string(),
                        std::ios::trunc | std::ios::binary);
        if (!outputFile.is_open())
        {
            std::cerr << "FATAL: Failed to open output file " << filePath << "\n";
            return 1;
        }

        zpkReader.UnpackFile(fileInfo, outputFile);
        if (zpkReader.Bad())
        {
            std::cerr << "FATAL: Failed to unpack " << fileInfo->filename << "\n";
            return 1;
        }
        outputFile.close();
    }

    steady_clock::time_point endTime = steady_clock::now();
    duration<float> timeElapsed = duration_cast<milliseconds>(endTime - startTime);
    printUnpackStats(timeElapsed.count(), entryList.size());

    return 0;
}

int main(int argc, char **argv)
{
    int mode = 0;
    std::string outputFolder = ".";
    PathList pathList;
    std::string filename = "";
    int compressionLevel = 19;
    
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string arg(argv[i]);

            if (argc > 2 && i == argc - 1)
            {
                filename = arg;
                break;
            }

            if (arg == "-c" || arg == "--create")
            {
                mode = 1;
                continue;
            }
            else if (arg == "-u" || arg == "--unpack")
            {
                mode = 2;
                continue;
            }
            else if (arg == "-o" || arg == "--output")
            {
                outputFolder = argv[++i];
                continue;
            }
            else if (arg == "-h" || arg == "--help")
            {
                printUsage();
                return 0;
            }
            else if (arg == "-v" || arg == "--version")
            {
                std::cout <<
                    PROGRAM_NAME "\n"
                    "Copyright (c) 2020 LeadRDRK. Licensed under the BSD 3-Clause license.\n";
                return 0;
            }
            else if (std::isdigit(arg[1])) {
                int temp;
                try {
                    temp = std::stoi(arg);
                    if (temp <= -1 && temp >= -22)
                    {
                        compressionLevel = -temp;
                    }
                    else {
                        std::cout << "Invalid argument: " << arg << "\n";
                        printUsage();
                        return 1;
                    }
                }
                catch (int _) {}
            }
            else if (mode == 1)
            {
                pathList.push_back(arg);
            }
            else {
                std::cout << "Invalid argument: " << arg << "\n";
                printUsage();
                return 1;
            }
        }
    }
    else
    {
        missingArgs();
        return 1;
    }

    if (mode == 1) // create mode
    {
        return createArchive(pathList, filename, compressionLevel);
    }
    else if (mode == 2) // unpack mode
    {
        return unpackArchive(filename, outputFolder);
    }
    return 0;
}
