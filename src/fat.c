#include "./fat.h"
#include "./fdc.h"
#include "./string.h"

// FAT Copies
// First copy is fat0 stored at 
// Second copy is fat1
// There were issues declaring the FATs as non-pointers
// When they would get read from floppy, it would overwrite wrong areas of memory
fat_t *fat0;
fat_t *fat1;
void *startAddress = (void *) 0x20000;

directory_t currentDirectory;  // The current directory we have opened
directory_entry_t rootDirectoryEntry;   // The root directory's directory entry (this does not exist on the disk since the root is not inside of another directory)
file_t currentFile;            // The current file we have opened

// Initialize the file system
// Loads the FATs and root directory
void init_fs()
{
    // The FATs and directory are loaded into 0x20000, 0x21200, and 0x22400
    // These addresses were chosen because they are far enough away from the kernel (0x01000 - 0x07000)

    // Read the first copy of the FAT (Drive 0, Cluster 1, 512 bytes * 9 clusters)
    fat0 = (fat_t *) startAddress; // Put FAT at 0x20000
    floppy_read(0, 1,  (void *)fat0, sizeof(fat_t));

    // Read the second copy of the FAT (Drive 0, Cluster 10, 512 bytes * 9 clusters)
    fat1 = (fat_t *) (startAddress+sizeof(fat_t)); // Put FAT at 0x21200
    floppy_read(0, 10, (void *)fat1, sizeof(fat_t));

    // Read the root directory (Drive 0, Cluster 19, 512 bytes * 14 clusters)
    currentDirectory.isOpened = 1;
    currentDirectory.directoryEntry = &rootDirectoryEntry;

    currentDirectory.startingAddress = (uint8 *) (startAddress+(sizeof(fat_t)*2)); // Put ROOT at 0x22400
    stringcopy("ROOT    ", (char *)currentDirectory.directoryEntry->filename, 8);

    floppy_read(0, 19, (void *)currentDirectory.startingAddress, 512 * 14);

    // Start our file out blank
    currentFile.isOpened = 0;
    currentFile.directoryEntry = 0;
    currentFile.index = 0;
    currentFile.startingAddress = 0;
}

int closeFile() //definitely had the hardest time with this one.
                //So if this one seems all over the place its because a lot got shifted around.
{               //i wrote a lot of this without realizing it asks if you want to add sectors to the file.
                //it somehow works, im sure ive missed some functionality here but i ran out of time.
    
    uint16 fileCluster = currentFile.directoryEntry->startingCluster;
    uint32 fileSize = currentFile.directoryEntry ->fileSize;
    uint16 nextCluster = 0;
    uint8 *block = currentFile.startingAddress;

    if(!currentFile.isOpened){
        printf("no file to close"); //for debugging****
        return -1;
    }

    while(fileSize > 0){
        floppy_write(0, (fileCluster + 31), block, 512);
        fileSize -= (fileSize >= 512) ? 512 :fileSize;
        block += 512;
        
        if(fileSize > 0){
            nextCluster = fat0->clusters[fileCluster];

        if (nextCluster == 0xffff || nextCluster == 0x0000){
            nextCluster = 0;
        
        for(uint32 i = 2; i < 2304; i++){
            if (fat0->clusters[i] == 0x0000){
                nextCluster = i;
                break;
            }
        }
    
            if(nextCluster == 0){
                printf("not enough space");
                return -1;
            }
                         
                fat0->clusters[fileCluster] = nextCluster;
                fat1->clusters[fileCluster] = nextCluster;

        }

                fileCluster = nextCluster;

    }
}

                fat0->clusters[fileCluster] = 0xffff;
                fat1->clusters[fileCluster] = 0xffff;
                  
        floppy_write(0,19, (void *)currentDirectory.startingAddress, 512);
        floppy_write(0, 1, (uint8 *)fat0, sizeof(fat_t));
        floppy_write(0, 10, (uint8 *)fat1, sizeof(fat_t));
        currentFile.isOpened = 0;
        return 0;

}

