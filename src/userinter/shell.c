#include <userinter/shell.h>
#include <userinter/output.h>
#include <io/screen.h>
#include <drivers/keyboard.h>
#include <filesystem/fat.h>
#include <common/str.h>
#include <memorymanagement.h>
#define INPUTBUFFERSIZE 512

void output_write(char* line) {
    print_string(line);
}

void output_write_line(char* line) {
    print_string(line);
    print_string("\n");
}

/// @brief outputs the prompt
void output_prompt() {
    output_write_line("");
    output_write("> ");
}

/// @brief splits fileName from path
/// @param path the path with the fileName
/// @return the fileName also removes it from path string
char* split_path(char* path) {
    // Checks if path is only fileName
    if(!strchr(path, '/')) {
        return path;
    }
    char* last = path + strlen(path);

    // Find the start of this token
    while (last > path && !strchr("/", *(last - 1))) {
        last--;
    }
    *(last - 1) = '\0';
    return last;
}

/// @brief gets input form user
/// @return the line of input
char* get_input_line() {
    key_packet p;
    int position = 0;
    char* line = (char*) malloc(INPUTBUFFERSIZE * sizeof(char));
    while (1) {
        p = kb_fetch();
        if(p.printable)
            terminal_write(&p.value, 1);
        if(p.value == -2 && position > 0) {
            terminal_rem();
            position--;
        }

        if(p.value == '\n') {
            line[position++] = '\0';
            return line;
        } else {
            if(p.printable)
                line[position++] = p.value;
        }
        if(position >= INPUTBUFFERSIZE)
            return line;
    }
}

#define TOKENBUFFSIZE 64
char **split_line(char* line) {
    int position = 0;
    char** tokens = (char**) malloc(TOKENBUFFSIZE * sizeof(char*));
    
    char *token;

    token = strtok(line, " ");
    while(token != NULL) {
        tokens[position++] = token;

        if (position >= TOKENBUFFSIZE) {
            return tokens;
        }
        
        token = strtok(NULL, " ");
    }
    tokens[position] = NULL;
    return tokens;
}

ata_drive hd;
partition_descr *partDesc;

/// @brief writes the contents of the dir
/// @param argc count of args
/// @param argv the args
/// @param hd 
/// @param partDesc 
void cmd_ls(int argc, char** argv) {
    tree(hd, partDesc);
}

/// @brief changes the current working directory
/// @param argc 
/// @param argv 
void cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: cd <directory>");
        return;
    }

    change_current_working_dir(hd, argv[1], partDesc);
}

/// @brief creates a directory
/// @param argc 
/// @param argv 
void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: mkdir <directory path>");
        return;
    }

    char* fileName = split_path(argv[1]);
    if(strcmp(fileName, argv[1])) {
        if(*(argv[1])) { // if first char was /
            create_dir(hd, argv[1], fileName, partDesc);
        } else {
            create_dir(hd, "/", fileName, partDesc);
        }
    } else {
        create_dir(hd, "", argv[1], partDesc);
    }
}

/// @brief removes a directory and all its contents
/// @param argc 
/// @param argv 
void cmd_rmdir(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: rmdir <directory path>");
        return;
    }
    char* dirName = split_path(argv[1]);
    if(strcmp(dirName, argv[1])) {
        if(*(argv[1])) { // if first char was /
            delete_dir(hd, argv[1], dirName, partDesc);
        } else {
            delete_dir(hd, "/", dirName, partDesc);
        }
    } else {
        delete_dir(hd, "", argv[1], partDesc);
    }
}

/// @brief removes a file
/// @param argc 
/// @param argv 
void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: rm <file path>");
        return;
    }

    char* fileName = split_path(argv[1]);
    if(strcmp(fileName, argv[1])) {
        if(*(argv[1])) { // if first char was /
            delete_file(hd, argv[1], fileName, partDesc);
        } else {
            delete_file(hd, "/", fileName, partDesc);
        }
    } else {
        delete_file(hd, "", argv[1], partDesc);
    }
}


/// @brief prints message on screen
/// @param argc 
/// @param argv 
void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        output_write(argv[i]);
        output_write(" ");
    }
    output_write("\n");
}

/// @brief writes the contents of a file
/// @param argc 
/// @param argv 
void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: cat <file path>");
        return;
    }

    char* fileName = split_path(argv[1]);
    if(strcmp(fileName, argv[1])) {
        if(*(argv[1])) { // if first char was /
            read_file(hd, argv[1], fileName, partDesc);
        } else {
            read_file(hd, "/", fileName, partDesc);
        }
    } else {
        read_file(hd, "", argv[1], partDesc);
    }
}

/// @brief creates a new file
/// @param argc 
/// @param argv 
void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: touch <file path>");
        return;
    }
    char* fileName = split_path(argv[1]);
    if(strcmp(fileName, argv[1])) {
        if(*(argv[1])) { // if first char was /
            create_file(hd, argv[1], fileName, partDesc);
        } else {
            create_file(hd, "/", fileName, partDesc);
        }
    } else {
        create_file(hd, "", argv[1], partDesc);
    }
}

/// @brief clears screen
/// @param argc 
/// @param argv 
void cmd_clear(int argc, char** argv) {
    terminal_init();
}

void start_shell(ata_drive hardDrive, partition_descr *partDescriptor) {
    hd = hardDrive;
    partDesc = partDescriptor;
    terminal_init();
    while(1) {
        output_prompt();
        char* input = get_input_line();
        char** args = split_line(input);

        //calcs argc
        char** temp = args;
        int argc = 0;
        for(; *temp; temp++) {
            argc++;
        }

        if (!argc) {
            free(input);
            free(args);
            continue;
        }
        
        // finds which command to exec and execs it
        if (strcmp(args[0], "ls") == 0) {
            cmd_ls(argc, args);
        } else if (strcmp(args[0], "cd") == 0) {
            cmd_cd(argc, args);
        } else if (strcmp(args[0], "mkdir") == 0) {
            cmd_mkdir(argc, args);
        } else if (strcmp(args[0], "rmdir") == 0) {
            cmd_rmdir(argc, args);
        } else if (strcmp(args[0], "rm") == 0) {
            cmd_rm(argc, args);
        } else if (strcmp(args[0], "clear") == 0) {
            terminal_init();
        } else if (strcmp(args[0], "echo") == 0) {
            cmd_echo(argc, args);
        } else if (strcmp(args[0], "cat") == 0) {
            cmd_cat(argc, args);
        } else if (strcmp(args[0], "touch") == 0) {
            cmd_touch(argc, args);
        } else if (strcmp(args[0], "clear") == 0) {
            cmd_clear(argc, args);
        } else {
            output_write("Unknown command: ");
            output_write_line(args[0]);
        }

        free(input);
        free(args);
    }
}