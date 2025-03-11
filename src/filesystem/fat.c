#include <filesystem/fat.h>
#include <memorymanagement.h>
#include <common/str.h>
#include <io/screen.h>
#define SECTOR_SIZE 512
#define SECTOR_TO_BYTE SECTOR_SIZE / 4




/// @brief returns the val of the specified entry in FAT
/// @param hd
/// @param ent_idx 
/// @return the value
uint32_t read_fat_entry(ata_drive hd, uint32_t ent_idx, partition_descr partDesc) {
    uint8_t fatBuffer[512];

    // gets the next cluster belonging to the file
    uint32_t entrySector = ent_idx / SECTOR_TO_BYTE;
    read28(hd, partDesc.fatDesc.fat_start + entrySector, fatBuffer, 512);

    uint32_t entryOffInSect = ent_idx % (SECTOR_TO_BYTE);
    return ((uint32_t*)fatBuffer)[entryOffInSect] & 0x0FFFFFFF;
}

/// @brief writes a new value to the sepcified fat entry
/// @param hd 
/// @param ent_idx the index of the entry
/// @param partDesc 
/// @param val the new value
void write_fat_entry(ata_drive hd, uint32_t ent_idx, partition_descr partDesc, uint32_t val) {
    uint8_t fatBuffer[512];

    // gets the next cluster belonging to the file
    uint32_t entrySector = ent_idx / SECTOR_TO_BYTE;
    read28(hd, partDesc.fatDesc.fat_start + entrySector, fatBuffer, 512);

    uint32_t entryOffInSect = ent_idx % (SECTOR_TO_BYTE);
    ((uint32_t*)fatBuffer)[entryOffInSect] = val;
    write28(hd, partDesc.fatDesc.fat_start + entrySector, fatBuffer, 512);
    flush(hd);

    read28(hd, partDesc.fatDesc.fat_start + partDesc.fatDesc.fat_size + entrySector, fatBuffer, 512);
    ((uint32_t*)fatBuffer)[entryOffInSect] = val;
    write28(hd, partDesc.fatDesc.fat_start + partDesc.fatDesc.fat_size + entrySector, fatBuffer, 512);
    flush(hd);
}

/// @brief parses partition bpb
/// @param hd 
/// @param partitionOffset 
/// @return a partiton descriptor
partition_descr read_BPB(ata_drive hd, uint32_t partitionOffset) {
    partition_descr partDesc;
    read28(hd, partitionOffset, (uint8_t*)&partDesc.bpb, sizeof(bios_parameter_block32));

    terminal_write_string("sectors per cluster: ");
    terminal_write_int(partDesc.bpb.sectorPerCluster, 16);
    terminal_write_string("\n");

    partDesc.fatDesc.fat_start = partitionOffset + partDesc.bpb.reservedSectors;
    partDesc.fatDesc.fat_size = partDesc.bpb.tableSize;

    partDesc.fatDesc.data_start = partDesc.fatDesc.fat_start + partDesc.fatDesc.fat_size * partDesc.bpb.fatCopies;

    return partDesc;
}

void tree(ata_drive hd, partition_descr partDesc) {
    read_dir(hd, partDesc.bpb.rootCluster, partDesc); 
}

