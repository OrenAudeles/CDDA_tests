#include "file.h"

#include <sys/stat.h>
#include <stdio.h>

int file_size(const char* path){
	struct stat statbuf;
	int sz = 0;

	if (stat(path, &statbuf) != -1){
		sz = (int)statbuf.st_size;
	}
	return sz;
}

int file_read(const char* path, void* mem, uint32_t max_bytes){
	FILE *f = fopen(path, "rb");
	int bytes = -1;
	if (f){
		bytes = file_size(path);
		bytes = bytes > max_bytes ? max_bytes : bytes;
		bytes = fread(mem, bytes, 1, f);

		fclose(f);
	}
	return bytes;
}
int file_write(const char* path, void* mem, uint32_t write_bytes){
	FILE *f = fopen(path, "wb");
	int bytes = -1;
	if (f){
		bytes = write_bytes;
		int written = fwrite(mem, 1, bytes, f);

		if (written != bytes){
			/* error! */
			bytes = written;
		}
		fclose(f);
	}
	return bytes;
}