#include <multitasking.h>
#include <io/screen.h>
/// @brief handles syscall
/// @param regs the registers of the cpu when the sys was called
void handle_syscall(registers_t regs) {

    switch (regs.eax)
    {
    case 4:
        terminal_write_string((char*) regs.ebx);
        break;
    
    default:
        break;
    }
}