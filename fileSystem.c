/*

 Filename       : fileSystem.c
 Team members   : Suvansh Kumar, Shivam Gupta
 UTD_ID         : 2021426838, 2021492803
 NetID          : SXK170058, SXG190040 
 Class          : CS 5348.001
 Project        : Project 3

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

/*
*
* Initializing the static values like the block size (in bytes), free array size and inode size
*
*/

#define BLOCK_SIZE 1024
#define MAX_FILE_SIZE 4194304 // 4GB of file size
#define FREE_ARRAY_SIZE 248 // free and inode array size
#define INODE_SIZE 64


/*************** superBlock block structure**********************/
typedef struct {
    unsigned int isize; // 4 byte
    unsigned int fsize;
    unsigned int nfree;
    unsigned short free[FREE_ARRAY_SIZE];
    unsigned int ninode;
    unsigned short inode[FREE_ARRAY_SIZE];
    unsigned short flock;
    unsigned short ilock;
    unsigned short fmod;
    unsigned int time[2];
} superBlockType;

/****************inode structure ************************/
typedef struct {
    unsigned short flags; // 2 bytes
    char nlinks;  // 1 byte
    char uid;
    char gid;
    unsigned int size; // 32bits  2^32 = 4GB filesize
    unsigned short addr[22]; // to make total size = 64 byte inode size
    unsigned int actime;
    unsigned int modtime;
} Inode;

/********** Structure of the directory entry ************/
typedef struct {
    unsigned short inode;
    char fileName[14];
} directoryEntry;

// Initializing the global variables
superBlockType superBlock;
int fileDescriptor, currentINodeNumber, totalINodesCount;
char currentWorkingDirectory[100];
char fileSystemPath[100];

/* 
    Writes the data in the buffer data structure to the block 
    in the file pointed by the fileDescriptor
*/
void writeBufferToBlock (int blockNumber, void * buffer, int numberOfBytes) {
    lseek(fileDescriptor, BLOCK_SIZE * blockNumber, SEEK_SET);
    write(fileDescriptor, buffer, numberOfBytes);
}

/* 
    Writes the data from buffer to the block in the file, 
    from the offset position and not from the beginning of the block
*/
void writeToBlockWithOffset (int blockNumber, int offset, void * buffer, int numberOfBytes) {
    lseek(fileDescriptor,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);
    write(fileDescriptor, buffer, numberOfBytes);
}

/* 
    Reads data from the block in the file to the buffer data structure, from the offset position
    and not from the beginning of the block
*/
void readFromBlockWithOffset(int blockNumber, int offset, void * buffer, int numberOfBytes) {
    lseek(fileDescriptor,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);
    read(fileDescriptor, buffer, numberOfBytes);
}

/*
    Adds the newly deallocated blocks to the free list. Checks if the nfree variable is equal to FREE_ARRAY_SIZE.
    If so, copies the array to a new block and sets the nfree variable to zero.
    In any case, add the block to the free list, and increment the nfree variable. 
*/
void addAFreeBlock(int blockNumber) {
    if(superBlock.nfree == FREE_ARRAY_SIZE) {
        // write to the new block
        writeBufferToBlock(blockNumber, superBlock.free, FREE_ARRAY_SIZE * 2);
        superBlock.nfree = 0;
    }
    superBlock.free[superBlock.nfree] = blockNumber;
    superBlock.nfree++;
}

/*
    Gets a free block from the free array and returns the block number. If the nfree variable reaches zero,
    copies the block numbers from the free[0] block into the free array, and sets the nfree variable to 
    FREE_ARRAY_SIZE
*/
int getAFreeBlock() {
    if(superBlock.nfree == 0) {
        int blockNumber = superBlock.free[0];
        lseek(fileDescriptor, BLOCK_SIZE * blockNumber , SEEK_SET);
        read(fileDescriptor, superBlock.free, FREE_ARRAY_SIZE * 2);
        superBlock.nfree = FREE_ARRAY_SIZE;
        return blockNumber;
    }
    superBlock.nfree--;
    return superBlock.free[superBlock.nfree];
}

/*
    Checks if the ninode variable is equal to FREE_ARRAY_SIZE. If not, add the inode to the free inode list
    and increment ninode variable
*/
void addAFreeInode(int iNumber) {
    if(superBlock.ninode == FREE_ARRAY_SIZE)
        return; 
    superBlock.inode[superBlock.ninode] = iNumber;
    superBlock.ninode++;
}

