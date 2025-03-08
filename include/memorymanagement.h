#ifndef __WAVOS__MEMORYMANAGEMENT_H
#define __WAVOS__MEMORYMANAGEMENT_H

#include <common/types.h>

void init_memory(size_t start, size_t size);
void* malloc(size_t size);
void* calloc(size_t n, size_t size);
void free(void* ptr);
#endif
