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


void taskA()
{
    terminal_write_string("A");
    while(1){
        key_packet p = kb_fetch();
        if(p.printable)
            terminal_write(&p.value, 1);
        if(p.value == -2)
            terminal_rem();
    }
}

void taskB()
{
    terminal_write_string("B");
    while(1) {
    }
}


void taskC()
{
    terminal_write_string("C");
    while(1) {
    }
}


/// @brief sets up the PIT's reload value
/// @param count the reload value
void set_pit_count(uint16_t count)
{

	outb(0x40, count & 0xFF); // Low byte
	outb(0x40, (count & 0xFF00) >> 8); // High byte
}

int kmain(void *mbd, unsigned int magic){
    terminal_init();
    if (magic!=0x2BADB002) {
        terminal_write_string("Invalid multiboot header.");
        return -1;
    }
    terminal_write_string("Hallo!\n");
    terminal_write_string("Setting Up The GDT.\n");
    gdt_setup();
    terminal_write_string("GDT Set.\n");
    terminal_write_string("Setting Up Heap.");
    uint32_t* memupper = (uint32_t*)(((size_t)mbd) + 8);
    size_t heap = 10*1024*1024;
    init_memory(heap, (*memupper)*1024 - heap - 10*1024);
    terminal_write_string("heap: 0x");
    terminal_write_int((heap >> 24) & 0xFF, 16);
    terminal_write_int((heap >> 16) & 0xFF, 16);
    terminal_write_int((heap >> 8 ) & 0xFF, 16);
    terminal_write_int((heap      ) & 0xFF, 16);

    void* allocated = malloc(1024);
    terminal_write_string("\nallocated: 0x");
    terminal_write_int(((size_t)allocated >> 24) & 0xFF, 16);
    terminal_write_int(((size_t)allocated >> 16) & 0xFF, 16);
    terminal_write_int(((size_t)allocated >> 8 ) & 0xFF, 16);
    terminal_write_int(((size_t)allocated      ) & 0xFF, 16);
    terminal_write_string("\n");
    free(allocated);

    terminal_write_string("setting up keyboard\n");
    kb_init();
    if (kb_self_test()) {
        terminal_write_string("keyboard set!\n");
    } else {
        terminal_write_string("fail\n");
    }
    select_drivers();

    set_pit_count(1000); // sets the time between task switches
    terminal_write_string("Setting Up The IDT.\n");
    idt_setup();
    terminal_write_string("IDT Set.\n");
    interrupts_activate();

    terminal_write_string("\nATA primary slave: ");
    ata_drive ataSlave = create_ata(false, 0x1F0);
    identify(ataSlave);
    

    partition_descr *part_descriptors = read_partitions(ataSlave);
    start_shell(ataSlave, part_descriptors);
    

    
    /*char lotsOWords[7002];
    memset(lotsOWords, 'B', 7000);
    lotsOWords[7000] = 'C';
    lotsOWords[7001] = '\0';*/
    //write_to_file(ataSlave, "dir1", "file1.txt", "this is file1 part1 dir1", 25, part_descriptors);
    //read_file(ataSlave, ".", "file1.txt", part_descriptors);
    //create_dir(ataSlave, "dir1/dir2", "dir3", part_descriptors);
    //create_file(ataSlave, "dir1/dir2/dir3", "file1.txt", part_descriptors);
    //write_to_file(ataSlave, "dir1/dir2/dir3", "file1.txt", "this is file1 part1 dir3", 25, part_descriptors);
    //read_file(ataSlave, "dir1/dir2/dir3", "file1.txt", part_descriptors);
    //delete_file(ataSlave, "dir1/dir2/dir3" , "file1.txt", part_descriptors);
    
    //tree(ataSlave, part_descriptors);
    //write_to_file(ataSlave, "", "file1.txt", "this is file1 part1", 20, false, part_descriptors);
    //read_file(ataSlave, "", "file1.txt", part_descriptors);
    while(1) {
        key_packet p = kb_fetch();
        if(p.printable)
            terminal_write("a", 1);
        if(p.value == -2)
            terminal_rem();
    }
    return 0;
}
