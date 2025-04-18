#include <filesystem/fat.h>
#include <memorymanagement.h>
#include <common/str.h>
#include <common/tools.h>
#include <userinter/output.h>
#define SECTOR_SIZE 512
#define SECTOR_TO_BYTE SECTOR_SIZE / 4
#define SFN_ALLOWED "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'()-@^_`{}~"

uint32_t find_dir_first_cluster(ata_drive hd, const char* path, partition_descr *partDesc);
bool is_file_in_dir(ata_drive hd, uint32_t dirFirstCluster, const char* fileName, partition_descr *partDesc);


/// @brief convets utf16 to utf8 ascii (assuming only utf8 compatible chars are used)
/// @param utf16 the utf16 string
/// @param ascii the buffer in which the utf8 string will be put
/// @param length length of utf16 string
void utf16_to_utf8(uint16_t *utf16, char *ascii, int length) {
    for (int i = 0; i < length; i++) {
        if (utf16[i] == 0xFFFF || utf16[i] == 0) break;
        ascii[i] = (char)utf16[i];
    }
}

/// @brief converts utf8 back into utf16
/// @param utf8 the utf8 string
/// @param utf16 output buffer for utf16 string
void utf8_to_utf16(const char *utf8, uint16_t *utf16) {
    while (*utf8) {
        *utf16++ = (uint16_t)(*utf8++);
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
        utf16_to_utf8((uint16_t*)lfn_entries[i].chars1, &output[pos], 5);
        pos += 5;
        utf16_to_utf8((uint16_t*)lfn_entries[i].chars2, &output[pos], 6);
        pos += 6;
        utf16_to_utf8((uint16_t*)lfn_entries[i].chars3, &output[pos], 2);
        pos += 2;
    }

    output[pos] = '\0'; 
}

/// @brief returns the val of the specified entry in FAT
/// @param hd
/// @param ent_idx 
/// @return the value
uint32_t read_fat_entry(ata_drive hd, uint32_t ent_idx, partition_descr *partDesc) {
    uint8_t fatBuffer[512];

    // gets the next cluster belonging to the file
    uint32_t entrySector = ent_idx / SECTOR_TO_BYTE;
    read28(hd, partDesc->fatDesc.fat_start + entrySector, fatBuffer, 512);

    uint32_t entryOffInSect = ent_idx % (SECTOR_TO_BYTE);
    return ((uint32_t*)fatBuffer)[entryOffInSect] & 0x0FFFFFFF;
}

/// @brief writes a new value to the sepcified fat entry
/// @param hd 
/// @param ent_idx the index of the entry
/// @param partDesc 
/// @param val the new value
void write_fat_entry(ata_drive hd, uint32_t ent_idx, partition_descr *partDesc, uint32_t val) {
    uint8_t fatBuffer[512];

    // gets the next cluster belonging to the file
    uint32_t entrySector = ent_idx / SECTOR_TO_BYTE;
    read28(hd, partDesc->fatDesc.fat_start + entrySector, fatBuffer, 512);

    uint32_t entryOffInSect = ent_idx % (SECTOR_TO_BYTE);
    ((uint32_t*)fatBuffer)[entryOffInSect] = val;
    write28(hd, partDesc->fatDesc.fat_start + entrySector, fatBuffer, 512);
    flush(hd);

    read28(hd, partDesc->fatDesc.fat_start + partDesc->fatDesc.fat_size + entrySector, fatBuffer, 512);
    ((uint32_t*)fatBuffer)[entryOffInSect] = val;
    write28(hd, partDesc->fatDesc.fat_start + partDesc->fatDesc.fat_size + entrySector, fatBuffer, 512);
    flush(hd);
}

/// @brief parses partition bpb
/// @param hd 
/// @param partitionOffset 
/// @return a partiton descriptor
partition_descr read_BPB(ata_drive hd, uint32_t partitionOffset) {
    partition_descr partDesc;
    read28(hd, partitionOffset, (uint8_t*)&partDesc.bpb, sizeof(bios_parameter_block32));
    read28(hd, partitionOffset + partDesc.bpb.fatInfo, (uint8_t*)&partDesc.FSInfo, sizeof(FSInfo_block));

    partDesc.fatDesc.fat_start = partitionOffset + partDesc.bpb.reservedSectors;
    partDesc.fatDesc.fat_size = partDesc.bpb.tableSize;

    partDesc.fatDesc.data_start = partDesc.fatDesc.fat_start + partDesc.fatDesc.fat_size * partDesc.bpb.fatCopies;

    partDesc.currentWorkingDir = partDesc.bpb.rootCluster;
    partDesc.CWDString[0] = '/';

    return partDesc;
}

/// @brief updates the FSInfo section
/// @param hd 
/// @param partDesc 
void update_FSInfo(ata_drive hd, partition_descr *partDesc) {
    write28(hd, partDesc->fatDesc.fat_start - partDesc->bpb.reservedSectors + partDesc->bpb.fatInfo, (uint8_t*)&partDesc->FSInfo, sizeof(FSInfo_block));
    flush(hd);
}

/// @brief zeros the cluser's data
/// @param hd 
/// @param cluster the cluster index
/// @param partDesc 
void empty_out_cluster(ata_drive hd, uint32_t cluster, partition_descr *partDesc) {
    uint32_t clusterFirstSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (cluster - 2);
    // zeros all sectors belonging to the cluster
    for (int sectorOffset = 0; sectorOffset < partDesc->bpb.sectorPerCluster; sectorOffset++)
    {
        uint8_t zero[512] = {0};
        write28(hd,clusterFirstSector + sectorOffset, zero, 512);
        flush(hd);
    }
}