int createFile(char *filename, char *ext)
{
    uint8 *directoryAddress = currentDirectory.startingAddress;
    uint32 directorySize = 7168; //14 sectors. each directory 32 bytes, 224 possible entries
    uint32 entrySize = 32;
    uint8 *directoryEnd = directoryAddress + directorySize;
    directory_entry_t *newEntry = 0;

    while (directoryAddress < directoryEnd) {
        if (directoryAddress[0] == 0x00) {
            newEntry = (directory_entry_t *)directoryAddress;
            stringcopy(filename, (char *)newEntry->filename, 8);
            stringcopy(ext, (char *)newEntry->ext, 3);
            newEntry->fileSize = 512;
            newEntry->startingCluster = 0; //set after making fat check
            
            newEntry->attributes =0;    //included all of these, not sure if necessary for this project 
            newEntry->reserved = 0;    
            newEntry->creationTime = 0;   
            newEntry->creationDate = 0;  
            newEntry->lastAccessDate = 0; 
            newEntry->ignored = 0;      
            newEntry->lastWriteTime = 0; 
            newEntry->lastWriteDate = 0; 

            break;

        }

        directoryAddress += entrySize; //should move to next directory index 0
    }

     if(newEntry == 0){ //if no entry is modified it should return this error
    printf("Error: Directory full\n");
    return -1;
    }

     for(uint32 i = 2; i < 2304; i++){
        if (fat0->clusters[i] == 0x0000){
            newEntry->startingCluster = i;
            fat0->clusters[i] = 0xffff;
            fat1->clusters[i] = 0xffff;
            floppy_write(0,19, currentDirectory.startingAddress, directorySize);
            floppy_write(0, 1, (uint8 *)fat0, sizeof(fat_t));
            floppy_write(0, 10, (uint8 *)fat0, sizeof(fat_t));
            currentFile.isOpened = 0;
            return 0;
        }

    }
 
    printf("Error: Directory full\n");
    return -1;
    
}

int deleteFile()//pretty easy after doing the other two.
                //a good way to show how your files arent really deleted
                //until theyre over written or shredded. 
{
uint16 cluster = currentFile.directoryEntry->startingCluster;

    if(!currentFile.isOpened){
        printf("no file to close"); //for debugging****
        return -1;
    }
    if(fat0->clusters[cluster] == 0xffff){
        fat0->clusters[cluster] = 0x0000;
    }

    while(cluster != 0xffff && cluster != 0x0000){
        uint16 nextCluster = fat0->clusters[cluster];
        fat0->clusters[cluster] = 0x0000;
        fat1->clusters[cluster] = 0x0000;
        cluster = nextCluster;
    }
    currentFile.directoryEntry->filename[0] = 0x00;
    floppy_write(0,19, currentDirectory.startingAddress, 7168);
    floppy_write(0, 1, (uint8 *)fat0, sizeof(fat_t));
    floppy_write(0, 10, (uint8 *)fat0, sizeof(fat_t));
    currentFile.isOpened = 0;
    return 0;
}

// Returns a byte from a file that is currently loaded into memory
// This does NOT modify the floppy disk
// This function requires the file to have been loaded into memory with floppy_read()
uint8 readByte(uint32 index)
{
    // Are we trying to read from the end of a file?
    if(index >= currentFile.directoryEntry->fileSize)
    {
        return -2;
    }

    // Check if the file is opened and is not a NULL nextCluster
    if(currentFile.isOpened && currentFile.startingAddress != 0)
    {
        currentFile.index = index + 1;              // Point us to the next index
        // Return the byte at the specified index
        return currentFile.startingAddress[index];
    }

    // If the file was not opened, or was a NULL nextCluster, return error
    printf("Error: File was not opened or pointed to NULL!\n");
    return -1;
}

// Returns the next byte from a file that is currently loaded into memory
uint8 readNextByte()
{
    return readByte(currentFile.index);
}

// Writes a byte to the current file that is currently loaded into memory
// This does NOT modify the floppy disk
// To write this to the floppy disk, we have to call floppy_write()
int writeByte(uint8 byte, uint32 index)
{
    // Check if the file is opened and is not a NULL nextCluster
    if(currentFile.isOpened && currentFile.startingAddress != 0)
    {
        currentFile.startingAddress[index] = byte;  // Place the byte at the directoryAddress + index
        if(index + 1 > currentFile.directoryEntry->fileSize) currentFile.directoryEntry->fileSize = index + 1;    // Increase the file size
        currentFile.index = index + 1;              // Point us to the next index
        return 0;
    }

    // If the file was not opened, or was a NULL nextCluster, return error
    printf("Error: File was not opened or pointed to NULL!\n");
    return -1;
}

