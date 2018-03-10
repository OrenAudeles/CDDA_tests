#ifndef H_FILE_H
#define H_FILE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
	int file_size(const char* path);
	int file_read(const char* path, void* mem, uint32_t max_bytes);
	int file_write(const char* path, void* mem, uint32_t write_bytes);
#ifdef __cplusplus
}
#endif

#endif