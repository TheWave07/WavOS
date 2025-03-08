#include <filesystem/msdospart.h>
#include <filesystem/fat.h>
#include <io/screen.h>
#include <memorymanagement.h>
/// @brief reads the partiton table of the drive and prints basic info
/// @param drive
/// @return an array of partition descriptors
partition_descr* read_partitions(ata_drive drive) {
    master_boot_record mbr;
    read28(drive, 0,(uint8_t*)&mbr, sizeof(master_boot_record));

    if(mbr.magicnum != 0xAA55)
    {
       terminal_write_string("illegal MBR");
       return NULL;
    }
    partition_descr nullDesc = {0};
    partition_descr* descriptors = (partition_descr*) malloc(4 * sizeof(partition_descr));
    // Pretty print of MBR
    for(int i = 0; i < 4; i++)
    {
        if(mbr.primaryPartition[i].partition_id == 0x00) {
            descriptors[i] = nullDesc;
            continue;
        }
        descriptors[i] = read_BPB(drive, mbr.primaryPartition[i].start_lba);
        
    }
    terminal_write_string("\n");

    return descriptors;
}