#ifndef H_SPLIT_MIX_H
#define H_SPLIT_MIX_H

#include <inttypes.h>

typedef struct split_mixer{
	uint64_t s;
} split_mixer;


#ifdef __cplusplus
extern "C" {
#endif

split_mixer split_mix_make(uint64_t seed);
uint64_t split_mix_next(split_mixer* gen);

#ifdef __cplusplus
}
#endif


#endif