#include <drivers/ata.h>
#include <hardwarecomms/portio.h>
#include <io/screen.h>






/// @brief creates the drive struct
/// @param master is the drive a master or a slave
/// @param portBase the portbase of the drive
/// @return the drive struct
ata_drive create_ata(bool master, uint16_t portBase)
{
    ata_drive nDrive;
    
    nDrive.dataPort = portBase;
    nDrive.errorPort = portBase + 0x1;
    nDrive.sectorCountPort = portBase + 0x2;
    nDrive.lbaLowPort = portBase + 0x3;
    nDrive.lbaMidPort = portBase + 0x4;
    nDrive.lbaHiPort = portBase + 0x5;
    nDrive.devicePort = portBase + 0x6;
    nDrive.commandPort = portBase + 0x7;
    nDrive.controlPort = portBase + 0x206;

    nDrive.master = master;

    return nDrive;
}


/// @brief identifies a ata drive
/// @param drive the drive to id
void identify(ata_drive drive)
{
    outb(drive.devicePort, drive.master ? 0xA0 : 0xB0);
    outb(drive.controlPort, 0);

    outb(drive.devicePort, 0xA0);
    uint8_t status = inb(drive.commandPort);
    if (status == 0xFF)
        return;
        
    
    outb(drive.devicePort, drive.master ? 0xA0 : 0xB0);
    outb(drive.sectorCountPort, 0);
    outb(drive.lbaLowPort, 0);
    outb(drive.lbaMidPort, 0);
    outb(drive.lbaHiPort, 0);
    outb(drive.commandPort, 0xEC); // identify command

    status = inb(drive.commandPort);
    if(status == 0x00)
        return;
    
    while(((status & 0x80) == 0x80) && ((status & 0x01) != 0x01))
        status = inb(drive.commandPort);
    
    if(status & 0x01)
    {
        terminal_write_string("ERROR");
        return;
    }

    for(int i = 0; i < 256; i++)
    {
        uint16_t data = inw(drive.dataPort);
        char *text = "  \0";
        text[0] = (data >> 8) & 0xFF;
        text[1] = data & 0xFF;
        terminal_write_string(text);
    }
    terminal_write_string("\n");
}

/// @brief reads from a specified sector in the drive
/// @param drive 
/// @param sectorNum
/// @param buff buffer in which read data will be stored
/// @param count the amount of bytes to read
void read28(ata_drive drive, uint32_t sectorNum, uint8_t* buff, int count) {
    if(sectorNum > 0x0FFFFFFF)
        return;
    
    
    outb(drive.devicePort, (drive.master ? 0xE0 : 0xF0) | ((sectorNum & 0x0F000000) >> 24));
    outb(drive.errorPort, 0);
    outb(drive.sectorCountPort, 1);
    outb(drive.lbaLowPort, sectorNum & 0x000000FF);
    outb(drive.lbaMidPort, (sectorNum & 0x0000FF00) >> 8);
    outb(drive.lbaHiPort, (sectorNum & 0x00FF0000) >> 16);
    outb(drive.commandPort, 0x20); // identify command

    uint8_t status = inb(drive.commandPort);

    while(((status & 0x80) == 0x80) && ((status & 0x01) != 0x01))
        status = inb(drive.commandPort);
    
    if(status & 0x01)
    {
        terminal_write_string("ERROR");
        return;
    }

    for(int i = 0; i < count; i+=2)
    {
        uint16_t wdata = inw(drive.dataPort);
        buff[i] =  wdata & 0xFF;
        if(i+1 < count)
            buff[i + 1] = (wdata >> 8) & 0xFF;
    }

    // reads the rest of the sector
    for(int i = count + (count%2); i < 512; i += 2)
        inw(drive.dataPort);
}

/// @brief writes to a specifed sector in the drive
/// @param drive 
/// @param sectorNum 
/// @param data the data to write
/// @param count amount of bytes to write
void write28(ata_drive drive, uint32_t sectorNum, uint8_t *data, uint32_t count) {
    if(sectorNum > 0x0FFFFFFF)
        return;
    
    if (count > 512)
        return;
    outb(drive.devicePort, (drive.master ? 0xE0 : 0xF0) | ((sectorNum & 0x0F000000) >> 24));
    outb(drive.errorPort, 0);
    outb(drive.sectorCountPort, 1);
    outb(drive.lbaLowPort, sectorNum & 0x000000FF);
    outb(drive.lbaMidPort, (sectorNum & 0x0000FF00) >> 8);
    outb(drive.lbaHiPort, (sectorNum & 0x00FF0000) >> 16);
    outb(drive.commandPort, 0x30); // identify command

    uint8_t status = inb(drive.commandPort);
    
    for(int i = 0; i < ((int)count); i+=2)
    {
        uint16_t wdata = data[i];
        if(i+1 < ((int)count))
            wdata |= ((uint16_t)data[i+1]) << 8;

        outw(drive.dataPort, wdata);
        
        char *text = "  \0";
        text[0] = wdata & 0xFF;
        text[1] = (wdata >> 8) & 0xFF;
        terminal_write_string(text);
    }

    // reads the rest of the sector
    for(int i = count + (count%2); i < 512; i += 2)
        outw(drive.dataPort, 0x0000);
}


/// @brief flashes drive to make updates perm
/// @param drive 
void flush(ata_drive drive) {
    outb(drive.devicePort, drive.master ? 0xE0 : 0xF0);
    outb(drive.commandPort, 0xE7);

    uint8_t status = inb(drive.commandPort);
    if(status == 0x00)
        return;
    
    while(((status & 0x80) == 0x80) && ((status & 0x01) != 0x01))
        status = inb(drive.commandPort);
    
    if(status & 0x01)
    {
        terminal_write_string("ERROR");
        return;
    }

}

