#include "common.h"
#include "memory/cache.h"

uint32_t dram_read(hwaddr_t addr, size_t len);
void dram_write(hwaddr_t addr, size_t len, uint32_t data);

#define CACHE_OFFSET_BITS 6
#define L1_SET_COUNT (L1_CACHE_LINES / L1_CACHE_WAYS)
#define L1_INDEX_BITS 7
#define L2_SET_COUNT (L2_CACHE_LINES / L2_CACHE_WAYS)
#define L2_INDEX_BITS 12

typedef struct {
	bool valid;
	uint32_t tag;
	uint8_t data[CACHE_BLOCK_SIZE];
} L1CacheLine;

typedef struct {
	bool valid;
	bool dirty;
	uint32_t tag;
	uint8_t data[CACHE_BLOCK_SIZE];
} L2CacheLine;

static L1CacheLine l1_cache[L1_CACHE_LINES];
static L2CacheLine l2_cache[L2_CACHE_LINES];
static uint32_t replacement_state;

static uint32_t next_random(void) {
	replacement_state ^= replacement_state << 13;
	replacement_state ^= replacement_state >> 17;
	replacement_state ^= replacement_state << 5;
	return replacement_state;
}

static L2CacheLine *l2_lookup(hwaddr_t addr) {
	uint32_t set = (addr >> CACHE_OFFSET_BITS) & (L2_SET_COUNT - 1);
	uint32_t tag = addr >> (CACHE_OFFSET_BITS + L2_INDEX_BITS);
	uint32_t i;

	for(i = 0; i < L2_CACHE_WAYS; i ++) {
		L2CacheLine *line = &l2_cache[set * L2_CACHE_WAYS + i];
		if(line->valid && line->tag == tag) {
			return line;
		}
	}
	return NULL;
}

static void l2_write_back(uint32_t set, L2CacheLine *line) {
	hwaddr_t block_addr;
	uint32_t offset;

	if(!line->valid || !line->dirty) {
		return;
	}
	block_addr = ((line->tag << L2_INDEX_BITS) | set)
		<< CACHE_OFFSET_BITS;
	for(offset = 0; offset < CACHE_BLOCK_SIZE; offset += 4) {
		uint32_t word;
		memcpy(&word, line->data + offset, sizeof(word));
		dram_write(block_addr + offset, sizeof(word), word);
	}
	line->dirty = false;
}

static void l2_fill(hwaddr_t block_addr, L2CacheLine *line) {
	uint32_t offset;

	for(offset = 0; offset < CACHE_BLOCK_SIZE; offset += 4) {
		uint32_t word = dram_read(block_addr + offset, sizeof(word));
		memcpy(line->data + offset, &word, sizeof(word));
	}
}

static L2CacheLine *l2_get_line(hwaddr_t addr) {
	uint32_t set = (addr >> CACHE_OFFSET_BITS) & (L2_SET_COUNT - 1);
	uint32_t tag = addr >> (CACHE_OFFSET_BITS + L2_INDEX_BITS);
	uint32_t i;
	L2CacheLine *line = l2_lookup(addr);

	if(line != NULL) {
		return line;
	}
	for(i = 0; i < L2_CACHE_WAYS; i ++) {
		line = &l2_cache[set * L2_CACHE_WAYS + i];
		if(!line->valid) {
			break;
		}
	}
	if(i == L2_CACHE_WAYS) {
		i = next_random() & (L2_CACHE_WAYS - 1);
		line = &l2_cache[set * L2_CACHE_WAYS + i];
	}
	l2_write_back(set, line);
	l2_fill(addr & ~(CACHE_BLOCK_SIZE - 1), line);
	line->valid = true;
	line->dirty = false;
	line->tag = tag;
	return line;
}

static void l2_read(hwaddr_t addr, size_t len, uint8_t *dest) {
	while(len > 0) {
		uint32_t offset = addr & (CACHE_BLOCK_SIZE - 1);
		size_t part = CACHE_BLOCK_SIZE - offset;
		L2CacheLine *line;

		if(part > len) {
			part = len;
		}
		line = l2_get_line(addr);
		memcpy(dest, line->data + offset, part);
		addr += part;
		dest += part;
		len -= part;
	}
}

