#include <multitasking.h>
#include <io/screen.h>
#include <stdout.h>
#include <filesystem/fat.h>
#include <common/str.h>
#include <memorymanagement.h>
/// @brief writes data to stdout
/// @param data data buff
/// @param size the size of the data
void sys_write(char* data, size_t size) {
    stdout_desc stdout = get_stdout();
    if(stdout.mode == STDOUT_SCREEN) {
        terminal_write(data, size);
    } else {
        write_to_file(stdout.hd, stdout.dirPath, stdout.fileName, data, size, stdout.rewrite, stdout.part_desc);
        set_stdout_rewrite(false);
    }

}
/// @brief handles syscall
/// @param regs the registers of the cpu when the sys was called
void handle_syscall(registers_t regs) {

    switch (regs.eax)
    {
    case 1:
        set_stdout_to_terminal();
        break;
    case 2:
        set_stdout_to_file((char*) regs.ebx, (char*) regs.ecx,  (partition_descr*) (regs.edx), *((ata_drive*)regs.esi), (bool) regs.edi);
        break;
    case 3:
        sys_write((char*) regs.ebx, regs.ecx);
        break;
    
    default:
        break;
    }
}