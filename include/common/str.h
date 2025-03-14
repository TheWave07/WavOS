#ifndef __WAVOS__COMMON__STR_H
#define __WAVOS__COMMON__STR_H
#include <common/types.h>

int strlen(const char *str);
void strcpy(char *dest, const char *src);
int strcmp(const char *str1, const char *str2);
char* strchr(const char *str, char c);
char toupper(char c);
char* strtok(char *str, const char *delim);
char* concat(char* str1, char* str2);
#endif