/// @brief findes an empty cluster in fat table
/// @param hd 
/// @param partDesc 
/// @return if found returns its index, otherwise returns 0xFFFFFFFF
uint32_t find_empty_Cluster(ata_drive hd, partition_descr *partDesc) {
    for (uint32_t i = 2; i < partDesc->fatDesc.fat_size / 4; i++)
    {
        if(read_fat_entry(hd, i, partDesc) == 0)
            return i;
    }
    return 0xFFFFFFFF;
}

/// @brief finds the cluster chain length
/// @param hd 
/// @param firstCluster the first cluster
/// @param partDesc 
/// @return the length of the cluster chain
uint32_t clusterChainLen(ata_drive hd, uint32_t firstCluster, partition_descr *partDesc) {
    uint32_t clusterCount = 0;
    uint32_t nextFileCluster = firstCluster;
    //follows cluster chain
    for (; nextFileCluster < 0x0FFFFFF8; clusterCount++ )
    {
        nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
    }

    return clusterCount;
}
/// @brief finds a specific amount of free clusters
/// @param hd 
/// @param partDesc 
/// @param clusterAmount the size needed
/// @return pointer to an array of cluster indexs
uint32_t* find_free_clusters(ata_drive hd, partition_descr *partDesc, uint32_t clusterAmount) {
    if(clusterAmount > partDesc->FSInfo.freeClusterCount)
        return NULL;
    uint32_t* clustersIdxs = (uint32_t*) malloc(clusterAmount * sizeof(uint32_t));
    if(!clustersIdxs)
        return NULL;
    uint32_t currClusterIdx = 0;
    for (uint32_t i = 2; (i < partDesc->fatDesc.fat_size / 4) && (currClusterIdx < clusterAmount); i++)
    {
        if(read_fat_entry(hd, i, partDesc) == 0)
            clustersIdxs[currClusterIdx++]  = i;
    }
    return clustersIdxs;
}

/// @brief finds the cluster chain starting in a certain cluster
/// @param hd 
/// @param partDesc 
/// @param firstCluster the first cluster of the chain
/// @param clusterAmount the amount of clusters in the chain
/// @return an array containing the indexes of the clusters in the chain, NULL if no chain was found
uint32_t* find_cluster_chain(ata_drive hd, partition_descr *partDesc, uint32_t firstCluster, uint32_t clusterAmount) {

    if(firstCluster == 0)
        return NULL;
    
    uint32_t* clustersIdxs = (uint32_t*) malloc(clusterAmount * sizeof(uint32_t));

    uint32_t nextFileCluster = firstCluster;
    //follows cluster chain
    for(int i = 0; nextFileCluster < 0x0FFFFFF8; i++)
    {
        clustersIdxs[i] = nextFileCluster;
        nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
    }

    return clustersIdxs;
}

/// @brief finds the index of the last cluster in the chain
/// @param hd 
/// @param firstCluster the first cluster in the chain
/// @param partDesc 
/// @return the index of the last cluster in that chain
uint32_t get_last_cluster_in_chain(ata_drive hd, uint32_t firstCluster, partition_descr *partDesc) {
    uint32_t currFileCluster = firstCluster;
    uint32_t nextFileCluster = read_fat_entry(hd, currFileCluster, partDesc);
    //follows cluster chain
    while (nextFileCluster < 0x0FFFFFF8)
    {
        currFileCluster = nextFileCluster;
        nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
    }

    return currFileCluster;
}

/// @brief allocates space for data in the FAT
/// @param hd 
/// @param partDesc 
/// @param startCluster the last cluster of the current chain
/// @param clustersNeeded the amount clusters needed 
/// @return the amount of clusters allocated, -1 if there is not enough space
bool allocate_new_clusters(ata_drive hd, partition_descr *partDesc, uint32_t startCluster, int32_t clustersNeeded) {
    if(clustersNeeded == 0)
        return 1;
    
    uint32_t lastCluster = get_last_cluster_in_chain(hd, startCluster, partDesc);
    //checks if there are enough free clusters 
    if (((int32_t) partDesc->FSInfo.freeClusterCount) < clustersNeeded)
        return false;
    
    uint32_t* clustersIdx = find_free_clusters(hd, partDesc, clustersNeeded);
    
    if(!clustersIdx) {
        return false;
    }
    

    write_fat_entry(hd, lastCluster, partDesc, *clustersIdx);

    
    // creates cluster chain in fat
    for (int32_t i = 0; i < (clustersNeeded - 1); i++)
    {
        write_fat_entry(hd, clustersIdx[i], partDesc, clustersIdx[i+1]);
    }

    write_fat_entry(hd, clustersIdx[clustersNeeded - 1], partDesc, 0x0FFFFFF8);

    free(clustersIdx);

    //updates FSInfo
    partDesc->FSInfo.freeClusterCount -= clustersNeeded;
    update_FSInfo(hd, partDesc);

    return true;
}

/// @brief removes an amount of clusters from the end of cluster chain
/// @param hd 
/// @param partDesc 
/// @param startCluster the starting cluster of the chain
/// @param chainSize the size of the chain
/// @param clustersToRemove the amount of clusters to remove
void remove_clusters_from_chain(ata_drive hd, partition_descr *partDesc, uint32_t startCluster, uint32_t chainSize, int32_t clustersToRemove) {
    uint32_t* clustersIdx = find_cluster_chain(hd, partDesc, startCluster, chainSize);

    // if no chain found
    if(!clustersIdx)
        return;
    

    // creates cluster chain in fat
    for (int32_t i = chainSize + clustersToRemove; i < (int32_t) chainSize; i++)
    {
        write_fat_entry(hd, clustersIdx[i], partDesc, 0);
    }

    if ((int32_t)chainSize > -clustersToRemove)
        write_fat_entry(hd, clustersIdx[chainSize + clustersToRemove - 1], partDesc, 0x0FFFFFF8);

    partDesc->FSInfo.freeClusterCount -= clustersToRemove;
    update_FSInfo(hd, partDesc);

    free(clustersIdx);
}

