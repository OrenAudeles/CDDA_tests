#include "split_mix64.h"

split_mixer split_mix_make(uint64_t seed){
	split_mixer result = {seed};
	return result;
}
uint64_t split_mix_next(split_mixer* gen){
	uint64_t z = (gen->s += UINT64_C(0x9E3779B97F4A7C15));
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return z ^ (z >> 31);
}