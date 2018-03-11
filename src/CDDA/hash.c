#include "hash.h"
#include "array.h"

hash_t hash_create(void){
	hash_t result = {0};
	return result;
}
void hash_destroy(hash_t* hash){
	array_free(hash->keys);
	array_free(hash->values);

	*hash = hash_create();
}
	
uint64_t hash_lookup(const hash_t* hash, uint64_t k, uint64_t default_value){
	for (uint32_t n = 0; n < hash->size; ++n){
		if (hash->keys[n] == k){
			return hash->values[n];
		}
	}

	return default_value;
}
void hash_add(hash_t* hash, uint64_t k, uint64_t value){
	array_push(hash->keys, k);
	array_push(hash->values, value);
	++hash->size;
}