static void l2_write(hwaddr_t addr, size_t len, const uint8_t *src) {
	while(len > 0) {
		uint32_t offset = addr & (CACHE_BLOCK_SIZE - 1);
		size_t part = CACHE_BLOCK_SIZE - offset;
		L2CacheLine *line;

		if(part > len) {
			part = len;
		}
		/* L2 uses write allocate and write back. */
		line = l2_get_line(addr);
		memcpy(line->data + offset, src, part);
		line->dirty = true;
		addr += part;
		src += part;
		len -= part;
	}
}

static L1CacheLine *l1_lookup(hwaddr_t addr) {
	uint32_t set = (addr >> CACHE_OFFSET_BITS) & (L1_SET_COUNT - 1);
	uint32_t tag = addr >> (CACHE_OFFSET_BITS + L1_INDEX_BITS);
	uint32_t i;

	for(i = 0; i < L1_CACHE_WAYS; i ++) {
		L1CacheLine *line = &l1_cache[set * L1_CACHE_WAYS + i];
		if(line->valid && line->tag == tag) {
			return line;
		}
	}
	return NULL;
}

static L1CacheLine *l1_get_line(hwaddr_t addr) {
	uint32_t set = (addr >> CACHE_OFFSET_BITS) & (L1_SET_COUNT - 1);
	uint32_t tag = addr >> (CACHE_OFFSET_BITS + L1_INDEX_BITS);
	uint32_t i;
	L1CacheLine *line = l1_lookup(addr);

	if(line != NULL) {
		return line;
	}
	for(i = 0; i < L1_CACHE_WAYS; i ++) {
		line = &l1_cache[set * L1_CACHE_WAYS + i];
		if(!line->valid) {
			break;
		}
	}
	if(i == L1_CACHE_WAYS) {
		i = next_random() & (L1_CACHE_WAYS - 1);
		line = &l1_cache[set * L1_CACHE_WAYS + i];
	}
	l2_read(addr & ~(CACHE_BLOCK_SIZE - 1), CACHE_BLOCK_SIZE, line->data);
	line->valid = true;
	line->tag = tag;
	return line;
}

void init_cache(void) {
	uint32_t i;

	for(i = 0; i < L1_CACHE_LINES; i ++) {
		l1_cache[i].valid = false;
	}
	for(i = 0; i < L2_CACHE_LINES; i ++) {
		l2_cache[i].valid = false;
		l2_cache[i].dirty = false;
	}
	/* Zero is a fixed point of xorshift32, so use a non-zero seed. */
	replacement_state = 0x6d2b79f5u;
}

uint32_t cache_read(hwaddr_t addr, size_t len) {
	uint32_t result = 0;
	uint8_t *dest = (uint8_t *)&result;

	assert(len == 1 || len == 2 || len == 4);
	while(len > 0) {
		uint32_t offset = addr & (CACHE_BLOCK_SIZE - 1);
		size_t part = CACHE_BLOCK_SIZE - offset;
		L1CacheLine *line;

		if(part > len) {
			part = len;
		}
		line = l1_get_line(addr);
		memcpy(dest, line->data + offset, part);
		addr += part;
		dest += part;
		len -= part;
	}
	return result;
}

void cache_write(hwaddr_t addr, size_t len, uint32_t data) {
	const uint8_t *src = (const uint8_t *)&data;

	assert(len == 1 || len == 2 || len == 4);
	while(len > 0) {
		uint32_t offset = addr & (CACHE_BLOCK_SIZE - 1);
		size_t part = CACHE_BLOCK_SIZE - offset;
		L1CacheLine *line;

		if(part > len) {
			part = len;
		}
		/* L1 writes through to L2, but an L1 miss does not allocate. */
		line = l1_lookup(addr);
		if(line != NULL) {
			memcpy(line->data + offset, src, part);
		}
		l2_write(addr, part, src);
		addr += part;
		src += part;
		len -= part;
	}
}