/*
    Returns the inode data for the given i-number
*/
Inode getAnInode(int iNumber) {
    Inode iNode;
    int blockNumber = (iNumber * INODE_SIZE) / BLOCK_SIZE;
    int offset = (iNumber * INODE_SIZE) % BLOCK_SIZE;
    lseek(fileDescriptor,(BLOCK_SIZE * blockNumber) + offset, SEEK_SET);
    read(fileDescriptor,&iNode,INODE_SIZE);
    return iNode;
}

/*
    Gets a free inode from the free-inode array and returns. Checks if the ninode variable is 
    less than or equal to zero. If so, iterates the i-list and adds the free inodes to the 
    free-inode list, and returns the last added inode, and decrements ninode variable
*/
int getAFreeInode(){
        if (superBlock.ninode <= 0) {
            int i;
            for (i = 2; i<totalINodesCount; i++) {
                Inode freeInode = getAnInode(superBlock.inode[i]);
                if (freeInode.flags != 1<<15) {
                    superBlock.inode[superBlock.ninode] = i;
                    superBlock.ninode++;
                }
            }
        }
        superBlock.ninode--;
        return superBlock.inode[superBlock.ninode];
}

/*
    Writes the data of the inode structure into the inode in the filesystem, specified
    by the i-number
*/
void writeTheInode(int iNumber, Inode inode) {
    int blockNumber = (iNumber * INODE_SIZE )/ BLOCK_SIZE;
    int offset = (iNumber * INODE_SIZE) % BLOCK_SIZE;
    writeToBlockWithOffset(blockNumber, offset, &inode, sizeof(Inode));
}

/*
    Creates the root directory structure and initializes the variables to default values, when
    the filesystem is initialized
*/
void initializeRootDirectory() {
    int blockNumber = getAFreeBlock();
    directoryEntry directory[2];
    directory[0].inode = 0;
    strcpy(directory[0].fileName,".");

    directory[1].inode = 0;
    strcpy(directory[1].fileName,"..");

    writeBufferToBlock(blockNumber, directory, 2*sizeof(directoryEntry));

    Inode root;
    root.flags = 1<<14 | 1<<15; // setting 14th and 15th bit to 1, 15: allocated and 14: directory
    root.nlinks = 1;
    root.uid = 0;
    root.gid = 0;
    root.size = 2*sizeof(directoryEntry);
    root.addr[0] = blockNumber;
    root.actime = time(NULL);
    root.modtime = time(NULL);

    writeTheInode(0,root);
    currentINodeNumber = 0;
    strcpy(currentWorkingDirectory,"/");
}

/*
    Lists the contents of the current directory, by reading the i-node that represents 
    the current directory
*/
void ls() {                                                              
    // list directory contents
    Inode currentINode = getAnInode(currentINodeNumber);
    int blockNumber = currentINode.addr[0];
    directoryEntry directory[100];
    int i;
    readFromBlockWithOffset(blockNumber, 0, directory, currentINode.size);
    for(i = 0; i < currentINode.size/sizeof(directoryEntry); i++) {
        printf("%s\n",directory[i].fileName);
    }
}

/*
    Creates a new directory inside the current directory
    Sets the block and inode of the directory
    Writes the data blocks, sets the flags of the directory, writes the block number of the data
    block into the inode, sets the current and parent directory values as the first two entries,
    and writes the inode numbers into the memory address
*/
void makeDirectory (char* dirName) {
    int blockNumber = getAFreeBlock(); // block to store directory table
    int iNumber = getAFreeInode(); // inode numbr for directory
    directoryEntry directory[2];
    directory[0].inode = iNumber;
    strcpy(directory[0].fileName,".");
    printf("%s",directory[0].fileName);

    directory[1].inode = currentINodeNumber;
    strcpy(directory[1].fileName,"..");
    printf("%s",directory[1].fileName);

    writeBufferToBlock(blockNumber, directory, 2*sizeof(directoryEntry));
    // write directory i node
    Inode dir;
    dir.flags = 1<<14 | 1<<15; // setting 14th and 15th bit to 1, 15: allocated and 14: directory
    dir.nlinks = 1;
    dir.uid = 0;
    dir.gid = 0;
    dir.size = 2*sizeof(directoryEntry);
    dir.addr[0] = blockNumber;
    dir.actime = time(NULL);
    dir.modtime = time(NULL);

    writeTheInode(iNumber,dir);

    Inode currentINode = getAnInode(currentINodeNumber);
    blockNumber = currentINode.addr[0];
    directoryEntry newDir;
    newDir.inode = iNumber ;
    strcpy(newDir.fileName,dirName);
    int i;
    writeToBlockWithOffset(blockNumber,currentINode.size,&newDir,sizeof(directoryEntry));
    currentINode.size += sizeof(directoryEntry);
    writeTheInode(currentINodeNumber,currentINode);
}

