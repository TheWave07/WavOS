#include <stdout.h>
#include <common/str.h>
#include <memorymanagement.h>
#include <io/screen.h>
stdout_desc stdout;
/// @brief sets stdout to terminal
void set_stdout_to_terminal() {
    stdout.mode = STDOUT_SCREEN;
}

/// @brief sets stdout to file type
/// @param dirPath the path of the dir the file is located in
/// @param fileName the name of the file
/// @param part_desc 
/// @param hd 
/// @param rewrite to rewrite file or add to it
void set_stdout_to_file(char* dirPath, char* fileName, partition_descr* part_desc, ata_drive hd, bool rewrite) {
    if (is_file_exist(hd, dirPath, fileName, part_desc)) {
        free(stdout.dirPath);
        free(stdout.fileName);
        
        stdout.mode = STDOUT_FILE;
        stdout.dirPath = (char*) malloc(strlen(dirPath) + 1);
        strcpy(stdout.dirPath, dirPath);
        stdout.fileName = (char*) malloc(strlen(fileName) + 1);
        strcpy(stdout.fileName, fileName);
        stdout.part_desc = part_desc;
        stdout.hd = hd;
        stdout.rewrite = rewrite;
    } else {
        terminal_write_string("no such file or directory");
    }
}

void set_stdout_rewrite(bool rewrite) {
    stdout.rewrite = rewrite;
}
stdout_desc get_stdout() {
    return stdout;
}