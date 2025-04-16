#include <userinter/shell.h>
#include <io/screen.h>
#include <drivers/keyboard.h>
#include <filesystem/fat.h>
#include <common/str.h>
#include <memorymanagement.h>
#define INPUTBUFFERSIZE 512

void output_write(char* line) {
    terminal_write_string(line);
}

void output_write_line(char* line) {
    terminal_write_string(line);
    terminal_write_string("\n");
}

/// @brief outputs the prompt
void output_prompt() {
    output_write_line("");
    output_write("> ");
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
            free(line);
            return tokens;
        }
        
        token = strtok(NULL, " ");
    }
    tokens[position] = NULL;
    free(line);
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
        output_write_line("Usage: mkdir <directory path> <directory name>");
        return;
    }
    if(argc == 2)
        create_dir(hd, "", argv[1], partDesc);
    else
        create_dir(hd, argv[1], argv[2], partDesc);
}

/// @brief removes a directory and all its contents
/// @param argc 
/// @param argv 
void cmd_rmdir(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: rmdir <directory path> <directory name>");
        return;
    }
    if(argc == 2)
        delete_dir(hd, "", argv[1], partDesc);
    else
        delete_dir(hd, argv[1], argv[2], partDesc);
}

/// @brief removes a file
/// @param argc 
/// @param argv 
void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        output_write_line("Usage: rm <file path> <file name>");
        return;
    }

    if(argc == 2)
        delete_file(hd, "", argv[1], partDesc);
    else
        delete_file(hd, argv[1], argv[2], partDesc);
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
        output_write("Usage: cat <file path> <file name>\n");
        return;
    }
    if(argc == 2)
        read_file(hd, "", argv[1], partDesc);
    else
        read_file(hd, argv[1], argv[2], partDesc);
}

/// @brief creates a new file
/// @param argc 
/// @param argv 
void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        output_write("Usage: touch <file path> <file name>\n");
        return;
    }
    if(argc == 2)
        create_file(hd, "", argv[1], partDesc);
    else
        create_file(hd, argv[1], argv[2], partDesc);
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
        char** args = split_line(get_input_line());

        //calcs argc
        char** temp = args;
        int argc = 0;
        for(; *temp; temp++) {
            argc++;
        }

        if (!argc) {
            free(args);
            return;
        }
        // finds whcih command to exec and execs it
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

        free(args);
    }
}