/*
    Local utility function.
    Finds and returns the index of the last occurence of the character in a string.
    Returns -1 if the character is not found
*/
int findLastIndex(char str[100], char ch) {
    int i, position=-1;
    for (i=0; i<100; i++) {
        if (str[i]==ch)
            position = i;
    }
    return position;
}

/*
    Local utility function used for slicing of a string
*/
void sliceString(const char * str, char * buffer, size_t startPosition, size_t endPosition) {
    size_t j = 0;
    size_t i;
    for (i=startPosition; i<=endPosition; ++i) {
        buffer[j++] = str[i];
    }
    buffer[j] = 0;
}

/*
    Changes the current working directory of the filesystem
    cd .. -> moves to the parent directory
    cd . -> stays in the current directory
    cd directoryName -> checks if the directory named 'directoryName' exists, 
    if so, moves into that directory
*/
void changeDirectory(char* directoryName) {
    Inode currentINode = getAnInode(currentINodeNumber);
    int blockNumber = currentINode.addr[0];
    directoryEntry directory[100];
    int i;
    readFromBlockWithOffset(blockNumber,0,directory,currentINode.size);
    for(i = 0; i < currentINode.size/sizeof(directoryEntry); i++) {
        if(strcmp(directoryName,directory[i].fileName)==0) {
            Inode dir = getAnInode(directory[i].inode);
            if(dir.flags ==( 1<<14 | 1<<15)) {
                if (strcmp(directoryName, ".") == 0) {
                        return;
                }
                else if (strcmp(directoryName, "..") == 0) {
                    currentINodeNumber=directory[i].inode;
                    int lastSlashPosition = findLastIndex(currentWorkingDirectory, '/');
                    char temp[100];
                    sliceString(currentWorkingDirectory, temp, 0, lastSlashPosition-1);
                    strcpy(currentWorkingDirectory, temp);
                }
                else {
                    currentINodeNumber=directory[i].inode;
                    strcat(currentWorkingDirectory, "/");
                    strcat(currentWorkingDirectory, directoryName);
                } 
            }
            else {
                printf("\n%s\n","NOT A DIRECTORY!");
            }
            return;
        }
    }
}

/*
    Copies a file (named fileName) from external filesystem into the current filesystem
*/
void copyIn(char* sourceFilePath, char* fileName) {
    int source,blockNumber;
    if((source = open(sourceFilePath,O_RDWR|O_CREAT,0600))== -1) {
        printf("\n Error while opening the file [%s]\n",strerror(errno));
        return;
    }

    int iNumber = getAFreeInode();
    Inode inodeForFile;
    inodeForFile.flags = 1<<15; // setting 15th bit to 1, 15: allocated
    inodeForFile.nlinks = 1;
    inodeForFile.uid = 0;
    inodeForFile.gid = 0;
    inodeForFile.size = 0;
    inodeForFile.actime = time(NULL);
    inodeForFile.modtime = time(NULL);

    int bytesRead = BLOCK_SIZE;
    char buffer[BLOCK_SIZE] = {0};
    int i = 0;
    while(bytesRead == BLOCK_SIZE) {
        bytesRead = read(source,buffer,BLOCK_SIZE);
        inodeForFile.size += bytesRead;
        blockNumber = getAFreeBlock();
        inodeForFile.addr[i++] = blockNumber;
        writeBufferToBlock(blockNumber, buffer, bytesRead);
    }
    writeTheInode(iNumber,inodeForFile);

    Inode currentINode = getAnInode(currentINodeNumber);
    blockNumber = currentINode.addr[0];
    directoryEntry newFile;
    newFile.inode = iNumber ;
    strcpy(newFile.fileName,fileName);
    writeToBlockWithOffset(blockNumber,currentINode.size,&newFile,sizeof(directoryEntry));
    currentINode.size += sizeof(directoryEntry);
    writeTheInode(currentINodeNumber,currentINode);
}

