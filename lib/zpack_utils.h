// file reading/writing utils

#pragma once

#include <cstdint>
#include <fstream>

inline void writeLE16(std::ofstream &p, uint16_t v)
{
    p << (uint8_t)v
      << (uint8_t)(v >> 8);
}

inline void writeLE32(std::ofstream &p, uint32_t v)
{
    p << (uint8_t)v
      << (uint8_t)(v >> 8)
      << (uint8_t)(v >> 16)
      << (uint8_t)(v >> 24);
}

inline void writeLE64(std::ofstream &p, uint64_t v)
{
    writeLE32(p, (uint32_t)v);
    writeLE32(p, (uint32_t)(v >> 32));
}

inline void readLE16(std::ifstream &file, uint16_t &out) { file.read(reinterpret_cast<char *>(&out), 2); }
inline void readLE32(std::ifstream &file, uint32_t &out) { file.read(reinterpret_cast<char *>(&out), 4); }
inline void readLE64(std::ifstream &file, uint64_t &out) { file.read(reinterpret_cast<char *>(&out), 8); }

inline bool illegalFilename(const std::string& filename)
{
    return filename == ".." || filename == "." ||
           filename.find("../") != -1 || filename.find("./") != -1;
}