#ifndef __MEMORY_CACHE_H__
#define __MEMORY_CACHE_H__

#include "common.h"

#define CACHE_BLOCK_SIZE 64
#define L1_CACHE_SIZE (64 * 1024)
#define L1_CACHE_WAYS 8
#define L1_CACHE_LINES (L1_CACHE_SIZE / CACHE_BLOCK_SIZE)
#define L2_CACHE_SIZE (4 * 1024 * 1024)
#define L2_CACHE_WAYS 16
#define L2_CACHE_LINES (L2_CACHE_SIZE / CACHE_BLOCK_SIZE)

void init_cache(void);
uint32_t cache_read(hwaddr_t addr, size_t len);
void cache_write(hwaddr_t addr, size_t len, uint32_t data);

#endif