inline static bool is_valid_sfn_char(char c) {
    return (strchr(SFN_ALLOWED, c) != NULL);
}

/// @brief creats sfn from provided lfn
/// @param lfn the lfn
/// @param name buffer for sfn name
/// @param ext buffer for sfn ext
/// @param id a unique id to stop duplications
void generate_sfn(const char *lfn, char* name, char* ext, int id) {
    int name_len = 0, ext_len = 0, i;

    //extracts name
    for(i = 0; lfn[i] && (lfn[i] != '.') && (name_len < 8); i++) {
        if(is_valid_sfn_char(toupper(lfn[i])))
            name[name_len++] = toupper(lfn[i]);
    }

    // if there is an ext add it
    if(lfn[i] == '.') {
        i++;
        for(; lfn[i] && (ext_len < 3); i++) {
            if(is_valid_sfn_char(toupper(lfn[i])))
                ext[ext_len++] = toupper(lfn[i]);
        }
    }

    if(id > 0) {
        name[6] = '~';
        itoa(id, name + 7, 10);
    }
}

/// @brief creates file in a specified directory 
/// @param hd 
/// @param dirCluster the first cluster of the dir in which the file should be created
/// @param fileName the name of the file
/// @param partDesc 
void create_file_by_dir_cluster(ata_drive hd, uint32_t dirCluster, char* fileName, partition_descr *partDesc) {
    directory_entry_fat32 dirent[16];

    uint32_t nextDirCluster = dirCluster;


    int lfnCount = (strlen(fileName) + 12)/13; // num of lfn entries needed
    uint16_t name_utf16[256];

    utf8_to_utf16(fileName, name_utf16);

    char name[9] = "        ", ext[4] =  "   ";
    generate_sfn(fileName, name, ext, 0);
    uint8_t checksum = 0;
    for (int i = 0; i < 11; i++) {
        if (i < 8)
            checksum = ((checksum >> 1) | (checksum << 7)) + name[i];
        else
            checksum = ((checksum >> 1) | (checksum << 7)) + ext[i - 8];
    }

    LFN_entry_fat32* lfns = (LFN_entry_fat32*) malloc(lfnCount * sizeof(LFN_entry_fat32));

    for (int i = 0; i < lfnCount; i++) {
        memset((uint8_t*)(lfns+i), 0xFF, sizeof(LFN_entry_fat32)); // Fill unused bytes

        lfns[i].order = (i + 1) | (i == lfnCount - 1 ? 0x40 : 0x00);
        lfns[i].attributes = 0x0F;
        lfns[i].longEntryType = 0x00;
        lfns[i].checksum = checksum;
        lfns[i].zero = 0;

        // Copy name parts
        memcpy(lfns[i].chars1, &name_utf16[i * 13], 5 * 2);
        memcpy(lfns[i].chars2, &name_utf16[i * 13 + 5], 6 * 2);
        memcpy(lfns[i].chars3, &name_utf16[i * 13 + 11], 2 * 2);
    }
    directory_entry_fat32* lfnsDirEnt = (directory_entry_fat32*) lfns;
    bool found = 0;
    while (!found && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            //findes empty entry
            read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            int emptyEntIdx = -1;
            int emptyEntrySeqLen = 0;
            for (int i = 0; (i < 16) && (emptyEntrySeqLen <= lfnCount); i++) {
                if(dirent[i].name[0] == 0x00) { // end of dir entries
                    emptyEntrySeqLen += 16 - i;
                    emptyEntIdx = emptyEntIdx == -1 ? i : emptyEntIdx;
                }
                if(dirent[i].name[0] == 0xE5) {
                    emptyEntrySeqLen++;
                    emptyEntIdx = emptyEntIdx == -1 ? i : emptyEntIdx;
                }
            }

            //checks if enough space was found
            if(emptyEntrySeqLen <= lfnCount)
                continue;
            
            found = 1;

            int i = 0;
            for( ; i < lfnCount; i++) {
                dirent[emptyEntIdx + lfnCount - 1 - i] = lfnsDirEnt[i];
            }

            free(lfns);
            int sfnEntIdx = i + emptyEntIdx;
            // creates dir entry for the file
            for (int j = 0; j < 8; j++)
                dirent[sfnEntIdx].name[j] = name[j];
            
            for (int j = 0; j < 3; j++) {
                dirent[sfnEntIdx].ext[j] = ext[j];
            }
            dirent[sfnEntIdx].size = 0;
            dirent[sfnEntIdx].attributes = 0x0;
            uint32_t firstCluster = 0;
            dirent[sfnEntIdx].firstClusterHi = (uint16_t )(firstCluster >> 16);
            dirent[sfnEntIdx].firstClusterLo = (uint16_t)(firstCluster & 0xffff);

            // updates dir
            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && !found);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
    if (!found)
        print_string("directory full\n");
}

/// @brief deletes the specified file in a specified dir
/// @param hd 
/// @param dirCluster the first cluster of the dir
/// @param fileName the name of the file
/// @param partDesc 
void delete_file_by_dir_cluster(ata_drive hd, uint32_t dirCluster, char* fileName, partition_descr *partDesc) {
    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;

    uint32_t nextDirCluster = dirCluster;
    bool moreEnt = true;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
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

                    if(strcmp(fileName, nameBuff)) {
                        lfnIdx = 0;
                        continue;
                    }
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

            uint32_t chainSize = clusterChainLen(hd, firstFileCluster, partDesc);
            remove_clusters_from_chain(hd, partDesc, firstFileCluster, chainSize, -chainSize);

            //marks all lfn entries and the file entry as deleted
            for(int j = fileEntIdx - lfnIdx; j <= fileEntIdx; j++)
                dirent[j].name[0] = 0xE5;
            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);

            break;
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
}

