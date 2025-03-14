#ifndef __WAVOS__FILESYSTEM__FAT_H
#define __WAVOS__FILESYSTEM__FAT_H

#include <common/types.h>
#include <drivers/ata.h>
typedef struct {
    // common biosParameter for fat12/16/32
    uint8_t jump[3];
    uint8_t softName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorPerCluster;
    uint16_t reservedSectors;
    uint8_t  fatCopies;
    uint16_t rootDirEntries;
    uint16_t totalSectors;
    uint8_t mediaType;
    uint16_t fatSectorCount;
    uint16_t sectorPerTrack;
    uint16_t headCount;
    uint32_t hiddenSectors;
    uint32_t totalSectorCount;

    // extended biosParameter for fat32 only
    uint32_t tableSize;
    uint16_t extFlags;
    uint16_t fatVersion;
    uint32_t rootCluster;
    uint16_t fatInfo;
    uint16_t backupSector;
    uint8_t reserved0[12];
    uint8_t driveNumber;
    uint8_t reserved;
    uint8_t bootSignature;
    uint32_t volumeId;
    uint8_t volumeLabel[11];
    uint8_t fatTypeLabel[8];
} __attribute__((packed)) bios_parameter_block32;


typedef struct {
    uint32_t leadSig;
    uint8_t reserved0[480];
    uint32_t secSig;
    uint32_t freeClusterCount;
    uint32_t freeClusterHint;
    uint8_t reserved1[12];
    uint32_t trailSig;
} __attribute__((packed)) FSInfo_block;

typedef struct {
    uint8_t name[8];
    uint8_t ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t cTimeTenth;
    uint16_t cTime;
    uint16_t cDate;
    uint16_t aTime;
    uint16_t firstClusterHi;
    uint16_t wTime;
    uint16_t wDate;
    uint16_t firstClusterLo;
    uint32_t size;
} __attribute__((packed)) directory_entry_fat32;

typedef struct {
    uint8_t order; //6 bit indicates last entry
    uint16_t chars1[5];
    uint8_t attributes;
    uint8_t longEntryType;
    uint8_t checksum;
    uint16_t chars2[6];
    uint16_t zero;
    uint16_t chars3[2];
} __attribute__((packed)) LFN_entry_fat32;

typedef struct {
    uint32_t fat_start;
    uint32_t fat_size;

    uint32_t data_start;
} fat_descriptor;

typedef struct {
    bios_parameter_block32 bpb;
    
    FSInfo_block FSInfo;

    fat_descriptor fatDesc;
} partition_descr;

partition_descr read_BPB(ata_drive hd, uint32_t partitionOffset);
void read_dir(ata_drive hd, const char* path, partition_descr *partDesc);
void tree(ata_drive hd, partition_descr *partDesc);
void create_file(ata_drive hd, char* path, char* fileName, partition_descr *partDesc);
void delete_file(ata_drive hd, char* dirPath, char* fileName, partition_descr *partDesc);
void create_dir(ata_drive hd, char* path, char* dirName, partition_descr *partDesc);
void delete_dir(ata_drive hd, char* dirPath, char* dirName, partition_descr *partDesc);
void read_file(ata_drive hd, const char* dirPath, const char* fileName, partition_descr *partDesc);
void write_to_file(ata_drive hd, const char* dirPath,const char *fileName, const char *data, uint32_t size, partition_descr *partDesc);
#endif