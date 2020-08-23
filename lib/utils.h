// file reading/writing utils

#pragma once

#include <cstdint>
#include <fstream>

inline void WriteLE16(std::ofstream &p, uint16_t v)
{
    p << (uint8_t)v
      << (uint8_t)(v >> 8);
}

inline void WriteLE32(std::ofstream &p, uint32_t v)
{
    p << (uint8_t)v
      << (uint8_t)(v >> 8)
      << (uint8_t)(v >> 16)
      << (uint8_t)(v >> 24);
}

inline void WriteLE64(std::ofstream &p, uint64_t v)
{
    WriteLE32(p, (uint32_t)v);
    WriteLE32(p, (uint32_t)(v >> 32));
}

inline void ReadLE16(std::ifstream &file, uint16_t &out) { file.read(reinterpret_cast<char *>(&out), 2); }
inline void ReadLE32(std::ifstream &file, uint32_t &out) { file.read(reinterpret_cast<char *>(&out), 4); }
inline void ReadLE64(std::ifstream &file, uint64_t &out) { file.read(reinterpret_cast<char *>(&out), 8); }