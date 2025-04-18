#ifndef __WAVOS__USERINTER__OUTPUT_H
#define __WAVOS__USERINTER__OUTPUT_H
#include <drivers/ata.h>
#include <filesystem/msdospart.h>

void print(char* data, int size);
void print_string(char* str);
void print_int(int i, int base);
void change_stdout_to_file(char* dirPath, char* fileName, partition_descr* part_desc, ata_drive hd, bool rewrite);
void change_stdout_to_screen();
#endif