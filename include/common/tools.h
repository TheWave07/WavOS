#ifndef __WAVOS__COMMON__TOOLS_H
#define __WAVOS__COMMON__TOOLS_H
#include <common/types.h>
void memset(unsigned char *dest, unsigned char val, unsigned int len);
void *memcpy(void *dest, const void *src, size_t n);
char * itoa( int value, char * str, int base );

#endif