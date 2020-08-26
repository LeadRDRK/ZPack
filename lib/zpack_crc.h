#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

void crc32_init(uint32_t *crc);
void crc32_add(const void *buf, size_t size, uint32_t *crc);
void crc32_finalize(uint32_t *crc);

#ifdef __cplusplus
}
#endif