// Writes a byte to the current file that is currently loaded into memory at the next index
int writeNextByte(uint8 byte)
{
    return writeByte(byte, currentFile.index);
}

// Writes a byte to the current file that is currently loaded into memory at the next index multiple times
int writeBytes(uint8 byte, uint32 count)
{
    for(int i = 0; i < (int)count; i++)
    {
        int error = writeByte(byte, currentFile.index);

        if (error != 0) return error;
    }

    return 0;
}

// Finds a file within our current directory and loads every sector of the file into memory
// Returns 0 if the file was found in the current directory
// Returns -3 if the file was not found in the current directory
// Returns other error codes if something went wrong
int openFile(char *filename, char *ext)
{
	// If a file is open, stop!
    if(currentFile.isOpened)
    {
        printf("A file is already open! Please close this file before opening another!\n");
        return 0;
    }
	
    // Scrub null terminators from our filename and extension and pad with spaces for more accurate comparisons
    char nullFound = 0;
    for(int i = 1; i < 8; i++)
    {
        if (filename[i] == 0 && !nullFound) nullFound = 1;
        if (nullFound) filename[i] = ' ';
    }

    nullFound = 0;
    for(int i = 1; i < 3; i++)
    {
        if (ext[i] == 0 && !nullFound) nullFound = 1;
        if (nullFound) ext[i] = ' ';
    }

    // Get a nextCluster to the first directoryAddress of the directory entry in this directory
	directory_entry_t *directoryEntry = (directory_entry_t *)currentDirectory.startingAddress;
    uint32 maxDirectoryEntryCount = 16;
    char fileExists = 0;

    // Assume we can have a maximum of 16 directory entries per directory
    // Check each one to see if our filename and extension match
    // This should actually be checking if the first byte of the directory entry is null
	for(uint32 index = 0; index < maxDirectoryEntryCount; index++)
	{
        // Check if this entry has the same file name and extension
		fileExists = stringcompare((char *)directoryEntry->filename, filename, 8) && stringcompare((char *)directoryEntry->ext, ext, 3);
		
        // If we found the file we can stop looping!
		if(fileExists) break;

		directoryEntry++;
	}

    // If the file exists, let's open it
    if(fileExists)
    {
        // Check if the file has been corrupted
        // Check each entry in the FAT table, ensure both FATs are consistent before opening the file
        uint16 cluster = directoryEntry->startingCluster;
        while(cluster != 0xFFFF)
        {
            // Check if the copies of the FATs are consistent
            if(fat0->clusters[cluster] != fat1->clusters[cluster])
            {
                printf("Error: The file was found BUT the FAT table entries for this file differ!\n");
                return -1;
            }

            // Get the next cluster
            cluster = fat0->clusters[cluster];
        }

        // Set the starting directoryAddress of the file to some memory location
        uint8 *startingAddress = (void *)0x30000;

        // Starting at the first sector, each each sector from floppy into memory
        cluster = directoryEntry->startingCluster;
        uint16 sectorCount = 0;

        // Loop through every link in the FAT
        while(cluster != 0xFFFF)
        {
            // Convert the cluster to a sector
            uint32 sector = cluster + 31;
            
            // Read the sector from the floppy disk
            floppy_read(0, sector, (void *) startingAddress + (512 * sectorCount), 512);
            sectorCount++;

            // Get the next cluster
            cluster = fat0->clusters[cluster];

            // It is possible to get stuck in an infinite loop, reading FAT entries forever
            // We prevent that here by checking if the amount of sectors could actually fit on disk
            if(sectorCount > 2880)
            {
                printf("Error: The file appears to be bigger than the entire floppy disk!\n");
                return -2;
            }
        }

        // If no error has occured, label the file as opened and point it to all the data we just read in
        currentFile.directoryEntry = directoryEntry;
        currentFile.startingAddress = startingAddress;
        currentFile.index = 0;
        currentFile.isOpened = 1;
        return 0;
    }

    // If we did not find the file return -3
	return -3;
}