/// @brief initializes a new dir
/// @param hd 
/// @param dirFirstCluster the first cluster of the dir
/// @param partentDirFirstCluster the first cluster of the parent dir
/// @param partDesc 
void init_dir(ata_drive hd, uint32_t dirFirstCluster, uint32_t partentDirFirstCluster,partition_descr *partDesc) {
    directory_entry_fat32 dirent[2];
    empty_out_cluster(hd, dirFirstCluster, partDesc);

    uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (dirFirstCluster - 2);
    read28(hd, dirSector, (uint8_t*)&dirent[0], 2*sizeof(directory_entry_fat32));

    //sets up . and .. entries
    char* ext = "   ";
    char* dotName = ".       ";
    char* dotDotName = "..      ";
    for (int j = 0; j < 8; j++) {
        dirent[0].name[j] = dotName[j];
        dirent[1].name[j] = dotDotName[j];
    }
    
    for (int j = 0; j < 3; j++) {
        dirent[0].ext[j] = ext[j];
        dirent[1].ext[j] = ext[j];
    }

    dirent[0].size = 0;
    dirent[0].attributes = 0x10;
    dirent[0].firstClusterHi = (uint16_t )(dirFirstCluster >> 16);
    dirent[0].firstClusterLo = (uint16_t)(dirFirstCluster & 0xffff);

    dirent[1].size = 0;
    dirent[1].attributes = 0x10;
    dirent[1].firstClusterHi = (uint16_t )(partentDirFirstCluster >> 16);
    dirent[1].firstClusterLo = (uint16_t)(partentDirFirstCluster & 0xffff);


    write28(hd, dirSector, (uint8_t*)&dirent[0], 2*sizeof(directory_entry_fat32));
}

/// @brief creates a new dir 
/// @param hd 
/// @param parentCluster the first cluster of the parent dir 
/// @param dirName the name of the current dir
/// @param partDesc 
void create_dir_by_parent_cluster(ata_drive hd, uint32_t parentCluster, char* dirName, partition_descr *partDesc) {
    directory_entry_fat32 dirent[16];
    uint32_t nextDirCluster = parentCluster;

    int lfnCount = (strlen(dirName) + 12)/13; // num of lfn entries needed
    uint16_t name_utf16[256];

    utf8_to_utf16(dirName, name_utf16);

    char name[9] = "        ", ext[4] =  "   ";
    generate_sfn(dirName, name, ext, 0);
    uint8_t checksum = 0;
    for (int i = 0; i < 11; i++) {
        if (i < 8)
            checksum = ((checksum >> 1) | (checksum << 7)) + name[i];
        else
            checksum = ((checksum >> 1) | (checksum << 7)) + ext[i - 8];
    }

    LFN_entry_fat32* lfns = (LFN_entry_fat32*) malloc(lfnCount * sizeof(LFN_entry_fat32));

    for (int i = 0; i < lfnCount; i++) {
        memset((uint8_t*)(lfns+i), 0xFF, sizeof(LFN_entry_fat32)); // Fill unused bytes

        lfns[i].order = (i + 1) | (i == lfnCount - 1 ? 0x40 : 0x00);
        lfns[i].attributes = 0x0F;
        lfns[i].longEntryType = 0x00;
        lfns[i].checksum = checksum;
        lfns[i].zero = 0;

        // Copy name parts
        memcpy(lfns[i].chars1, &name_utf16[i * 13], 5 * 2);
        memcpy(lfns[i].chars2, &name_utf16[i * 13 + 5], 6 * 2);
        memcpy(lfns[i].chars3, &name_utf16[i * 13 + 11], 2 * 2);
    }
    directory_entry_fat32* lfnsDirEnt = (directory_entry_fat32*) lfns;
    bool found = 0;
    while (!found && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
        do {
            //findes empty entry
            read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            int emptyEntIdx = -1;
            int emptyEntrySeqLen = 0;
            for (int i = 0; (i < 16) && (emptyEntrySeqLen <= lfnCount); i++) {
                if(dirent[i].name[0] == 0x00) { // end of dir entries
                    emptyEntrySeqLen += 16 - i;
                    emptyEntIdx = emptyEntIdx == -1 ? i : emptyEntIdx;
                }
                if(dirent[i].name[0] == 0xE5) {
                    emptyEntrySeqLen++;
                    emptyEntIdx = emptyEntIdx == -1 ? i : emptyEntIdx;
                }
            }

            //checks if enough space was found
            if(emptyEntrySeqLen <= lfnCount)
                continue;
            
            found = 1;

            int i = 0;
            for( ; i < lfnCount; i++) {
                dirent[emptyEntIdx + lfnCount - 1 - i] = lfnsDirEnt[i];
            }

            free(lfns);
            int sfnEntIdx = i + emptyEntIdx;
            // creates dir entry for the file
            for (int j = 0; j < 8; j++)
                dirent[sfnEntIdx].name[j] = name[j];
            
            for (int j = 0; j < 3; j++) {
                dirent[sfnEntIdx].ext[j] = ext[j];
            }
            dirent[sfnEntIdx].size = 0;
            dirent[sfnEntIdx].attributes = 0x10;
            uint32_t firstCluster = find_empty_Cluster(hd, partDesc);
            dirent[sfnEntIdx].firstClusterHi = (uint16_t )(firstCluster >> 16);
            dirent[sfnEntIdx].firstClusterLo = (uint16_t)(firstCluster & 0xffff);

            partDesc->FSInfo.freeClusterCount -= 1;
            update_FSInfo(hd, partDesc);

            // updates dir and fat
            write_fat_entry(hd, firstCluster, partDesc, 0x0FFFFFF8);
            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);

            init_dir(hd, firstCluster, parentCluster, partDesc);
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && !found);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }

    if (!found) {
        free(lfns);
        print_string("directory full\n");
    }
}

