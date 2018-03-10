#ifndef H_ARRAY_H
#define H_ARRAY_H

#include "common.h"


typedef struct {
	uint32_t size;
	uint32_t capacity;
} array_header_t;

static void* array__grow(void* arr, int increment, int size);

#define array_header(a) \
	((array_header_t*)((char*)(a) - sizeof(array_header_t)))

#define array_size(a) \
	((a) ? array_header(a)->size : 0)
#define array_capacity(a) \
	((a) ? array_header(a)->capacity : 0)

#define array_push(a, item) \
	(array_maybe_grow(a, 1), (a)[array_header(a)->size++] = (item))

#define array_full(a) \
	(array_header(a)->size == array_header(a)->capacity)

#define array_free(a) \
	((a) ? reallocate(array_header(a), array_capacity(a), 0), 0 : 0)


#define array_need_grow(a, n) \
	((a) == 0 || array_size(a) + (n) >= array_capacity(a))
#define array_maybe_grow(a, n) \
	(array_need_grow(a, (n)) ? array_grow(a, (n)) : 0)

#define array_last(a) \
	((a)[array_size(a) - 1])

#define array_grow(a, n) \
	(*((void**)&(a)) = array__grow((a), (n), sizeof(*(a))))


#ifdef __cplusplus
extern "C" {
#endif
	void* reallocate(void* previous, int oldSize, int newSize);
#ifdef __cplusplus
}
#endif

static inline void* array__grow(void* arr, int increment, int size){
	void* result = 0;

	int next_cap = arr ? 2 * array_capacity(arr) : 8;
	int min_cap = array_size(arr) + increment;

	int m = next_cap > min_cap ? next_cap : min_cap;
	array_header_t* header = (array_header_t*)reallocate(arr ? array_header(arr) : arr, size * array_capacity(arr) + sizeof(array_header_t), size * m + sizeof(array_header_t));
	if (header){
		if (!arr){
			header->size = 0;
		}
		header->capacity = m;
		result = header + 1;
	}
	else{
		result = (void*)(sizeof(array_header_t));
	}

	return result;
}

#endif