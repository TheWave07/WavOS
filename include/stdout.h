#ifndef __WAVOS__STDOUT_H
#define __WAVOS__STDOUT_H

#include <common/types.h>
#include <drivers/ata.h>
#include <filesystem/fat.h>
typedef enum {
    STDOUT_SCREEN,
    STDOUT_FILE
} stdout_mode;

// a descriptor for stdout
typedef struct {
    stdout_mode mode; // file or terminal
    char* dirPath; // path of dir in which the file is located
    char* fileName; // the fileName
    partition_descr* part_desc;
    ata_drive hd;
    bool  rewrite; // to rewrite
} stdout_desc;

void set_stdout_to_terminal();
void set_stdout_to_file(char* dirPath, char* fileName, partition_descr* part_desc, ata_drive hd, bool rewrite);
void set_stdout_rewrite(bool rewrite);
stdout_desc get_stdout();
#endif