/// @brief clears all contents of a dir
/// @param hd 
/// @param dirCluster the first cluster of the dir
/// @param partDesc 
void clear_dir_by_cluster (ata_drive hd, uint32_t dirCluster, partition_descr *partDesc) {
    directory_entry_fat32 dirent[16];
    uint32_t nextDirCluster = dirCluster;
    bool moreEnt = 1;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
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
                    continue;
                }

                uint32_t firstEntCluster = ((uint32_t)dirent[i].firstClusterHi) << 16
                                          | ((uint32_t)dirent[i].firstClusterLo);
                // if dir, recursivly read its contents
                if((dirent[i].attributes & 0x10) == 0x10) {
                    if (dirent[i].name[0] == '.')
                        continue;
                    clear_dir_by_cluster(hd, firstEntCluster, partDesc);
                }

                uint32_t chainSize = clusterChainLen(hd, firstEntCluster, partDesc);
                remove_clusters_from_chain(hd, partDesc, firstEntCluster, chainSize, -chainSize);
            }
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
}

/// @brief deletes a dir
/// @param hd 
/// @param parentCluster the first cluster of the dir
/// @param dirName the name of the dir to be delted
/// @param partDesc 
void delete_dir_by_parent_cluster(ata_drive hd, uint32_t parentCluster, char* dirName, partition_descr *partDesc) {
    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;
    uint32_t firstDirCluster;
    uint32_t nextParentDirCluster = parentCluster;
    bool moreEnt = true;
    bool found = false;
    // finds dir first cluster and deletes its entry from the parent dir
    while (moreEnt && (nextParentDirCluster < 0x0FFFFFF8) && !found) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextParentDirCluster - 2);
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

                    if(strcmp(dirName, nameBuff)) {
                        lfnIdx = 0;
                        continue;
                    }
                } else {
                    continue;
                }

                fileEntIdx = i;
                break;
            }
            if(fileEntIdx == -1)
                continue;
            
            found = true;

            // gets first cluster of file
            firstDirCluster = ((uint32_t)dirent[fileEntIdx].firstClusterHi) >> 16
                            | ((uint32_t)dirent[fileEntIdx].firstClusterLo);
            //marks all lfn entries and the file entry as deleted
            for(int j = fileEntIdx - lfnIdx; j <= fileEntIdx; j++)
                dirent[j].name[0] = 0xE5;
            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);
            break;
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt && !found);
        // gets next cluster belonging to the dir
        nextParentDirCluster = read_fat_entry(hd, nextParentDirCluster, partDesc);
    }

    //deletes the dir
    clear_dir_by_cluster(hd, firstDirCluster, partDesc);
    uint32_t chainSize = clusterChainLen(hd, firstDirCluster, partDesc);
    remove_clusters_from_chain(hd, partDesc, firstDirCluster, chainSize, -chainSize);
}


/// @brief rewrites a file in a certian dir
/// @param hd 
/// @param dirFirstClust the first cluster of the dir in which the file is located
/// @param fileName the name of the file
/// @param partDesc 
/// @param data the data to write
/// @param size size of the data to write
void rewrite_file_by_dir_cluster(ata_drive hd, uint32_t dirFirstClust, const char *fileName, partition_descr *partDesc, const char *data, uint32_t size) {
    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;

    uint32_t nextDirCluster = dirFirstClust;
    bool moreEnt = true;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
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

            uint32_t currentClusterAmount = dirent[fileEntIdx].size / (SECTOR_SIZE * partDesc->bpb.sectorPerCluster);
            if (dirent[fileEntIdx].size % (SECTOR_SIZE * partDesc->bpb.sectorPerCluster))
                currentClusterAmount++;
            uint32_t neededClusterAmount = size / (SECTOR_SIZE * partDesc->bpb.sectorPerCluster);
            if (size % (SECTOR_SIZE * partDesc->bpb.sectorPerCluster))
                neededClusterAmount++;

            int32_t clustersNeeded = (int32_t)neededClusterAmount - (int32_t)currentClusterAmount;
            int noClusters = (clustersNeeded > 0 && firstFileCluster == 0);
            if(noClusters) {
                firstFileCluster = find_empty_Cluster(hd, partDesc);
                write_fat_entry(hd, firstFileCluster, partDesc, 0x0FFFFFF8);
                dirent[fileEntIdx].firstClusterHi = (uint16_t )(firstFileCluster >> 16);
                dirent[fileEntIdx].firstClusterLo = (uint16_t)(firstFileCluster & 0xffff);
                clustersNeeded--;
            }
            if (clustersNeeded >= 0) {
                if ( firstFileCluster >= 0x0FFFFFF8 || !allocate_new_clusters(hd, partDesc, firstFileCluster, clustersNeeded)) {
                    print_string("Not enough Space");
                    break;
                }
                partDesc->FSInfo.freeClusterCount -= noClusters;
                update_FSInfo(hd, partDesc);
            } else {                
                remove_clusters_from_chain(hd, partDesc, firstFileCluster, currentClusterAmount, clustersNeeded);
            }


            // updates the entry size
            dirent[fileEntIdx].size = size;
            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);

            // writes data to cluster chain
            int32_t SIZE = size;
            uint32_t nextFileCluster = firstFileCluster;
            while (nextFileCluster < 0x0FFFFFF8 )
            {
                
                uint32_t fileSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextFileCluster - 2);
                int sectorOffset = 0;
                

                // reads all sectors belonging to file from curr file cluster
                for (; SIZE > 0; SIZE -= 512)
                {
                    int amountToWrite =  (SIZE > 512 ? 512 : SIZE);
                    write28(hd, fileSector + sectorOffset, (uint8_t*)data, amountToWrite);
                    flush(hd);
                    data += amountToWrite;

                    if(++sectorOffset > partDesc->bpb.sectorPerCluster) {
                        SIZE -= 512;
                        break;
                    }
                }
                nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
                
            }

            break;
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
}

