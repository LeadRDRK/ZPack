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
#include <unordered_map>
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
        "Usage: zpack [options] ... <archive>\n"
        "\n"
        "Available options:\n"
        "  -1 ... -22     : compression level\n"
        "  -f, --fast [n] : very fast compression level\n"
        "  -c, --create   : create a new archive\n"
        "  -u, --unpack   : unpack an archive\n"
        "  -o, --output   : specify the output folder\n"
        "  -l, --list     : list files in an archive\n"
        "  -h, --help     : prints the usage and exit\n"
        "  -v, --version  : prints the version and exit\n"
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
std::string getFilename(const std::string& fullPath)
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

std::string getFullPath(const fs::path& path, int depth)
{
    std::string fullPath;
    fs::path curPath(path);
    for (int i = 0; i < depth + 1; ++i)
    {
        curPath = curPath.parent_path();
        fullPath = curPath.filename().string() + "/" + fullPath;
    }
    return fullPath;
}

void getDirFileList(const std::string &directory, FileList &fileList)
{
    for (auto entry = fs::recursive_directory_iterator(directory);
              entry != fs::recursive_directory_iterator();
            ++entry)
    {
        const fs::path &path = entry->path();
        if (fs::is_directory(path))
            continue;

        std::string filename = getFullPath(path, entry.depth()) + path.filename().string();
        std::ifstream* file = new std::ifstream(path.string());
        if (!file->is_open())
        {
            std::cerr << "FATAL: Failed to open " << path << "\n";
            exit(1);
        }
        fileList.push_back({filename, file});
    }
}

