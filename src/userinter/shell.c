#include <userinter/shell.h>
#include <userinter/output.h>
#include <io/screen.h>
#include <drivers/keyboard.h>
#include <filesystem/fat.h>
#include <common/str.h>
#include <memorymanagement.h>
#define INPUTBUFFERSIZE 512
#define TOKENBUFFSIZE 64

ata_drive hd;
partition_descr *partDesc;

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
    output_write("WavOS:");
    output_write(partDesc->CWDString);
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

/// @brief displays all shell commands and a description of them
void cmd_help() {
    output_write_line("Available commands:");
    output_write_line("  help         - Show this help message");
    output_write_line("  ls           - List directory contents");
    output_write_line("  cd <path>    - Change current directory");
    output_write_line("  mkdir <name> - Create a new directory");
    output_write_line("  rmdir <dir>  - Delete a directory");
    output_write_line("  touch <file> - Create an empty file");
    output_write_line("  cat <file>   - Show contents of a file");
    output_write_line("  echo <text>  - Print text");
    output_write_line("  rm <file>    - Delete a file");
    
}

/// @brief writes the contents of the dir
/// @param argc count of args
/// @param argv the args
/// @param hd 
/// @param partDesc 
void cmd_ls(int argc, char** argv) {
    if (argc == 1)
        read_dir(hd, "", partDesc);
    else
        read_dir(hd, argv[1], partDesc);
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
void cmd_clear() {
    terminal_init();
}

void start_shell(ata_drive hardDrive, partition_descr *partDescriptor) {
    hd = hardDrive;
    partDesc = partDescriptor;
    bool cont = true;
    output_write_line("");
    output_write_line ("         _______         _______ _______ ");
    output_write_line ("|\\     /(  ___  )\\     /(  ___  |  ____ \\");
    output_write_line ("| )   ( | (   ) | )   ( | (   ) | (    \\/");
    output_write_line ("| | _ | | (___) | |   | | |   | | (_____ ");
    output_write_line ("| |( )| |  ___  ( (   ) ) |   | (_____  )");
    output_write_line ("| || || | (   ) |\\ \\_/ /| |   | |     ) |");
    output_write_line ("| () () | )   ( | \\   / | (___) /\\____) |");
    output_write_line ("(_______)/     \\|  \\_/  (_______)_______)");
    output_write_line ("Versio: 1.0");
    
    while(cont) {
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

        // Checks for shell redirection
        for (int i = 1; i < (argc - 1); i++) {
            if (strcmp(args[i], ">") == 0) { // if redirection
                char* fileName = split_path(args[i + 1]);
                if (strcmp(fileName, args[i + 1])) {
                    if(*(args[i + 1])) { // if first char was /
                        change_stdout_to_file(args[i + 1], fileName, partDesc, hd, true);
                    } else {
                        change_stdout_to_file("/", fileName, partDesc, hd, true);
                    }
                } else {
                    change_stdout_to_file("", fileName, partDesc, hd, true);
                }
                argc = i;
                break;
            } else if ((strcmp(args[i], ">>") == 0)) {
                char* fileName = split_path(args[i + 1]);
                if (strcmp(fileName, args[i + 1])) {
                    if(*(args[i + 1])) { // if first char was /
                        change_stdout_to_file(args[i + 1], fileName, partDesc, hd, false);
                    } else {
                        change_stdout_to_file("/", fileName, partDesc, hd, false);
                    }
                } else {
                    change_stdout_to_file("", fileName, partDesc, hd, false);
                }
                argc = i;
                break;
            }
        }
        // finds which command to exec and execs it
        if (strcmp(args[0], "help") == 0) {
            cmd_help();
        }
        else if (strcmp(args[0], "ls") == 0) {
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
            cmd_clear();
        } else {
            output_write("Unknown command: ");
            output_write_line(args[0]);
        }

        change_stdout_to_screen();
        free(input);
        free(args);
    }
}