#pragma once
#include <cstdlib>
#include <cstdint>

class CRC32
{
    public:
        CRC32();
        void add(const char* buf, size_t size);
        uint32_t digest();

    private:
        uint32_t crc;
        
};