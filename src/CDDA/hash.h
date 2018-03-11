#ifndef H_STATIC_HASH_H
#define H_STATIC_HASH_H

#include "common.h"

typedef struct hash_t{
	uint32_t size;
	/** stretchy */uint64_t* keys;
	/** stretchy */uint64_t* values;
} hash_t;

#ifdef __cplusplus
extern "C" {
#endif
	hash_t hash_create(void);
	void hash_destroy(hash_t* hash);
	
	uint64_t hash_lookup(const hash_t* hash, uint64_t k, uint64_t default_value);
	void hash_add(hash_t* hash, uint64_t k, uint64_t value);

#ifdef __cplusplus
}
#endif

#endif