/// @brief reads a dir and prints its contents
/// @param hd
/// @param firstCluster first cluster of the dir
void read_dir(ata_drive hd, uint32_t firstCluster, partition_descr partDesc) {

    directory_entry_fat32 dirent[16];

    uint32_t nextDirCluster = firstCluster;
    bool moreEnt = 1;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            for (int i = 0; i < 16; i++) {
                if(dirent[i].name[0] == 0x00) { // end of dir entries
                    moreEnt = 0;
                    break;
                }
                if(dirent[i].name[0]  == 0xE5) // skip, unused entry
                    continue;

                if((dirent[i].attributes & 0x0F) == 0x0F) // long name dir entry
                    continue;
                
                char *foo = "        \n";
                for (int j = 0; j < 8; j++)
                {
                    foo[j] = dirent[i].name[j];
                }
                if(strlen(foo) != 9) {
                    foo[strlen(foo) + 1] = '\0';
                    foo[strlen(foo)] = '\n';
                }
                terminal_write_string(foo);

                uint32_t firstEntCluster = ((uint32_t)dirent[i].firstClusterHi) << 16
                                          | ((uint32_t)dirent[i].firstClusterLo);
                if((dirent[i].attributes & 0x10) == 0x10) { // dir
                    if (dirent[i].name[0] == '.')
                        continue;
                    read_dir(hd, firstEntCluster, partDesc);
                    continue;
                }

                int32_t SIZE = dirent[i].size;
                uint32_t nextFileCluster = firstEntCluster;
                uint8_t buffer[513];
                //follows cluster chain
                while (nextFileCluster < 0x0FFFFFF8)
                {
                    uint32_t fileSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextFileCluster - 2);
                    int sectorOffset = 0;
                    
                    // reads all sectors belonging to file from curr file cluster
                    for (; SIZE > 0; SIZE -= 512)
                    {
                        read28(hd, fileSector + sectorOffset, buffer, 512);
                        buffer[SIZE > 512 ? 512 : SIZE] = '\0';

                        if(++sectorOffset > partDesc.bpb.sectorPerCluster)
                            break;

                        terminal_write_string((char*)buffer);
                    }

                    nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
                    
                }
            }
        } while ((++dirSectorOffset <= partDesc.bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
}

// returns the first empty entry in the FAT
uint32_t find_empty_Cluster(ata_drive hd, partition_descr partDesc) {
    for (uint32_t i = 2; i < partDesc.fatDesc.fat_size / 4; i++)
    {
        if(read_fat_entry(hd, i, partDesc) == 0)
            return i;
    }
    return -1;
}


uint32_t* find_cluster_chain(ata_drive hd, partition_descr partDesc, int clusterAmount) {
    if(clusterAmount > partDesc.freeClusters)
        return NULL;
    uint32_t* clustersIdxs = (uint32_t*) malloc(clusterAmount * sizeof(uint32_t));
    if(!clustersIdxs)
        return NULL;
    int currClusterIdx = 0;
    for (uint32_t i = 2; (i < partDesc.fatDesc.fat_size / 4) && (currClusterIdx < clusterAmount); i++)
    {
        if(read_fat_entry(hd, i, partDesc) == 0)
            clustersIdxs[currClusterIdx++]  = i;
    }
    return clustersIdxs;
}

/// @brief allocates space for data in the FAT
/// @param hd 
/// @param partDesc 
/// @param start_cluster the starting cluster of the chain
/// @param size the size needing allocation
/// @return the amount of clusters allocated, -1 if there is not enough space
int allocate_space(ata_drive hd, partition_descr partDesc, uint32_t start_cluster, uint32_t size) {
    uint32_t clustersNeeded = (size / (SECTOR_SIZE * partDesc.bpb.sectorPerCluster));

    if(clustersNeeded == 0)
        return 1;

    //checks if there are enough free clusters 
    if (partDesc.freeClusters < clustersNeeded)
        return -1;
    
    uint32_t* clustersIdx = find_cluster_chain(hd, partDesc, clustersNeeded);
    
    if(!clustersIdx)
        return -1;
    

    write_fat_entry(hd, start_cluster, partDesc, *clustersIdx);

    
    // creates cluster chain in fat
    for (int i = 0; i < (clustersNeeded - 1); i++)
    {
        write_fat_entry(hd, clustersIdx[i], partDesc, clustersIdx[i+1]);
    }

    write_fat_entry(hd, clustersIdx[clustersNeeded - 1], partDesc, 0x0FFFFFF8);

    return clustersNeeded  + 1;
}

/// @brief creates file in root directory
/// @param hd 
/// @param name 
/// @param ext 
void create_file(ata_drive hd, char* name, char* ext, partition_descr partDesc) {
    
    
    directory_entry_fat32 dirent[16];

    uint32_t nextDirCluster = partDesc.bpb.rootCluster;
    bool moreEnt = 1;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            int emptyEntIdx = -1;
            for (int i = 0; i < 16; i++) {
                if((dirent[i].name[0] == 0x00) || (dirent[i].name[0]  == 0xE5)) { // end of dir entries
                    emptyEntIdx = i;
                    break;
                }
            }
            if(emptyEntIdx == -1)
                continue;
            // creates dir entry for the file
            for (int j = 0; (j < 8) && (name[j] != '\0'); j++)
                dirent[emptyEntIdx].name[j] = name[j];
            
            for (int j = 0; j < 3; j++) {
                dirent[emptyEntIdx].ext[j] = ext[j];
            }
            dirent[emptyEntIdx].size = 0;
            dirent[emptyEntIdx].attributes = 0x0;
            uint32_t firstCluster = find_empty_Cluster(hd, partDesc);
            dirent[emptyEntIdx].firstClusterHi = (uint16_t )(firstCluster >> 16);
            dirent[emptyEntIdx].firstClusterLo = (uint16_t)(firstCluster & 0xffff);

            // updates dir and fat
            write_fat_entry(hd, firstCluster, partDesc, 0x0FFFFFF8);
            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);
            break;
        } while ((++dirSectorOffset <= partDesc.bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
}




/// @brief writes to file in root dir
/// @param hd 
/// @param name name of file
/// @param ext extension of file
/// @param partDesc partition in which the file is
/// @param data 
/// @param size 
void write_to_file(ata_drive hd, const char *name, const char *ext, partition_descr partDesc, const char *data, uint32_t size) {
    directory_entry_fat32 dirent[16];

    uint32_t nextDirCluster = partDesc.bpb.rootCluster;
    bool moreEnt = 1;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            int fileEntIdx = -1;
            for (int i = 0; i < 16; i++) {
                if(dirent[i].name[0] == 0x00) { // end of dir entries
                    moreEnt = 0;
                    break;
                }
                if(dirent[i].name[0]  == 0xE5) // skip, unused entry
                    continue;

                if((dirent[i].attributes & 0x0F) == 0x0F) // long name dir entry
                    continue;

                char *fileName = "        ";
                for (int j = 0; j < 8; j++)
                {
                    fileName[j] = dirent[i].name[j];
                }
                if(strcmp(fileName, name))
                    continue;
                char *fileExt = "   ";
                for (int j = 0; j < 3; j++)
                {
                    fileExt[j] = dirent[i].ext[j];
                }
                if(strcmp(fileExt, ext))
                    continue;
                fileEntIdx = i;
                break;
            }
            if(fileEntIdx == -1)
                continue;
            // gets first cluster of file
            uint32_t firstFileCluster = ((uint32_t)dirent[fileEntIdx].firstClusterHi) >> 16
                                     | ((uint32_t)dirent[fileEntIdx].firstClusterLo);
            // updates the entry size
            dirent[fileEntIdx].size = size;

            if (allocate_space(hd, partDesc, firstFileCluster, size) == -1) {
                terminal_write_string("Not enough Space");
                break;
            }

            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);
            // writes the data
            uint32_t fileSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (firstFileCluster - 2);
            
            // writes data to cluster chain
            int32_t SIZE = size;
            uint32_t nextFileCluster = firstFileCluster;
            while (nextFileCluster < 0x0FFFFFF8 )
            {
                
                uint32_t fileSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextFileCluster - 2);
                int sectorOffset = 0;
                

                // reads all sectors belonging to file from curr file cluster
                for (; SIZE > 0; SIZE -= 512)
                {
                    int amountToWrite =  (SIZE > 512 ? 512 : SIZE);
                    write28(hd, fileSector + sectorOffset, (uint8_t*)data, amountToWrite);
                    flush(hd);
                    data += amountToWrite;

                    if(++sectorOffset > partDesc.bpb.sectorPerCluster) {
                        SIZE -= 512;
                        break;
                    }
                }
                nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
                
            }



            break;
        } while ((++dirSectorOffset <= partDesc.bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
}