/// @brief appends data to the end of file in a certian dir
/// @param hd 
/// @param dirFirstClust the first cluster of the dir in which the file is located
/// @param fileName the name of the file
/// @param partDesc 
/// @param data the data to write
/// @param size size of the data to write
void append_to_file_by_dir_cluster(ata_drive hd, uint32_t dirFirstClust, const char *fileName, partition_descr *partDesc, const char *data, uint32_t size) {
    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;

    uint32_t nextDirCluster = dirFirstClust;
    bool moreEnt = true;

    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
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

            uint32_t currentClusterAmount = dirent[fileEntIdx].size / (SECTOR_SIZE * partDesc->bpb.sectorPerCluster);
            if (dirent[fileEntIdx].size % (SECTOR_SIZE * partDesc->bpb.sectorPerCluster))
                currentClusterAmount++;
            uint32_t neededClusterAmount = (dirent[fileEntIdx].size + size) / (SECTOR_SIZE * partDesc->bpb.sectorPerCluster);
            if ((dirent[fileEntIdx].size + size) % (SECTOR_SIZE * partDesc->bpb.sectorPerCluster))
                neededClusterAmount++;

            int32_t clustersNeeded = (int32_t)neededClusterAmount - (int32_t)currentClusterAmount;
            int noClusters = (clustersNeeded > 0 && firstFileCluster == 0);
            if(noClusters) {
                firstFileCluster = find_empty_Cluster(hd, partDesc);
                write_fat_entry(hd, firstFileCluster, partDesc, 0x0FFFFFF8);
                dirent[fileEntIdx].firstClusterHi = (uint16_t )(firstFileCluster >> 16);
                dirent[fileEntIdx].firstClusterLo = (uint16_t)(firstFileCluster & 0xffff);
                clustersNeeded--;
            }
            if (clustersNeeded >= 0) {
                if ( firstFileCluster >= 0x0FFFFFF8 || !allocate_new_clusters(hd, partDesc, firstFileCluster, clustersNeeded)) {
                    print_string("Not enough Space");
                    break;
                }
                partDesc->FSInfo.freeClusterCount -= noClusters;
                update_FSInfo(hd, partDesc);
            } else {                
                remove_clusters_from_chain(hd, partDesc, firstFileCluster, currentClusterAmount, clustersNeeded);
            }


            // updates the entry size
            dirent[fileEntIdx].size = dirent[fileEntIdx].size + size;
            write28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
            flush(hd);

            // writes data to cluster chain
            int32_t SIZE = dirent[fileEntIdx].size;
            uint32_t nextFileCluster = firstFileCluster;
            while (nextFileCluster < 0x0FFFFFF8 )
            {
                
                uint32_t fileSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextFileCluster - 2);
                int sectorOffset = 0;
                

                // reads all sectors belonging to file from curr file cluster
                for (; SIZE > 0; SIZE -= 512)
                {
                    if((SIZE - 512) < (int32_t) size) {
                        int amountToWrite =  (SIZE > 512 ? 512 : SIZE);
                        int dataToKeep = SIZE - size;
                        if (dataToKeep > 0) {
                            amountToWrite -= dataToKeep;
                            uint8_t sectorToWrite[512];
                            read28(hd, fileSector + sectorOffset, sectorToWrite, dataToKeep);
                            for(int i = 0; i < amountToWrite; i++) {
                                sectorToWrite[i + dataToKeep] = data[i];
                            }
                            write28(hd, fileSector + sectorOffset, sectorToWrite, amountToWrite + dataToKeep);
                            flush(hd);
                        } else {
                            write28(hd, fileSector + sectorOffset, (uint8_t*)data, amountToWrite);
                            flush(hd);
                        }
                        data += amountToWrite;
                    }
                    if(++sectorOffset > partDesc->bpb.sectorPerCluster) {
                        SIZE -= 512;
                        break;
                    }
                }
                nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
                
            }

            break;
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
}

/// @brief reads a dir and prints its contents
/// @param hd
/// @param firstCluster first cluster of the dir
void read_dir_by_cluster(ata_drive hd, uint32_t firstCluster, partition_descr *partDesc) {
    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;
    uint32_t nextDirCluster = firstCluster;
    bool moreEnt = 1;
    
    while (moreEnt && (nextDirCluster < 0x0FFFFFF8)) {
        int dirSectorOffset = 0;
        uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
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
                    print_string(nameBuff);
                    if((dirent[i].attributes & 0x10) == 0x10) {
                        print_string("/    ");
                        print_string("<DIR>");
                    } else {
                        print_string("    ");
                        print_int(dirent[i].size, 10);
                        print_string(" bytes");
                    }
                    lfnIdx = 0;
                } else {
                    if(dirent[i].name[0] == '.')
                        continue;
                    print((char*) dirent[i].name, 8);
                    if((dirent[i].attributes & 0x10) != 0x10) {
                        print_string(".");
                        print((char*) dirent[i].ext, 3);
                        print_string("    ");
                        print_int(dirent[i].size, 10);
                        print_string(" bytes");
                    } else {
                        print_string("/    ");
                        print_string("<DIR>");
                    }
                }
                print_string("\n");
            }
        } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt);
        // gets next cluster belonging to the dir
        nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
    }
    print_string("\n");
}

