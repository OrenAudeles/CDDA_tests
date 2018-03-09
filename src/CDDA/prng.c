#include "prng.h"


static inline uint64_t rotl(const uint64_t x, int k){
	return (x << k) | (x >> (64 - k));
}

xorshiro128plus prng_make(uint64_t s0, uint64_t s1){
	xorshiro128plus result;
	result.s[0] = s0;
	result.s[1] = s1;

	return result;
}

uint64_t prng_next(xorshiro128plus* rand){
	const uint64_t s0 = rand->s[0];
	uint64_t s1 = rand->s[1];

	const uint64_t ret = s0 + s1;

	s1 ^= s0;
	rand->s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14); // a, b
	rand->s[1] = rotl(s1, 36);

	return ret;
}
void     prng_jump(xorshiro128plus* rand){
	static const uint64_t JUMP[] = {0xBEAC0467EBA5FACB, 0xD86B048B86AA9922};

	uint64_t s0 = 0, s1 = 0;
	// Let's just unroll our jump loop
	for (int b = 0; b < 64; ++b){
		if (JUMP[0] & UINT64_C(1) << b){
			s0 ^= rand->s[0];
			s1 ^= rand->s[1];
		}
		prng_next(rand);
	}
	for (int b = 0; b < 64; ++b){
		if (JUMP[1] & UINT64_C(1) << b){
			s0 ^= rand->s[0];
			s1 ^= rand->s[1];
		}
		prng_next(rand);
	}
	
	rand->s[0] = s0;
	rand->s[1] = s1;
}
