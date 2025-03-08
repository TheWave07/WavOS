#include <filesystem/msdospart.h>
#include <filesystem/fat.h>
#include <io/screen.h>

/// @brief reads the partiton table of the drive and prints basic info
/// @param drive 
void readPartitions(ata_drive drive) {
    master_boot_record mbr;
    read28(drive, 0,(uint8_t*)&mbr, sizeof(master_boot_record));

    if(mbr.magicnum != 0xAA55)
    {
       terminal_write_string("illegal MBR");
       return;
    }
 
    // Pretty print of MBR
    for(int i = 0; i < 4; i++)
    {
        if(mbr.primaryPartition[i].partition_id == 0x00)
            continue;

        terminal_write_string(" Partition ");
        terminal_write_int(i & 0xFF, 16);

        if(mbr.primaryPartition[i].bootable == 0x80)
        {
            terminal_write_string(" bootable. Type ");
        }
        else
        {
            terminal_write_string(" not bootable. Type ");
        }

        terminal_write_int(mbr.primaryPartition[i].partition_id, 16);

        readBPB(drive, mbr.primaryPartition[i].start_lba);
    }
    terminal_write_string("\n");
}