/// @brief checks whether or not a certain file is in a dir
/// @param hd 
/// @param dirFirstCluster 
/// @param fileName
/// @param partDesc 
/// @return true if found, false otherwise
bool is_file_in_dir(ata_drive hd, uint32_t dirFirstCluster, const char* fileName, partition_descr *partDesc) {
    uint32_t nextDirCluster = dirFirstCluster;

    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;
    bool moreEnt = true;
    while ((nextDirCluster < 0x0FFFFFF8) && moreEnt) {
            int dirSectorOffset = 0;
            uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
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

                    return true;
                }
            } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt);
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
directory_entry_fat32 find_file_dir_entry(ata_drive hd, uint32_t dirFirstCluster, const char* fileName, partition_descr *partDesc) {
    uint32_t nextDirCluster = dirFirstCluster;

    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;

    bool moreEnt = true;
    while ((nextDirCluster < 0x0FFFFFF8) && moreEnt) {
            int dirSectorOffset = 0;
            uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
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
            } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt);
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
void read_file_by_cluster(ata_drive hd, uint32_t firstCluster, uint32_t size, partition_descr *partDesc) {
    //if file is empty
    if (size == 0)
        return;
    
    int32_t SIZE = (int32_t)size;
    uint32_t nextFileCluster = firstCluster;
    uint8_t buffer[513];
    //follows cluster chain
    while (nextFileCluster < 0x0FFFFFF8)
    {
        
        uint32_t fileSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextFileCluster - 2);
        int sectorOffset = 0;
        
        // reads all sectors belonging to file from curr file cluster
        for (; SIZE > 0; SIZE -= 512)
        {
            read28(hd, fileSector + sectorOffset, buffer, 512);
            //buffer[] = '\0';

            if(++sectorOffset > partDesc->bpb.sectorPerCluster)
                break;
            print((char*)buffer, SIZE > 512 ? 512 : SIZE);
        }
        
        nextFileCluster = read_fat_entry(hd, nextFileCluster, partDesc);
        
    }
}

/// @brief finds the first cluster of the entry in a specifed path
/// @param hd 
/// @param path the specified path
/// @param partDesc 
/// @return the first cluster index of the entry
uint32_t find_dir_first_cluster(ata_drive hd, const char* path, partition_descr *partDesc) {
    uint32_t currentDirCluster;

    //checks if path starts from root or CWD
    if(path[0] == '/')
        currentDirCluster = partDesc->bpb.rootCluster;
    else
        currentDirCluster = partDesc->currentWorkingDir;
    
    char* tempPath = (char*) malloc(strlen(path) + 1);
    strcpy(tempPath, path);
    directory_entry_fat32 dirent[16];
    LFN_entry_fat32 lfnEnt[20];
    int lfnIdx = 0;
    char* nextDirName = strtok((char*) tempPath, "/");
    while(nextDirName) {
        uint32_t nextDirCluster = currentDirCluster;
        bool moreEnt = 1;
        bool found = 0;
        while (moreEnt && (nextDirCluster < 0x0FFFFFF8) && !found) {

            int dirSectorOffset = 0;
            uint32_t dirSector = partDesc->fatDesc.data_start + partDesc->bpb.sectorPerCluster * (nextDirCluster - 2);
            do {
                read28(hd, dirSector + dirSectorOffset, (uint8_t*)&dirent[0], 16*sizeof(directory_entry_fat32));
                if (strcmp(nextDirName, ".") == 0) { // Checks if . is used
                    found = 1;
                    continue;
                } else if (strcmp(nextDirName, "..") == 0) { // Checks if .. is used
                    if (currentDirCluster != partDesc->bpb.rootCluster) {
                        uint32_t EntCluster = ((uint32_t)dirent[1].firstClusterHi) << 16
                        | ((uint32_t)dirent[1].firstClusterLo);
                        currentDirCluster = EntCluster;
                    }
                    found = 1;
                    continue;
                }
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

                        if((dirent[i].attributes & 0x10) != 0x10)
                            continue;

                        if(strcmp(nextDirName, nameBuff))
                            continue;

                    } else {
                            continue;
                    }
                    
                    uint32_t EntCluster = ((uint32_t)dirent[i].firstClusterHi) << 16
                                            | ((uint32_t)dirent[i].firstClusterLo);


                    currentDirCluster = EntCluster;
                    found = 1;
                }
            } while ((++dirSectorOffset <= partDesc->bpb.sectorPerCluster) && moreEnt && !found);
            // gets next cluster belonging to the dir
            nextDirCluster = read_fat_entry(hd, nextDirCluster, partDesc);
        }
        if (!found) {
            free(tempPath);
            return 0xFFFFFFFF;
        }
        
        nextDirName = strtok(0, "/");
    }
    free(tempPath);
    return currentDirCluster;
}

/// @brief changes the current working directory
/// @param hd 
/// @param path the path of the new wd
/// @param partDesc 
void change_current_working_dir(ata_drive hd, const char* path, partition_descr *partDesc) {
    uint32_t dirCluster = find_dir_first_cluster(hd, path, partDesc);
    if(dirCluster == 0xFFFFFFFF) { //invalid path
        print_string("No such Directory");
        return;
    }

    partDesc->currentWorkingDir = dirCluster;

    char combined[512];
    char* tokens[64];
    int depth = 0;

    //update simple path
    if(path[0] == '/') {
        strcpy(combined, path);
    } else {
        char* merged = concat(partDesc->CWDString, "/");
        strcpy(combined, merged);
        free(merged);
        merged = concat(combined, path);
        strcpy(combined, merged);
        free(merged);
    }

    // process path
    char *token = strtok(combined, "/");
    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            if (depth > 0) depth--;  // goes back one dir
        } else if (strcmp(token, ".") != 0 && strcmp(token, "") != 0) {
            tokens[depth++] = token;  // adds to path
        }
        token = strtok(NULL, "/");
    }

    if (depth == 0) {
        strcpy(partDesc->CWDString, "/");
    } else {
        int len = 0;
        for (int i = 0; i < depth; i++) {
            partDesc->CWDString[len++] = '/';
            strcpy(partDesc->CWDString + len, tokens[i]);
            len += strlen(tokens[i]);
        }
    }
}

/// @brief reads the dir specified in path
/// @param hd 
/// @param path 
/// @param partDesc 
void read_dir(ata_drive hd, const char* path, partition_descr *partDesc) {
    read_dir_by_cluster(hd, find_dir_first_cluster(hd, path, partDesc), partDesc);
}

