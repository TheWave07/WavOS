#include <filesystem/fat.h>
#include <memorymanagement.h>
#include <common/str.h>
#include <io/screen.h>
#define SECTOR_SIZE 512
#define SECTOR_TO_BYTE SECTOR_SIZE / 4


uint32_t find_dir_first_cluster(ata_drive hd, const char* path, partition_descr partDesc);

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

// returns the first empty entry in the FAT
uint32_t find_empty_Cluster(ata_drive hd, partition_descr partDesc) {
    for (uint32_t i = 2; i < partDesc.fatDesc.fat_size / 4; i++)
    {
        if(read_fat_entry(hd, i, partDesc) == 0)
            return i;
    }
    return -1;
}


uint32_t* find_cluster_chain(ata_drive hd, partition_descr partDesc, uint32_t clusterAmount) {
    if(clusterAmount > partDesc.freeClusters)
        return NULL;
    uint32_t* clustersIdxs = (uint32_t*) malloc(clusterAmount * sizeof(uint32_t));
    if(!clustersIdxs)
        return NULL;
    uint32_t currClusterIdx = 0;
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
    int32_t clustersNeeded = (size / (SECTOR_SIZE * partDesc.bpb.sectorPerCluster));

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
    for (int32_t i = 0; i < (clustersNeeded - 1); i++)
    {
        write_fat_entry(hd, clustersIdx[i], partDesc, clustersIdx[i+1]);
    }

    write_fat_entry(hd, clustersIdx[clustersNeeded - 1], partDesc, 0x0FFFFFF8);

    return clustersNeeded  + 1;
}

