#include "memory.h"
#include "common.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


typedef struct {
	char type;
	int start;
	int size;
} MemoryBlock;

#define MEMORY_BLOCK_BYTES 4096
#define MAX_MEMORY_BLOCKS (MEMORY_BLOCK_BYTES / sizeof(MemoryBlock))
#define NULL_BLOCK 0
#define FREE_BLOCK 1
#define USED_BLOCK 2

typedef struct {
	MemoryBlock block[MAX_MEMORY_BLOCKS];
	int count;

	uint8_t* mem;
} MainMemory;

static MainMemory _main_mem = {0};

static void* alloc_block(int size, void* old);
static void free_block(void* m);
static void free_block_index(int ndx);
static void* realloc_block(void* old, int new_size);
static void consolidate_blocks(void);

static void init_main_mem(void){
	_main_mem.mem = NULL;
	_main_mem.count = 0;
}

void alloc_main_memory(int bytes){
	init_main_mem();

	_main_mem.mem = (uint8_t*)malloc(bytes);
	_main_mem.block[0].start = 0;
	_main_mem.block[0].size = bytes;
	_main_mem.block[0].type = FREE_BLOCK;
	++_main_mem.count;

	printf("Main Memory: %d Bytes @%p [%ld Max Blocks]\n", bytes, (void*)_main_mem.mem, MAX_MEMORY_BLOCKS);
}
void free_main_memory(void){
	// Assertion to make sure that all memory has been freed from main memory
	assert(_main_mem.count == 1 && _main_mem.block[0].type == FREE_BLOCK);

	free(_main_mem.mem);
	init_main_mem();
}

void mem_clear(void* p, char v, int sz){
	assert(p != 0);

	memset(p, v, sz);
}

void* reallocate(void* previous, int oldSize, int newSize){
	//return realloc(previous, newSize);
	return realloc_block(previous, newSize);
}


static void* alloc_block(int size, void* old){
	if (old == NULL){
		// Just find a block that is large enough to hold
		// [size] bytes
		for (int i = 0; i < _main_mem.count; ++i){
			if (_main_mem.block[i].type == FREE_BLOCK){
				if (_main_mem.block[i].size > size){
					// Need to split
					int diff = _main_mem.block[i].size - size;
					_main_mem.block[i].size = size;
					_main_mem.block[i].type = USED_BLOCK;

					// Alloc a new block starting at
					// start + size
					// with size of diff
					_main_mem.block[_main_mem.count].start = _main_mem.block[i].start + size;
					_main_mem.block[_main_mem.count].size = diff;
					_main_mem.block[_main_mem.count].type = FREE_BLOCK;
					++_main_mem.count;

					return _main_mem.mem + _main_mem.block[i].start;
				}
				else if (_main_mem.block[i].size == size){
					// No split, just consume
					_main_mem.block[i].type = USED_BLOCK;

					return _main_mem.mem + _main_mem.block[i].start;
				}
			}
		}
	}
	// Old is non-null, so we need to see if we should
	// grow our current block or alloc a new block and move data
	else{
		// Find the current block
		int cblk;
		for (cblk = 0; cblk < _main_mem.count; ++cblk){
			if (_main_mem.block[cblk].type == USED_BLOCK &&
				_main_mem.block[cblk].start == ((uint8_t*)old - _main_mem.mem))
			{
				break;
			}
		}
		if (cblk < _main_mem.count){
			// Now to see if there is a FREE block immediately following this block,
			// and also with a size large enough to hold the additional size
			int extra_bytes = size - _main_mem.block[cblk].size;
			int next_blk_start = _main_mem.block[cblk].start + _main_mem.block[cblk].size;

			for (int i = 0; i < _main_mem.count; ++i){
				if (_main_mem.block[i].type == FREE_BLOCK &&
					_main_mem.block[i].start == next_blk_start)
				{
					if (_main_mem.block[i].size > extra_bytes){
						// We can expand, and don't need to kill the expansion block
						_main_mem.block[cblk].size += extra_bytes;
						_main_mem.block[i].size -= extra_bytes;
						_main_mem.block[i].start += extra_bytes;

						return old;
					}
					else if (_main_mem.block[i].size == extra_bytes){
						// Reduce block to size 0
						_main_mem.block[cblk].size += extra_bytes;
						_main_mem.block[i].size = 0;
						_main_mem.block[i].start = 0;
						_main_mem.block[i].type = NULL_BLOCK;

						consolidate_blocks();
						return old;
					}
				}
			}

			// By getting here, we know that we cannot just expand, so we have to find a
			// block that can be moved into.
			
			// Recurse to get a new block -- forcing a new block
			void* new_blk = alloc_block(size, NULL);
			// Move data
			memcpy(new_blk, old, _main_mem.block[cblk].size);
			// Free old block
			free_block(old);
			return new_blk;
		}
	}

	return NULL;
}
static void free_block(void* m){
	// Find the block that owns [m]
	for (int i = 0; i < _main_mem.count; ++i){
		if (_main_mem.block[i].type == USED_BLOCK &&
			_main_mem.block[i].start == ((uint8_t*)m - _main_mem.mem))
		{
			free_block_index(i);
		}
	}
}
static void free_block_index(int ndx){
	_main_mem.block[ndx].type = FREE_BLOCK;
	consolidate_blocks();
}
static void* realloc_block(void* old, int new_size){
	void* result = NULL;
	if (new_size == 0){
		free_block(old);
	}
	else{
		// See if we can grow the block that this is in
		result = alloc_block(new_size, old);
	}
	return result;
}
// Very very aggressive 'garbage collection'
static void consolidate_blocks(void){
	// Try to consolidate blocks until there are no more blocks able to be
	// consolidated...
	int consolidation = 1;
	while (consolidation > 0){
		consolidation = 0;
		for (int i = 0; i < _main_mem.count;){
			MemoryBlock iblk = _main_mem.block[i];
			if (iblk.type == FREE_BLOCK){
				for (int j = i + 1; j < _main_mem.count; ++j){
					MemoryBlock jblk = _main_mem.block[j];
					if (jblk.type == FREE_BLOCK && jblk.start == (iblk.start + iblk.size)){
						_main_mem.block[i].size += _main_mem.block[j].size;
						_main_mem.block[j].type = NULL_BLOCK;
						// block j will get cleaned up at the end of lifetime
						consolidation = 1;
					}
				}
			}
			else if (iblk.type == NULL_BLOCK){
				// Kill off the block
				_main_mem.block[i] = _main_mem.block[_main_mem.count-1];
				--_main_mem.count;
				// Force consolidation
				consolidation = 1;
				continue;
			}
			++i;
		}
	}
}