/// @brief reads a certian file
/// @param hd 
/// @param dirPath the path of the dir the file is loacted in
/// @param fileName the name of the file
/// @param partDesc 
void read_file(ata_drive hd, const char* dirPath, const char* fileName, partition_descr *partDesc) {
    uint32_t dirsCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(dirsCluster == 0xFFFFFFFF) { //invalid path
        print_string("invalid path");
        return;
    }
    if(!is_file_in_dir(hd, dirsCluster, fileName, partDesc)) { // file not found
        print_string("file not found");
        return;
    } 

    directory_entry_fat32 fileEnt = find_file_dir_entry(hd, dirsCluster, fileName, partDesc);
    uint32_t firstFileClust = ((uint32_t)fileEnt.firstClusterHi) << 16
    | ((uint32_t)fileEnt.firstClusterLo);
    read_file_by_cluster(hd, firstFileClust, fileEnt.size, partDesc);
}

/// @brief creates a file
/// @param hd 
/// @param dirPath the path of the dir in which the file needs to be created
/// @param fileName the name of the file
/// @param partDesc 
void create_file(ata_drive hd, char* path, char* fileName, partition_descr *partDesc) {
    uint32_t dirCluster = find_dir_first_cluster(hd, path, partDesc);

    //directory not found
    if(dirCluster == 0xFFFFFFFF) {
        print_string("invalid path");
        return;
    }

    //file already exists
    if(is_file_in_dir(hd, dirCluster, fileName, partDesc)) {
        print_string("flie exists");
        return;
    }

    create_file_by_dir_cluster(hd, dirCluster, fileName, partDesc);
}

/// @brief deletes a certain file
/// @param hd 
/// @param dirPath the path of the dir the file is loacted in
/// @param fileName the name of the file
/// @param partDesc 
void delete_file(ata_drive hd, char* dirPath, char* fileName, partition_descr *partDesc) {
    uint32_t dirCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(dirCluster == 0xFFFFFFFF) { //invalid path
        print_string("invalid path");
        return;
    }
    if(!is_file_in_dir(hd, dirCluster, fileName, partDesc)) { // file not found
        print_string("file not found");
        return;
    } 
    delete_file_by_dir_cluster(hd, dirCluster, fileName, partDesc);
}

/// @brief creates a dir
/// @param hd 
/// @param path the path of the dir in which the new dir needs to be placed
/// @param dirName the name of the dir
/// @param partDesc 
void create_dir(ata_drive hd, char* path, char* dirName, partition_descr *partDesc) {
    uint32_t parentCluster = find_dir_first_cluster(hd, path, partDesc);
    if(parentCluster == 0xFFFFFFFF) { //invalid path
        print_string("invalid path");
        return;
    }
    if(is_file_in_dir(hd, parentCluster, dirName, partDesc)) { // dir already exists
        print_string("dir already exists");
        return;
    }

    create_dir_by_parent_cluster(hd, parentCluster, dirName, partDesc);
}

/// @brief deletes a certain dir
/// @param hd 
/// @param dirPath the path of the parent dir of dir
/// @param dirName the name of the dir
/// @param partDesc 
void delete_dir(ata_drive hd, char* dirPath, char* dirName, partition_descr *partDesc) {
    uint32_t parentDirCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(parentDirCluster == 0xFFFFFFFF) { //invalid path
        print_string("invalid path");
        return;
    }
    if(!is_file_in_dir(hd, parentDirCluster, dirName, partDesc)) { // file not found
        print_string("dir not found");
        return;
    } 
    delete_dir_by_parent_cluster(hd, parentDirCluster, dirName, partDesc);
}

/// @brief writes to a specified file
/// @param hd 
/// @param dirPath the path of the dir the file is loacted in
/// @param fileName the name of the file
/// @param data what to write
/// @param size the size of the data to write
/// @param rewrite whether to append the data or rewrite the file
/// @param partDesc 
void write_to_file(ata_drive hd, const char* dirPath,const char *fileName, const char *data, uint32_t size, bool rewrite, partition_descr *partDesc) {
    uint32_t dirsCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(dirsCluster == 0xFFFFFFFF) { //invalid path
        print_string("invalid path");
        return;
    }
    if(!is_file_in_dir(hd, dirsCluster, fileName, partDesc)) { // file not found
        print_string("file not found: ");
        return;
    } 
    if (rewrite) {
        rewrite_file_by_dir_cluster(hd, dirsCluster, fileName, partDesc, data, size);
    } else {
        append_to_file_by_dir_cluster(hd, dirsCluster, fileName, partDesc, data, size);
    }
}

/// @brief checks if a specified file exists
/// @param hd 
/// @param dirPath the path of the dir the file should be loacted in
/// @param fileName the name of the file
/// @param partDesc 
/// @return true if exists, false otherwise
bool is_file_exist(ata_drive hd, const char* dirPath, const char *fileName, partition_descr *partDesc) {
    uint32_t dirsCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(dirsCluster == 0xFFFFFFFF) { //invalid path
        return false;
    }
    if(!is_file_in_dir(hd, dirsCluster, fileName, partDesc)) { // file not found
        return false;
    }
    
    return true;
}

/// @brief checks if a specified dir exists
/// @param hd 
/// @param dirPath the path of the dir
/// @param partDesc 
/// @return true if exists, false otherwise
bool is_dir_exist(ata_drive hd, const char* dirPath, partition_descr *partDesc) {
    uint32_t dirsCluster = find_dir_first_cluster(hd, dirPath, partDesc);
    if(dirsCluster == 0xFFFFFFFF) { //invalid path
        return false;
    }
    
    return true;
}

/// @brief writes the contents of the whole file system
/// @param hd 
/// @param partDesc 
void tree(ata_drive hd, partition_descr *partDesc) {
    read_dir(hd, "", partDesc); 
}