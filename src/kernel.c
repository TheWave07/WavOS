#include <io/screen.h>
#include <common/str.h>
#include <common/tools.h>
#include <gdtdesc.h>
#include <hardwarecomms/idtdesc.h>
#include <hardwarecomms/portio.h>
#include <drivers/keyboard.h>
#include <hardwarecomms/pci.h>
#include <memorymanagement.h>
#include <drivers/ata.h>
#include <userinter/shell.h>
#include <userinter/output.h>
#include <stdout.h>
#include <filesystem/msdospart.h>
#include <filesystem/fat.h>
#include <multitasking.h>

void boot_log(const char* msg, bool ok) {
    terminal_write_string("[INFO] ");
    terminal_write_string(msg);

    // Pad with spaces until about column 40
    int len = 7 + strlen(msg); // [INFO] + msg length
    for (int i = len; i < 40; ++i) {
        terminal_write(" ", 1);
    }

    if (ok) {
        terminal_write_string("[OK]\n");
    } else {
        terminal_write_string("[FAIL]\n");
    }
}

int kmain(void *mbd, unsigned int magic){
    terminal_init();
    if (magic!=0x2BADB002) {
        terminal_write_string("[BOOT] Invalid multiboot header.\n");
        return -1;
    }
    terminal_write_string("[BOOT] Multiboot header valid.\n");

    boot_log("Initializing GDT...", true);
    gdt_setup();

    boot_log("Setting up heap...", true);
    uint32_t* memupper = (uint32_t*)(((size_t)mbd) + 8);
    size_t heap = 10*1024*1024;
    init_memory(heap, (*memupper)*1024 - heap - 10*1024);

    kb_init();
    boot_log("Initializing keyboard...", kb_self_test());

    boot_log("Initializing IDT...", true);
    idt_setup();

    boot_log("Enabling interrupts...", true);
    interrupts_activate();

    boot_log("Initializing ATA device...", true);
    ata_drive ataSlave = create_ata(false, 0x1F0);
    identify(ataSlave);
    
    boot_log("Loading partitions...", true);
    partition_descr *part_descriptors = read_partitions(ataSlave);

    boot_log("Starting shell...", true);
    start_shell(ataSlave, part_descriptors);
    

    return 0;
}
