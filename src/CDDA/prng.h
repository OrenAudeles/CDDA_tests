#ifndef H_PRNG_H
#define H_PRNG_H

#include <inttypes.h>

typedef struct xorshiro128plus{
	uint64_t s[2];
} xorshiro128plus;

#ifdef __cplusplus
extern "C" {
#endif

xorshiro128plus prng_make(uint64_t s0, uint64_t s1);

uint64_t prng_next(xorshiro128plus* rand);
void     prng_jump(xorshiro128plus* rand);

#ifdef __cplusplus
}
#endif

#endif