#include <common/str.h>

int strlen(const char* str) {
    int len = 0;
    while(*str != '\0') {
		str++;
		len++;
	}

    return len;
}


/// @brief copies one string to another
/// @param dest the dest string
/// @param src the source string
void strcpy (char* dest, const char* src) {
	while(*src != '\0') {
		*dest = *(src++);
		dest++;
	}
	*dest = '\0';
}

int strcmp(const char* str1, const char* str2) {
    while(*str1 && (*str1 == *str2))
    {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}