/*
    Copies the file (named fileName) from current filesystem to the external filesystem
*/
void copyOut(char* destinationFilePath, char* fileName) {
    int dest,blockNumber,x,i;
    char buffer[BLOCK_SIZE] = {0};
    if((dest = open(destinationFilePath,O_RDWR|O_CREAT,0600))== -1) {
        printf("\nError while opening the file [%s]\n",strerror(errno));
        return;
    }

    Inode currentINode = getAnInode(currentINodeNumber);
    blockNumber = currentINode.addr[0];
    directoryEntry directory[100];
    readFromBlockWithOffset(blockNumber,0,directory,currentINode.size);
    for(i = 0; i < currentINode.size/sizeof(directoryEntry); i++) {
        if(strcmp(fileName,directory[i].fileName)==0){
            Inode file = getAnInode(directory[i].inode);
            unsigned short* source = file.addr;
            if(file.flags ==(1<<15)) {
                for(x = 0; x<file.size/BLOCK_SIZE; x++) {
                    blockNumber = source[x];
                    readFromBlockWithOffset(blockNumber, 0, buffer, BLOCK_SIZE);
                    write(dest,buffer,BLOCK_SIZE);
                }
                blockNumber = source[x];
                readFromBlockWithOffset(blockNumber, 0, buffer, file.size%BLOCK_SIZE);
                write(dest,buffer,file.size%BLOCK_SIZE);
            }
            else {
                printf("\n%s\n","NOT A FILE!");
            }
            return;
        }
    }
}

/*
    Removes the file name 'fileName' from the filesystem if it exists
*/
void removeFile(char* fileName) {
    int blockNumber,x,i;
    Inode currentINode = getAnInode(currentINodeNumber);
    blockNumber = currentINode.addr[0];
    directoryEntry directory[100];
    readFromBlockWithOffset(blockNumber,0,directory,currentINode.size);

    for(i = 0; i < currentINode.size/sizeof(directoryEntry); i++) {
        if(strcmp(fileName,directory[i].fileName)==0) {
            Inode file = getAnInode(directory[i].inode);
            if(file.flags ==(1<<15)) {
                for(x = 0; x<file.size/BLOCK_SIZE; x++) {
                    blockNumber = file.addr[x];
                    addAFreeBlock(blockNumber);
                }
                if(0<file.size%BLOCK_SIZE) {
                    blockNumber = file.addr[x];
                    addAFreeBlock(blockNumber);
                }
                addAFreeInode(directory[i].inode);
                directory[i]=directory[(currentINode.size/sizeof(directoryEntry))-1];
                currentINode.size-=sizeof(directoryEntry);
                writeBufferToBlock(currentINode.addr[0],directory,currentINode.size);
                writeTheInode(currentINodeNumber,currentINode);
            }
            else {
                printf("\n%s\n","NOT A FILE!");
            }
            return;
        }
    }
}

/*
    Removes the specified directory from the filesystem
*/
void removeDirectory(char* fileName) {
    int blockNumber,x,i;
    Inode currentINode = getAnInode(currentINodeNumber);
    blockNumber = currentINode.addr[0];
    directoryEntry directory[100];
    readFromBlockWithOffset(blockNumber, 0, directory, currentINode.size);

    for(i = 0; i < currentINode.size/sizeof(directoryEntry); i++) {
        if(strcmp(fileName,directory[i].fileName)==0){
            Inode file = getAnInode(directory[i].inode);
            if (file.flags ==( 1<<14 | 1<<15)) {
                for( x = 0; x<file.size/BLOCK_SIZE; x++) {
                    blockNumber = file.addr[x];
                    addAFreeBlock(blockNumber);
                }
                if(0<file.size%BLOCK_SIZE) {
                    blockNumber = file.addr[x];
                    addAFreeBlock(blockNumber);
                }
                addAFreeInode(directory[i].inode);
                directory[i]=directory[(currentINode.size/sizeof(directoryEntry))-1];
                currentINode.size-=sizeof(directoryEntry);
                writeBufferToBlock(currentINode.addr[0],directory,currentINode.size);
                writeTheInode(currentINodeNumber,currentINode);
            }
            else{
                printf("\n%s\n","NOT A DIRECTORY!");
            }
            return;
        }
    }
}

/*
    Loads an external filesystem into the current filesystem, and copies the contents (inodes, datablocks)
    into the loaded filesystem
*/
int openFileSystem(const char *fileName) {
	fileDescriptor = open(fileName,2);
	lseek(fileDescriptor, BLOCK_SIZE, SEEK_SET);
	read(fileDescriptor, &superBlock, sizeof(superBlock));
	lseek(fileDescriptor, 2*BLOCK_SIZE, SEEK_SET);
    Inode rootINode = getAnInode(1);
	read(fileDescriptor, &rootINode, sizeof(rootINode));
	return 1;
}

