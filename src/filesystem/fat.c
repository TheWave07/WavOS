#include <filesystem/fat.h>
#include <io/screen.h>



void readBPB(ata_drive hd, uint32_t partitionOffset) {

    bios_parameter_block32 bpb;

    read28(hd, partitionOffset, (uint8_t*)&bpb, sizeof(bios_parameter_block32));

    terminal_write_string("sectors per cluster: ");
    terminal_write_int(bpb.sectorPerCluster, 16);
    terminal_write_string("\n");

    uint32_t fatStart = partitionOffset + bpb.reservedSectors;
    uint32_t fatSize = bpb.tableSize;

    uint32_t dataStart = fatStart + fatSize * bpb.fatCopies;
    uint32_t rootStart = dataStart + bpb.sectorPerCluster * (bpb.rootCluster - 2);

    directory_entry_fat32 dirent[16];
    read28(hd, rootStart, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));

    for (int i = 0; i < 16; i++)
    {
        if(dirent[i].name[0] == 0x00) // unused dir ent
            break;
        
        if((dirent[i].attributes & 0x0F) == 0x0F) // long name dir entry
            continue;
        
        char *foo = "        \n";
        for (int j = 0; j < 8; j++)
        {
            foo[j] = dirent[i].name[j];
        }
        terminal_write_string(foo);

        if((dirent[i].attributes & 0x10) == 0x10) // dir
            continue;
        
        uint32_t firstfileCluster = ((uint32_t)dirent[i].firstClusterHi) << 16
                             | ((uint32_t)dirent[i].firstClusterLo);

        int32_t SIZE = dirent[i].size;
        int32_t nextFileCluster = firstfileCluster;
        uint8_t buffer[513];
        uint8_t fatBuffer[512];
        while (SIZE > 0)
        {
            uint32_t fileSector = dataStart + bpb.sectorPerCluster * (nextFileCluster - 2);
            int sectorOffset = 0;
            
            // reads all sectors belonging to file from curr file cluster
            for (; SIZE > 0; SIZE -= 512)
            {
                read28(hd, fileSector + sectorOffset, buffer, 512);
                buffer[SIZE > 512 ? 512 : SIZE] = '\0';

                if(++sectorOffset > bpb.sectorPerCluster)
                    break;

                terminal_write_string((char*)buffer);
            }

            // gets next cluster belonging to the file
            uint32_t fatSectorCurrCluster = nextFileCluster / (512/sizeof(uint32_t));
            read28(hd, fatStart + fatSectorCurrCluster, fatBuffer, 512);

            uint32_t currClusterOffInFATSector = nextFileCluster % (512/sizeof(uint32_t));
            nextFileCluster = ((uint32_t*)fatBuffer)[currClusterOffInFATSector] & 0x0FFFFFFF;
            
        }
    }
    
}