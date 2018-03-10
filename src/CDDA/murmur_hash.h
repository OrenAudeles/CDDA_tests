#ifndef H_MURMUR_HASH_H
#define H_MURMUR_HASH_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
	/// Implementation of the 64 bit MurmurHash2 function
	/// http://murmurhash.googlepages.com/
	uint64_t murmur_hash_64(const void *key, uint32_t len, uint64_t seed);
#ifdef __cplusplus
}
#endif

#endif