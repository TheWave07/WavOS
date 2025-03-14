#include <common/str.h>
#include <memorymanagement.h>
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

/// @brief finds the first occurence of a char in a string
/// @param str 
/// @param c 
/// @return a pointer to the first occurence
char *strchr(const char *str, char c) {
    for (; *str; str++) {
        if(*(str) == c)
            return str;
    }

    return NULL;
}


char toupper(char c) {
    if((c >= 'a') && (c <= 'z'))
        return (c - ('a' - 'A'));
    return c;
}

/// @brief splits string by specifed delimeter
/// @param str
/// @param delim 
/// @return the next part of the string that was split
char* strtok(char *str, const char *delim) {
    static char *next = NULL;

    // init on first call
    if(str)
        next = str;
    
    // return null when there are no more tokens
    if(!next)
        return NULL;

    while (*next && strchr(delim, *next)) 
        next++;

    if(*next == '\0')
        return NULL;
    
    char *start = next;
    
    while (*next && !strchr(delim, *next)) 
        next++;
    
    if (*next) { 
        *next = '\0';
        next++;
    }

    return start;
}

/// @brief concats two strings
/// @param str1 
/// @param str2 
/// @return the concat string, uses malloc to alloc mem
char* concat(char *str1, char *str2) {
    char* res = (char*) malloc((strlen(str1) + strlen(str2)) * sizeof(char) + 1);
    int i = 0;
    for(; *str1; i++)
        res[i] = *(str1++);
    
    for(; *str2; i++)
        res[i] = *(str2++);
    
    res[i] = '\0';

    return res;
}