/// @brief creates file in root directory
/// @param hd 
/// @param name name string
/// @param ext ext string
void create_file(ata_drive hd, char* path, char* name, char* ext, partition_descr partDesc) {
    directory_entry_fat32 dirent[16];

    uint32_t resDirCluster = find_dir_first_cluster(hd, path, partDesc);

    //file already exists
    if(resDirCluster == 0xFFFFFFFF)
        return;
    
    uint32_t nextDirCluster = find_dir_first_cluster(hd, path, partDesc);
    //if path not found
    if(nextDirCluster == -1)
        return;
    
    bool moreEnt = 1;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            //findes empty entry
            read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            int emptyEntIdx = -1;
            for (int i = 0; i < 16; i++) {
                if((dirent[i].name[0] == 0x00) || (dirent[i].name[0]  == 0xE5)) { // end of dir entries
                    emptyEntIdx = i;
                    break;
                }
            }
            terminal_write_int(emptyEntIdx, 10);
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

/*void create_dir(ata_drive hd, char* name, partition_descr partDesc) {
    directory_entry_fat32 dirent[16];

    uint32_t nextDirCluster = partDesc.bpb.rootCluster;
    bool moreEnt = 1;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            //findes empty entry
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
}*/


void write_to_file_by_dir_cluster(ata_drive hd, uint32_t dirFirstClust, const char *fileName, partition_descr partDesc, const char *data, uint32_t size) {
    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;

    uint32_t nextDirCluster = dirFirstClust;
    bool moreEnt = true;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            int fileEntIdx = -1;
            for (int i = 0; i < 16; i++) {
                if(dirent[i].name[0] == 0x00) { // end of dir entries
                    moreEnt = false;
                    break;
                }
                if(dirent[i].name[0]  == 0xE5) // skip, unused entry
                    continue;

                if((dirent[i].attributes & 0x0F) == 0x0F) { // long name dir entry
                    lfnEnt[lfnIdx++] = *((LFN_entry_fat32 *) &dirent[i]);
                    continue;
                }

                if (lfnIdx > 0) {
                    char nameBuff[256] = {0};
                    read_long_filename(lfnEnt, lfnIdx, nameBuff);
                    lfnIdx = 0;

                    if(strcmp(fileName, nameBuff))
                        continue;
                } else {
                    continue;
                }

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

/// @brief convets utf16 to utf8 ascii (assuming only utf8 compatible chars are used)
/// @param utf16 the utf16 string
/// @param ascii the buffer in which the utf8 string will be put
/// @param length length of utf16 string
void utf16_to_ascii(uint16_t *utf16, char *ascii, int length) {
    for (int i = 0; i < length; i++) {
        if (utf16[i] == 0xFFFF || utf16[i] == 0) break;
        ascii[i] = (char)utf16[i];
    }
}

/// @brief reads long filenam 
/// @param lfn_entries lfn entries array
/// @param count count of entries
/// @param output where to put the lfn
void read_long_filename(LFN_entry_fat32 *lfn_entries, int count, char *output) {
    int pos = 0;

    // Process LFN entries in reverse order
    for (int i = count - 1; i >= 0; i--) {
        utf16_to_ascii((uint16_t*)lfn_entries[i].chars1, &output[pos], 5);
        pos += 5;
        utf16_to_ascii((uint16_t*)lfn_entries[i].chars2, &output[pos], 6);
        pos += 6;
        utf16_to_ascii((uint16_t*)lfn_entries[i].chars3, &output[pos], 2);
        pos += 2;
    }

    output[pos] = '\0'; 
}

/// @brief reads a dir and prints its contents
/// @param hd
/// @param firstCluster first cluster of the dir
void read_dir_by_cluster(ata_drive hd, uint32_t firstCluster, partition_descr partDesc) {

    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;
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

                if((dirent[i].attributes & 0x0F) == 0x0F) { // long name dir entry
                    lfnEnt[lfnIdx++] = *((LFN_entry_fat32 *) &dirent[i]);
                    continue;
                }

                if (lfnIdx > 0) {
                    char nameBuff[256] = {0};
                    read_long_filename(lfnEnt, lfnIdx, nameBuff);
                    terminal_write_string(nameBuff);
                    lfnIdx = 0;
                } else {
                    terminal_write(dirent[i].name, 8);
                    if((dirent[i].attributes & 0x10) != 0x10) {
                        terminal_write_string(".");
                        terminal_write(dirent[i].ext, 3);
                    }
                }
                terminal_write_string("\n");

                uint32_t firstEntCluster = ((uint32_t)dirent[i].firstClusterHi) << 16
                                          | ((uint32_t)dirent[i].firstClusterLo);
                // if dir, recursivly read its contents
                if((dirent[i].attributes & 0x10) == 0x10) {
                    if (dirent[i].name[0] == '.')
                        continue;
                    read_dir_by_cluster(hd, firstEntCluster, partDesc);
                    continue;
                }

                //read file
                //read_file_by_cluster(hd, firstEntCluster, dirent[i].size, partDesc);
            }
        } while ((++dirSectorOffset <= partDesc.bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
    terminal_write_string("\n");
}


/// @brief checks whether or not a certain file is in a dir
/// @param hd 
/// @param dirFirstCluster 
/// @param fileName
/// @param partDesc 
/// @return true if found, false otherwise
bool is_file_in_dir(ata_drive hd, uint32_t dirFirstCluster, const char* fileName, partition_descr partDesc) {
    uint32_t nextDirCluster = dirFirstCluster;

    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;

    bool moreEnt = true;
    while ((nextDirCluster < 0x0FFFFFF8) && moreEnt) {
            int dirSectorOffset = 0;
            uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
            do {
                read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
                for (int i = 0; i < 16; i++) {
                    if(dirent[i].name[0] == 0x00) { // end of dir entries
                        moreEnt = false;
                        break;
                    }
                    if(dirent[i].name[0]  == 0xE5) // skip, unused entry
                        continue;

                    if((dirent[i].attributes & 0x0F) == 0x0F) { // long name dir entry
                        lfnEnt[lfnIdx++] = *((LFN_entry_fat32 *) &dirent[i]);
                        continue;
                    }
    
                    if (lfnIdx > 0) {
                        char nameBuff[256] = {0};
                        read_long_filename(lfnEnt, lfnIdx, nameBuff);
                        lfnIdx = 0;

                        if(strcmp(fileName, nameBuff))
                            continue;
                    }

                    return true;
                }
            } while ((++dirSectorOffset <= partDesc.bpb.sectorPerCluster) && moreEnt);
            // gets next cluster belonging to the dir
            nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
    return false;
}


/// @brief returns the dir ent of a specified file
/// @param hd 
/// @param dirFirstCluster
/// @param fileName 
/// @param partDesc 
/// @return the entry if found, garbage otherwise
directory_entry_fat32 find_file_dir_entry(ata_drive hd, uint32_t dirFirstCluster, const char* fileName, partition_descr partDesc) {
    uint32_t nextDirCluster = dirFirstCluster;

    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;

    bool moreEnt = true;
    while ((nextDirCluster < 0x0FFFFFF8) && moreEnt) {
            int dirSectorOffset = 0;
            uint32_t dirSector = partDesc.fatDesc.data_start + partDesc.bpb.sectorPerCluster * (nextDirCluster - 2);
            do {
                read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
                for (int i = 0; i < 16; i++) {
                    if(dirent[i].name[0] == 0x00) { // end of dir entries
                        moreEnt = false;
                        break;
                    }
                    if(dirent[i].name[0]  == 0xE5) // skip, unused entry
                        continue;

                    if((dirent[i].attributes & 0x0F) == 0x0F) { // long name dir entry
                        lfnEnt[lfnIdx++] = *((LFN_entry_fat32 *) &dirent[i]);
                        continue;
                    }
    
                    if (lfnIdx > 0) {
                        char nameBuff[256] = {0};
                        read_long_filename(lfnEnt, lfnIdx, nameBuff);
                        lfnIdx = 0;

                        if(strcmp(fileName, nameBuff))
                            continue;
                    } else {
                        continue;
                    }

                    return dirent[i];
                }
            } while ((++dirSectorOffset <= partDesc.bpb.sectorPerCluster) && moreEnt);
            // gets next cluster belonging to the dir
            nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
    return dirent[0];
}

/// @brief reads the file thats starts in a certian cluster
/// @param hd 
/// @param firstCluster the cluster the file starts in
/// @param size the size of the file
/// @param partDesc 
void read_file_by_cluster(ata_drive hd, uint32_t firstCluster, uint32_t size, partition_descr partDesc) {
    int32_t SIZE = (int32_t)size;
    uint32_t nextFileCluster = firstCluster;
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

/// @brief finds the first cluster of the entry in a specifed path
/// @param hd 
/// @param path the specified path
/// @param partDesc 
/// @return the first cluster index of the entry
uint32_t find_dir_first_cluster(ata_drive hd, const char* path, partition_descr partDesc) {
    uint32_t currentDirCluster = partDesc.bpb.rootCluster;

    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;
    char* nextDirName = strtok((char*) path, "/");
    while(nextDirName) {
        uint32_t nextDirCluster = currentDirCluster;
        bool moreEnt = 1;
        bool found = 0;
        int nextDirNameLen = strlen(nextDirName);
        while (moreEnt && (nextDirCluster < 0x0FFFFFF8) && !found) {
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

                    if((dirent[i].attributes & 0x0F) == 0x0F) { // long name dir entry
                        lfnEnt[lfnIdx++] = *((LFN_entry_fat32 *) &dirent[i]);
                        continue;
                    }
    
                    if (lfnIdx > 0) {
                        char nameBuff[256] = {0};
                        read_long_filename(lfnEnt, lfnIdx, nameBuff);
                        lfnIdx = 0;

                        if(strcmp(nextDirName, nameBuff))
                            continue;
                    } else {
                        char dir[9] = "        ";
                        for(int j = 0; j < nextDirNameLen; j++)
                            dir[j] = nextDirName[j];
                        dir[9] = '\0';
    
                        char *dirName = "        ";
                        for (int j = 0; j < 8; j++)
                        {
                            dirName[j] = dirent[i].name[j];
                        }
                        if(strcmp(dir, dirName))
                            continue;
                    }
                    
                    uint32_t EntCluster = ((uint32_t)dirent[i].firstClusterHi) << 16
                                            | ((uint32_t)dirent[i].firstClusterLo);


                    currentDirCluster = EntCluster;
                    found = 1;
                }
            } while ((++dirSectorOffset <= partDesc.bpb.sectorPerCluster) && moreEnt && !found);
            // gets next cluster belonging to the dir
            nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
        }
        if (!found)
            return -1;
        
        nextDirName = strtok(0, "/");
    }

    return currentDirCluster;
}

/// @brief reads the dir specified in path
/// @param hd 
/// @param path 
/// @param partDesc 
void read_dir(ata_drive hd, const char* path, partition_descr partDesc) {
    read_dir_by_cluster(hd, find_dir_first_cluster(hd, path, partDesc), partDesc);
}

void read_file(ata_drive hd, const char* dirPath, const char* fileName, partition_descr partDesc) {
    uint32_t dirsCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(dirsCluster == 0xFFFFFFFF) { //invalid path
        terminal_write_string("inalid path");
        return;
    }
    if(!is_file_in_dir(hd, dirsCluster, fileName, partDesc)) { // file not found
        terminal_write_string("file not found");
        return;
    } 

    directory_entry_fat32 fileEnt = find_file_dir_entry(hd, dirsCluster, fileName, partDesc);
    uint32_t firstFileClust = ((uint32_t)fileEnt.firstClusterHi) << 16
    | ((uint32_t)fileEnt.firstClusterLo);
    read_file_by_cluster(hd, firstFileClust, fileEnt.size, partDesc);
}


void write_to_file(ata_drive hd, const char* dirPath,const char *fileName, const char *data, uint32_t size, partition_descr partDesc) {
    uint32_t dirsCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(dirsCluster == 0xFFFFFFFF) { //invalid path
        terminal_write_string("inalid path");
        return;
    }
    if(!is_file_in_dir(hd, dirsCluster, fileName, partDesc)) { // file not found
        terminal_write_string("file not found");
        return;
    } 

    write_to_file_by_dir_cluster(hd, dirsCluster, fileName, partDesc, data, size);
}
void tree(ata_drive hd, partition_descr partDesc) {
    read_dir_by_cluster(hd, partDesc.bpb.rootCluster, partDesc); 
}