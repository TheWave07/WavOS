#ifndef __WAVOS__DRIVERS__ATA_H
#define __WAVOS__DRIVERS__ATA_H
#include <common/types.h>

typedef struct ata_drive {
    // 16 bit port
    uint16_t dataPort;
    // 8 bit ports
    uint16_t errorPort;
    uint16_t sectorCountPort;
    uint16_t lbaLowPort;
    uint16_t lbaMidPort;
    uint16_t lbaHiPort;
    uint16_t devicePort;
    uint16_t commandPort;
    uint16_t controlPort;
    
    bool master;
} ata_drive;


ata_drive create_ata(bool master, uint16_t portBase);

void identify(ata_drive drive);
void read28(ata_drive drive, uint32_t sectorNum, uint8_t* buff, int count);
void write28(ata_drive drive, uint32_t sectorNum, uint8_t *data, uint32_t count);
void flush(ata_drive drive);

#endif