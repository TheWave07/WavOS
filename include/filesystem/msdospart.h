#ifndef __WAVOS__FILESYSTEM__MSDOSPART_H
#define __WAVOS__FILESYSTEM__MSDOSPART_H
#include <common/types.h>
#include <drivers/ata.h>
typedef struct {
    uint8_t bootable;

    uint8_t start_head;
    uint8_t start_sector : 6;
    uint16_t start_cylinder : 10;

    uint8_t partition_id;

    uint8_t end_head;
    uint8_t end_sector : 6;
    uint16_t end_cylinder : 10;

    uint32_t start_lba;
    uint32_t length;
} __attribute__((packed)) partition_table_entry;

typedef struct {
    uint8_t bootloader[440];
    uint32_t signature;
    uint16_t unused;

    partition_table_entry primaryPartition[4];

    uint16_t magicnum;

} __attribute__((packed)) master_boot_record;

void readPartitions(ata_drive drive);
#endif