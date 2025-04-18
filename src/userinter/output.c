#include <userinter/output.h>
#include <common/str.h>
#include <common/tools.h>

/// @brief prints specifed data to stdout
/// @param data the buffer containing the data
/// @param size the size of the data
void print(char* data, int size) {
    asm("int $0x80" : : "a" (3), "b" (data), "c" (size));
}


/// @brief prints the specfied string using syscall
/// @param str 
void print_string(char* str) {
    print(str, strlen(str));
}

/// @brief writes a specifed int to stdout
/// @param i the int
/// @param base the base in which to write it
void print_int(int i, int base)
{
    char buf[100];
	itoa(i,buf,base);
    print_string(buf);
}

/// @brief changes stdout to a specifed file
/// @param dirPath the path of the dir the file is located in
/// @param fileName the name of the file
/// @param part_desc 
/// @param hd 
/// @param rewrite to rewrite or addon
void change_stdout_to_file(char* dirPath, char* fileName, partition_descr* part_desc, ata_drive hd, bool rewrite) {
    asm("int $0x80" : : "a" (2), "b" (dirPath), "c" (fileName), "d" (part_desc), "S" (&hd), "D" (rewrite));
}

/// @brief changes stdout to screen
void change_stdout_to_screen() {
    asm("int $0x80" : : "a" (1));
}