void parsePathList(PathList &pathList, FileList &fileList)
{
    for (auto path: pathList)
    {
        if (fs::is_directory(path))
        {
            getDirFileList(path, fileList);
            continue;
        }
        else
        {
            std::string filename = getFilename(path);
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
    ZPack::Writer zpkWriter;
    if (zpkWriter.openFile(filename) != ZPack::OK)
    {
        std::cerr << "FATAL: Failed to open " << filename << "\n";
        return 1;
    }
        
    std::cout << "-- Writing header..." << std::endl;
    zpkWriter.writeHeader();
    std::cout << "-- Compressing files..." << std::endl;
    for (auto pair: fileList)
    {
        auto filename = pair.first;
        auto inputFile = pair.second;
        std::cout << " " << filename << std::endl;
        if (zpkWriter.writeFile(filename, inputFile, compressionLevel) != ZPack::OK)
        {
            std::cerr << "FATAL: Failed to compress " << filename << std::endl;
            return 1;
        }
        // writefile does not close the file automatically
        inputFile->close();
    }
    std::cout << "-- Writing CDR..." << std::endl;
    zpkWriter.writeCDR();
    std::cout << "-- Writing EOCDR..." << std::endl;
    zpkWriter.writeEOCDR();

    steady_clock::time_point endTime = steady_clock::now();
    duration<float> timeElapsed = duration_cast<milliseconds>(endTime - startTime);
    printCreateStats(timeElapsed.count(), fileList.size(),
                     zpkWriter.getUncompSize(), zpkWriter.getCompSize());

    return 0;
}

int unpackArchive(const std::string &filename, const std::string &outputFolder)
{
    steady_clock::time_point startTime = steady_clock::now();

    ZPack::Reader zpkReader;
    if (zpkReader.openFile(filename) != ZPack::OK)
    {
        std::cerr << "FATAL: File unreadable or invalid." << std::endl;
        return 1;
    }

    std::cout << "-- Unpacking files..." << std::endl;
    std::ofstream outputFile;
    ZPack::EntryList entryList = zpkReader.getEntryList();
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
        
        if (zpkReader.unpackFile(fileInfo, outputFile) != ZPack::OK)
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

int getNumDigits(int n)
{
    int numDigits = 0;

    do {
        ++numDigits; 
        n /= 10;
    } while (n);

    return numDigits;
}

void printArchiveEntry(uint64_t uncompSize, uint64_t compSize, const std::string &filename)
{
    std::string ucsSpace(13 - getNumDigits(uncompSize), ' ');
    std::string csSpace(13 - getNumDigits(compSize), ' ');

    std::cout <<
        uncompSize << ucsSpace <<
        compSize << csSpace <<
        filename <<
    std::endl;
}

int listArchive(const std::string &filename)
{
    ZPack::Reader zpkReader;
    if (zpkReader.openFile(filename) != ZPack::OK)
    {
        std::cerr << "FATAL: File unreadable or invalid." << "\n";
        return 1;
    }

    std::cout << "Size         Compressed   Name\n" <<
                 "-----------  -----------  -------------------" <<
    std::endl;

    ZPack::EntryList entryList = zpkReader.getEntryList();
    for (auto fileInfo: entryList)
    {
        printArchiveEntry(fileInfo->uncompSize,
                          fileInfo->compSize,
                          fileInfo->filename);
    }

    std::cout << "-----------  -----------  -------------------" << std::endl;
    printArchiveEntry(zpkReader.getUncompSize(),
                      zpkReader.getCompSize(),
                      std::to_string(entryList.size()) + " file(s)");

    return 0;
}

// Arguments
enum {
    ARG_CREATE,
    ARG_UNPACK,
    ARG_OUTPUT,
    ARG_LIST,
    ARG_FAST,
    ARG_HELP,
    ARG_VERSION,
};

static std::unordered_map<std::string, int> argMap = {
    {"-c",       ARG_CREATE},
    {"--create", ARG_CREATE},
    {"-u",       ARG_UNPACK},
    {"--unpack", ARG_UNPACK},
    {"-o",       ARG_OUTPUT},
    {"--output", ARG_OUTPUT},
    {"-l",       ARG_LIST},
    {"--list",   ARG_LIST},
    {"-f",       ARG_FAST},
    {"--fast",   ARG_FAST},
};

int main(int argc, char **argv)
{
    int mode = 0;
    std::string outputFolder = ".";
    PathList pathList;
    std::string filename = "";
    int compressionLevel = 3;
    
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string argStr(argv[i]);

            if (argc > 2 && i == argc - 1)
            {
                filename = argStr;
                break;
            }

            auto it = argMap.find(argStr);
            if (it != argMap.end())
            {
                switch (it->second)
                {
                case ARG_CREATE:
                    mode = 1;
                    continue;
                case ARG_UNPACK:
                    mode = 2;
                    continue;
                case ARG_OUTPUT:
                    outputFolder = argv[++i];
                    continue;
                case ARG_LIST:
                    mode = 3;
                    continue;
                case ARG_FAST:
                {
                    int level = atoi(argv[++i]);
                    if (level >= 1 && level <= 22)
                        compressionLevel = -level;
                    else {
                        std::cout << "Invalid argument: " << argStr << "\n";
                        printUsage();
                        return 1;
                    }
                    continue;
                }
                case ARG_HELP:
                    printUsage();
                    return 0;
                case ARG_VERSION:
                    std::cout <<
                        PROGRAM_NAME "\n"
                        "Copyright (c) 2020 LeadRDRK. Licensed under the BSD 3-Clause license.\n";
                    return 0;
                }
            }
            else if (std::isdigit(argStr[1])) {
                int level = -atoi(argStr.c_str());
                if (level >= 1 && level <= 22)
                    compressionLevel = level;
                else {
                    std::cout << "Invalid argument: " << argStr << "\n";
                    printUsage();
                    return 1;
                }
            }
            else if (mode == 1)
            {
                pathList.push_back(argStr);
            }
            else {
                std::cout << "Invalid argument: " << argStr << "\n";
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

    switch (mode)
    {
        case 1: // create mode
            return createArchive(pathList, filename, compressionLevel);
            break;

        case 2: // unpack mode
            return unpackArchive(filename, outputFolder);
            break;
        
        case 3: // list mode
            return listArchive(filename);
            break;
    }
    return 0;
}
