#pragma once
#include <cstdlib>
#include <cstdint>

class CRC32
{
    public:
        CRC32();
        void Add(char* buf, size_t size);
        uint32_t Get();
        operator uint32_t() { return Get(); };

    private:
        uint32_t crc;
        
};