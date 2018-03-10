#ifndef H_MEMORY_H
#define H_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif
	void alloc_main_memory(int bytes);
	void free_main_memory(void);

	void* reallocate(void* previous, int oldsize, int newsize);
	void mem_clear(void* p, char v, int sz);
#ifdef __cplusplus
}
#endif

#endif