/*
    Initializes the filesystem, sets the total number of blocks and total number of inodes that are taken as
    inputs from the user, and initializes other parameters to default values
    Also, initializes the super block and inode for the root directory
*/
void initfs(char* filePath, int totalNumberOfBlocks, int totalNumberOfINodes) {
    printf("\nFilesystem is now initializing \n");
    totalINodesCount = totalNumberOfINodes;
    char emptyBlock[BLOCK_SIZE] = {0};
    int no_of_bytes,i,blockNumber,iNumber;

    //init isize (Number of blocks for inode
    if(((totalNumberOfINodes*INODE_SIZE)%BLOCK_SIZE) == 0) // 300*64 % 1024
            superBlock.isize = (totalNumberOfINodes*INODE_SIZE)/BLOCK_SIZE;
    else
            superBlock.isize = (totalNumberOfINodes*INODE_SIZE)/BLOCK_SIZE+1;

    //init fsize
    superBlock.fsize = totalNumberOfBlocks;

    //create file for File System
    if((fileDescriptor = open(filePath,O_RDWR|O_CREAT,0600))== -1) {
            printf("\n file opening error [%s]\n",strerror(errno));
            return;
    }
    strcpy(fileSystemPath,filePath);

    writeBufferToBlock(totalNumberOfBlocks-1,emptyBlock,BLOCK_SIZE); // writing empty block to last block

    // add all blocks to the free array
    superBlock.nfree = 0;
    for (blockNumber= 1+superBlock.isize; blockNumber< totalNumberOfBlocks; blockNumber++)
            addAFreeBlock(blockNumber);

    // add free Inodes to inode array
    superBlock.ninode = 0;
    for (iNumber=1; iNumber < totalNumberOfINodes ; iNumber++)
            addAFreeInode(iNumber);


    superBlock.flock = 'f';
    superBlock.ilock = 'i';
    superBlock.fmod = 'f';
    superBlock.time[0] = 0;
    superBlock.time[1] = 0;

    //write superBlock Block
    writeBufferToBlock (0, &superBlock, BLOCK_SIZE);

    //allocate empty space for i-nodes
    for (i=1; i <= superBlock.isize; i++)
            writeBufferToBlock(i, emptyBlock, BLOCK_SIZE);

    initializeRootDirectory();
}

/*
    Saves the filesystem and quits the program
*/
void quit() {
    close(fileDescriptor);
    exit(0);
}


int main(int argc, char *argv[]) {
    char c;

    printf("\n Clearing screen \n");
    system("clear");

    unsigned int blk_no =0, inode_no=0;
    char *fs_path;
    char *arg1, *arg2;
    char *my_argv, cmd[512];

    while(1) {
        printf("\n%s@%s>>>",fileSystemPath,currentWorkingDirectory);
        scanf(" %[^\n]s", cmd);
        my_argv = strtok(cmd," ");

        if(strcmp(my_argv, "initfs")==0) {
            fs_path = strtok(NULL, " ");
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            if(access(fs_path, X_OK) != -1) {
                printf("filesystem already exists. \n");
                printf("same file system will be used\n");
            }
            else {
                if (!arg1 || !arg2)
                    printf("Insufficient arguments, cannot proceed\n");
                else {
                    blk_no = atoi(arg1);
                    inode_no = atoi(arg2);
                    // Initialize the filesystem
                    initfs(fs_path,blk_no, inode_no);
                }
            }
            my_argv = NULL;
        }
        // Call the respective functions for the commands
        else if(strcmp(my_argv, "q")==0){
            quit();
        }
        else if(strcmp(my_argv, "ls")==0){
            ls();
        }
        else if(strcmp(my_argv, "mkdir")==0){
            arg1 = strtok(NULL, " ");
            makeDirectory(arg1);
        }
        else if(strcmp(my_argv, "cd")==0){
            arg1 = strtok(NULL, " ");
            changeDirectory(arg1);
        }
        else if(strcmp(my_argv, "cpin")==0){
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            copyIn(arg1,arg2);
        }
        else if(strcmp(my_argv, "cpout")==0){
            arg1 = strtok(NULL, " ");
            arg2 = strtok(NULL, " ");
            copyOut(arg1,arg2);
        }
        else if(strcmp(my_argv, "rm")==0){
            arg1 = strtok(NULL, " ");
            removeFile(arg1);
        }
        else if(strcmp(my_argv, "remdir")==0){
            arg1 = strtok(NULL, " ");
            removeDirectory(arg1);
        }else if(strcmp(my_argv, "openfs")==0){
            arg1 = strtok(NULL, " ");
            openFileSystem(arg1);
        }
        else if(strcmp(my_argv, "currentWorkingDirectory")==0){
            printf("%s\n",currentWorkingDirectory);
        }
    }
}
