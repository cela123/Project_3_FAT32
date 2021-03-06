#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "parser.h"


#define FILE_ATTRIBUTE 0X20
#define DIRECTORY_ATTRIBUTE 0X10
#define READ 0x30
#define WRITE 0x40
#define READWRITE 0x50
#define WRITEREAD 0x60

//Structs
typedef struct bpb_info_struct
{
  unsigned short bpb_bytspersec, bpb_secperclus,
                bpb_rsvdseccnt, bpb_numfats;
  unsigned int bpb_fatsz32, bpb_rootclus, bpb_totsec32;
}Bpb_info_struct;

typedef struct { // DIRECTORY ENTRY
    unsigned char longnameListing[32];  //skip over the 32 bytes of the long listing
    unsigned char DIR_Name[11];         //offset byte 0
    unsigned char DIR_Attributes;       //offset byte 11
    unsigned short DIR_NTRes;           //offset byte 12, do not care
    unsigned short DIR_CrtTimeTenth;    //offset byte 13, do not care
    unsigned short DIR_CrtTime;         //offset byte 14, do not care
    unsigned short DIR_CrtDate;         //offset byte 16, do not care
    unsigned short DIR_LastAccDate;     //offset byte 18, do not care
    unsigned short DIR_FstClusHI;       //offset byte 20
    unsigned short DIR_WrtTime;         //offset byte 22, do not care
    unsigned short DIR_WrtDate;         //offset byte 24, do not care
    unsigned short DIR_FstClusLo;       //offset byte 26
    unsigned int DIR_FileSize;          //offset byte 28
} __attribute__((packed)) DIR_ENTRY;

Bpb_info_struct bpb_information;

// Linked List for storing opened files
struct openFiles{
	int firClust;
	short Mode;
	int Offset;
	char name[11];
	struct openFiles *next;
}; 

struct openFiles *head = NULL;
struct openFiles *current = NULL;

//Function Declarations
void gather_info(int fd);
void print_info();
void addFile(int fc, int M, int off, char n[11]); 
struct node* deleteOpenFile(char name[11]); 
struct openFiles* findOpenFile(char name[11]); 
void printOpenFiles(); 
int isCommand(char *); 
int find_dir_entry(int fd, char* dirName, int curDir); 
int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int curCluster); 
int file_cluster_num(int fd, char fileName[11], int curDir, int firstDataLoc, int curCluster); 
int next_cluster_num(int fd, unsigned int currentClusterNum); 
int last_cluster_num(int fd, int currentClusterNum); 
int file_size(int fd, char* fileName, int curDir); 
unsigned int find_empty_cluster(int fd, int dataRegStart); 
void create_dir_entry(int type); 
int data_region_loc(int clusterNum); 
void read_file(int fd, char name[11], int readSize, int); 
void write_to_file(int fd, char fileName[11], int sizeOfWrite, char* stringFromInput, int curDir, int fileDataClusterNum); 
int create(char* name, int dirCheck, int fd, int dataRegStart, int currDirectoryCluster);

//MAIN STARTS
int main(int argc, char *argv[]){
    off_t temp; 
    ssize_t temp2;

    int dataRegStart; 
    int test; 
    int i, j; 
    char fileName[11], tempstr[11];  
    int empty = 0; 
    unsigned int directory; 
    unsigned int clusterNumber = 0; 
    int clusterBytes = 32; 
    int counter = 0, offset = 0; 

    //-------CURRENT DIRECTORY DATA-------
    int currDirectory;
    int currDirectoryCluster; 
    //------------------------------------

    int fd = open(argv[1], O_RDWR);
    if (fd == -1)
        printf("Error opening file");

    gather_info(fd); 

    dataRegStart = bpb_information.bpb_bytspersec*(bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats*bpb_information.bpb_fatsz32)); 
    //set current directory to the beginning of data region intially (ie start in root)
    currDirectory = dataRegStart; 
    currDirectoryCluster = bpb_information.bpb_rootclus; 
    //printf("Data Region Start = %d\n", dataRegStart); 
    //printf("Root Cluster = %d\n", currDirectoryCluster); 
    while(1){
        
        printf("$ ");
        char* input = get_input();
		tokenlist *inputTokens = get_tokens(input);

        if(strcmp(inputTokens->items[0], "exit") == 0){
            free(input);
            break; 
        }
        if(strcmp(inputTokens->items[0], "info") == 0){

            print_info(); 
        }
        if(strcmp(inputTokens->items[0], "size") == 0){

            if(inputTokens->items[1] == NULL){
                printf("No file specified for size\n"); 
                continue; 
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20){
                //implementation
                printf("%s: %d\n", inputTokens->items[1] , file_size(fd, inputTokens->items[1], currDirectory)); 
            }
            else{
                printf("%s is not a file\n", inputTokens->items[1]); 
            }
        }
        if(strcmp(inputTokens->items[0], "ls") == 0){
            memset(fileName,' ',sizeof(tempstr));
            memset(tempstr,' ',sizeof(tempstr));
            clusterNumber = 0; 
            clusterBytes = 32; 
            counter = 0; 
            j = 0;

            if(inputTokens->items[1]!= NULL){
                //determine start of directory specified 
                if((dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster)) != -1){
                        directory = (dataRegStart + 512*(( (dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster)) -2)*bpb_information.bpb_secperclus));
                        clusterNumber = dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster); 
                        printf(". "); 
                        counter++; 

                }
                else
                {
                    printf("%s is not a directory\n", inputTokens->items[1]); 
                    continue; 
                }
            }
            else{
                directory = currDirectory; 
                clusterNumber = currDirectoryCluster; 
            }       
            temp = lseek(fd, directory, SEEK_SET);
            temp2 = read(fd, &empty, 4); 

            //reading "." for directories not the root
            if((currDirectory != dataRegStart) && empty != 0){
                temp = lseek(fd, directory, SEEK_SET);
                for(i=0; i<11; i++){
                    temp2 = read(fd, &tempstr[i], 1);            
                }
                // Removing trailing blank spaces
                for(i = 0; i < 11; i++)
                {
                    if (!(tempstr[i] == ' ' && tempstr[i+1] == ' '))
                    {
                        fileName[j] = tempstr[i];
                        j++;
                    }
                }
                fileName[j-1] = '\0'; // one trailing blank kept showing up
                j = 0;
                printf("%s ", fileName); 
                counter++; 
                temp = lseek(fd, directory+32, SEEK_SET);
                temp2 = read(fd, &empty, 4);
            }
                
            while(empty != 0){  
                empty = 0; 
                temp = lseek(fd, directory+clusterBytes, SEEK_SET);
                
                for(i=0; i<11; i++){
                    temp2 = read(fd, &tempstr[i], 1);            
                }
                // Removing trailing blank spaces
                for(i = 0; i < 11; i++)
                {
                    if (!(tempstr[i] == ' ' && tempstr[i+1] == ' '))
                    {
                        fileName[j] = tempstr[i];
                        j++;
                    }
                }
                fileName[j-1] = '\0'; // one trailing blank kept showing up
                j = 0;
                printf("%s ", fileName); 

                temp = lseek(fd, 21, SEEK_CUR);
                temp2 = read(fd, &empty, 4); 

                clusterBytes+=64; 

                if(clusterBytes >= 512){
                    if(next_cluster_num(fd, clusterNumber) != -1){
                        clusterNumber = next_cluster_num(fd, clusterNumber); 
                        clusterBytes = 32; 

                        directory = dataRegStart + 512*(clusterNumber-2)*bpb_information.bpb_secperclus;

                        temp = lseek(fd, directory, SEEK_SET);
                        temp2 = read(fd, &empty, 4); 
                    }
                    else
                    {
                        empty = 0; 
                    }   
                }
                counter++; 
                if(counter == 12){
                    printf("\n"); 
                    counter = 0; 
                }
            }
            printf("\n"); 
            
        }
        if(strcmp(inputTokens->items[0], "cd") == 0){
            int newDirectoryCluster = 0; 
            if(inputTokens->items[1] == NULL){
                printf("No directory specified for cd\n");
            }
            else if(strcmp(inputTokens->items[1], "..") == 0 && currDirectory == dataRegStart){
                printf("No directory '..' for root\n");
            }
            else{

                newDirectoryCluster = dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster);
                if(newDirectoryCluster == -1)
                    printf("%s is not a directory\n", inputTokens->items[1]); 
                else{

                    if((newDirectoryCluster == 0) && (currDirectory != dataRegStart)){
                        currDirectoryCluster = bpb_information.bpb_rootclus; 
                        currDirectory = dataRegStart;
                    }
                    else{
                        currDirectoryCluster = newDirectoryCluster;
                        currDirectory = dataRegStart + 512*(newDirectoryCluster-2)*bpb_information.bpb_secperclus;
                    }
                }
            }
        }
        if(strcmp(inputTokens->items[0], "creat") == 0){
            int newFileCluster = 0; 
            
            if(inputTokens->items[1] == NULL){
                printf("Missing operand for creat\n"); 
                continue; 
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20){
                printf("%s is already a file\n", inputTokens->items[1]);
                continue; 
            }
            else{
                create(inputTokens->items[1], 1, fd, dataRegStart, currDirectoryCluster);
            }
        }
        if(strcmp(inputTokens->items[0], "mkdir") == 0){
            if(inputTokens->items[1] == NULL){
                printf("Missing operand for mkdir\n"); 
                continue; 
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10){
                printf("%s is already a directory\n", inputTokens->items[1]);
                continue; 
            }
            else{
                create(inputTokens->items[1], 0, fd, dataRegStart, currDirectoryCluster);
            }
        }
        if(strcmp(inputTokens->items[0], "mv") == 0){   //mv FROM TO, [1] = FROM ,[2] = TO

            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n"); 
                continue; 
            }
            if(inputTokens->items[2] == NULL){
                printf("Missing destination file operand\n"); 
                continue; 
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20 && find_dir_entry(fd, inputTokens->items[2], currDirectory) == 0x20){
                printf("The name '%s' is already being used by another file\n", inputTokens->items[2]); 
                continue;                 
            }
            if(find_dir_entry(fd, inputTokens->items[2], currDirectory) == 0x20 && find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10){
                printf("Cannot move directory: invalid destination argument\n"); 
                continue; 
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == -1)
            {
                printf("The File '%s' cannot be found\n", inputTokens->items[1]);
            }
            if(find_dir_entry(fd, inputTokens->items[2], currDirectory) == -1 && find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20){
                memset(fileName,' ',sizeof(tempstr));
                memset(tempstr,' ',sizeof(tempstr));
                unsigned short space[1] = {0x20};
                j = 0;
                clusterBytes = 32;
                empty = 0;
                directory = currDirectory;
                temp = lseek(fd, directory, SEEK_SET);
                temp2 = read(fd, &empty, 4);
                while(empty != 0)
                {
                    empty = 0;
                    temp = lseek(fd, directory+clusterBytes, SEEK_SET);

                    for(i=0; i<11; i++)
                    {
                        temp2 = read(fd, &tempstr[i], 1);
                    }
                    // Removing trailing blank spaces
                    for(i = 0; i < 11; i++)
                    {
                        if (!(tempstr[i] == ' ' && tempstr[i+1] == ' '))
                        {
                            fileName[j] = tempstr[i];
                            //tempstr[i] = inputTokens->items[2][i]; // copying replacement name into tempStr
                            j++;
                        }
                    }
                    fileName[j-1] = '\0'; // one trailing blank kept showing up
                    j = 0;
                    if (strcmp(fileName, inputTokens->items[1]) == 0)
                    {
                        temp = lseek(fd, directory+clusterBytes, SEEK_SET);
                        // Clearing name to be changed
                        for(i=0; i<11; i++)
                        {
                            temp2 = write(fd, space, 1);
                        }
                        temp = lseek(fd, directory+clusterBytes, SEEK_SET);
                        // Writing name
                        temp2 = write(fd, inputTokens->items[2], strlen(inputTokens->items[2]));
                        break;
                    }

                    temp = lseek(fd, 21, SEEK_CUR);
                    temp2 = read(fd, &empty, 4);
                    clusterBytes += 64;
                } // End of While
            }
            if(find_dir_entry(fd, inputTokens->items[2], currDirectory) == 0x10){
                printf("moving %s to directory %s is not an implemented command\n", inputTokens->items[1], inputTokens->items[2]); 
                //if TO exists and it a directory, move FROM inside TO
            }
        }
        if(strcmp(inputTokens->items[0], "open") == 0){
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n"); 
                continue; 
            }
            if(inputTokens->items[2] == NULL){
                printf("Missing mode operand\n"); 
                continue;   
            }
            //error if name DNE or is for a directory not a file
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == -1){
                printf("File %s does not exist\n", inputTokens->items[1]); 
                continue;
            }   
            if(findOpenFile(inputTokens->items[1]) != NULL){
                printf("File %s is already open\n",inputTokens->items[1]); 
                continue; 
            } 
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20 && inputTokens->items[2] != NULL){
                if(strcmp(inputTokens->items[2], "r") == 0){
                    //printf("Open %s read only\n", inputTokens->items[1]); 
                    addFile(0, READ, 0, inputTokens->items[1]);
                }
                else if(strcmp(inputTokens->items[2], "w") == 0){
                    //printf("Open %s write only\n", inputTokens->items[1]); 
                    addFile(0, WRITE, 0, inputTokens->items[1]);
                }
                else if(strcmp(inputTokens->items[2], "rw") == 0){
                    //printf("Open %s read and write\n", inputTokens->items[1]); 
                    addFile(0, READWRITE, 0, inputTokens->items[1]);
                }   
                else if(strcmp(inputTokens->items[2], "wr") == 0){
                    //printf("Open %s write and read\n", inputTokens->items[1]); 
                    addFile(0, WRITEREAD, 0, inputTokens->items[1]);
                }   
                else{
                    printf("Invalid mode for open: %s\n", inputTokens->items[2]); 
                    continue; 
                }
                //printf("Currently open files: ");
                //printOpenFiles();                          
            }     
        }
        if(strcmp(inputTokens->items[0], "close") == 0){    //close FILENAME
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n"); 
                continue; 
            }
            //error if name DNE or is for a directory not a file
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == -1){
                printf("File %s does not exist\n", inputTokens->items[1]); 
                continue;
            } 

            if(findOpenFile(inputTokens->items[1]) != NULL)
            {
                deleteOpenFile(inputTokens->items[1]);
                
            } else {
                printf("File is not currently open\n");
            }
            //printf("Currently open files: "); 
            //printOpenFiles();       

        }
        if(strcmp(inputTokens->items[0], "lseek") == 0){    //lseek FILENAME OFFSET
            int offset; 
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n"); 
                continue; 
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) != 0x20){
                printf("%s is not a file\n",inputTokens->items[1]); 
                continue;  
            }
            if(inputTokens->items[2] == NULL){
                printf("Missing offset operand\n"); 
                continue;   
            }
            else{
                offset = atoi(inputTokens->items[2]);
            }
            //if OFFSET > FILE SIZE return error
            if(offset > file_size(fd, inputTokens->items[1], currDirectory)){
                printf("Offset value larger than file size\n"); 
                continue; 
            }
            else{

                current = head; 

                if(findOpenFile(inputTokens->items[1]) != NULL){
                    while (current->name != NULL){
                        if(strcmp(current->name, inputTokens->items[1]) == 0)
                            break;
                        else{
                            current = current->next;
                        }
                    } 

                    current->Offset = offset; 
                    //printf("%s, %d\n", current->name, current->Offset);
                }  
                else{
                printf("File %s has not been opened\n", inputTokens->items[1]);
                }
            }
        }
        if(strcmp(inputTokens->items[0], "read") == 0){     //read FILENAME SIZE
            
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n"); 
                continue; 
            }
            if(inputTokens->items[2] == NULL){
                printf("Missing size operand\n"); 
                continue;   
            }
            //error if name DNE or is for a directory not a file
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == -1){
                printf("File %s does not exist\n", inputTokens->items[1]); 
                continue;
            }  
            if(findOpenFile(inputTokens->items[1]) == NULL){
                printf("File %s is not open\n", inputTokens->items[1]); 
                continue; 
            }
            else{
                if(findOpenFile(inputTokens->items[1])->Mode == READ || findOpenFile(inputTokens->items[1])->Mode == READWRITE || findOpenFile(inputTokens->items[1])->Mode == WRITEREAD){
                    //read SIZE bytes at OFFSET
                    int readSize = atoi(inputTokens->items[2]); 
                    int fileClusterNum, fileData; 
                    //if OFFSET + SIZE > file size
                    if(findOpenFile(inputTokens->items[1])->Offset + readSize > file_size(fd, inputTokens->items[1], currDirectory)){
                        //read file size - OFFSET starting at OFFSET
                        //printf("having to change read size\n"); 
                        readSize = file_size(fd, inputTokens->items[1], currDirectory) - findOpenFile(inputTokens->items[1])->Offset; 
                    }
                    
                    fileClusterNum = file_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster);                    
                    fileData = data_region_loc(fileClusterNum);

                    //printf("data for %s starts at %d\n", inputTokens->items[1], fileData);

                    read_file(fd, inputTokens->items[1], readSize, fileClusterNum);  
                }
                else{
                    printf("File %s is not open for reading\n", inputTokens->items[1]); 
                    continue; 
                }
            }

        }
        if(strcmp(inputTokens->items[0], "write") == 0){    //write FILENAME SIZE "STRING"
            int writeSize = 0; 
            int i = 0; 
            char * fullString = NULL; 
            int memAlloc = 0; 
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n"); 
                continue; 
            }
            if(inputTokens->items[2] == NULL){
                printf("Missing size operand\n"); 
                continue;   
            }  
            else{
                writeSize = atoi(inputTokens->items[2]); 
            }     
            if(inputTokens->items[3] == NULL){
                printf("Missing string to write to %s\n", inputTokens->items[1]); 
                continue;   
            } 
            else{
                i=3; 
                while(inputTokens->items[i] != NULL){
                    if(memAlloc == 0){
                        fullString =(char*)malloc(strlen(inputTokens->items[i]) + 1); 
                        sprintf(fullString, "%s", inputTokens->items[i]); 
                        memAlloc = 1; 
                    }
                    else{
                        fullString = (char*)realloc(fullString, strlen(fullString)+ strlen(inputTokens->items[i]) + 3); 
                        sprintf(fullString, "%s %s", fullString, inputTokens->items[i]); 
                    }
                    i++; 
                }
            }         
            //error if name DNE or is for a directory not a file
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == -1){
                printf("File %s does not exist\n", inputTokens->items[1]); 
                continue;
            } 
            if(findOpenFile(inputTokens->items[1]) == NULL){
                printf("File %s is not open\n", inputTokens->items[1]); 
                continue;
            }
            
            if(findOpenFile(inputTokens->items[1])->Offset > file_size(fd, inputTokens->items[1], currDirectory)){
                printf("Offset is larger than file size\n"); 
                continue; 
            }
            if(findOpenFile(inputTokens->items[1])->Mode == READ){
                printf("File %s is not open for writing\n", inputTokens->items[1]); 
                continue; 

            }
            else{
                int fileClusterNum; 
                fileClusterNum = file_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster);
                if(fullString[0] == '"' && fullString[strlen(fullString)-1] == '"')
                    write_to_file(fd, inputTokens->items[1], writeSize, fullString, currDirectory, fileClusterNum); 
                else{
                    printf("Improper syntax used for string to write\n"); 
                    continue; 
                }
            }
            
        }
        if(strcmp(inputTokens->items[0], "rm") == 0){       //rm FILENAME
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n"); 
                continue; 
            }
            //error if name DNE or is for a directory not a file
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == -1){
                printf("File %s does not exist\n", inputTokens->items[1]); 
                continue;
            } 
            //remove:
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20)  {
                //printf("Removing %s\n", inputTokens->items[1]);
                memset(fileName,' ',sizeof(fileName));
                memset(tempstr,' ',sizeof(tempstr));
                j = 0;
                clusterBytes = 32;
                empty = 0;
                unsigned short emptyCluster[4] = {0,0,0,0};
                unsigned short E5[1] = {0xE5};
                clusterNumber = file_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster);
                offset =  bpb_information.bpb_rsvdseccnt*bpb_information.bpb_bytspersec+clusterNumber*4;
                
                temp = lseek(fd, offset, SEEK_SET);
                temp2 = read(fd, &clusterNumber, 4);
                
                // Clearing Clusters in Fat Region
                if(clusterNumber > 0x0FFFFFF8)
                {
                    temp = lseek(fd, offset, SEEK_SET);
                    temp2 = write(fd, emptyCluster, 4);
                }
                else {
                    while(clusterNumber < 0x0FFFFFF8)
                    {
                        temp = lseek(fd, offset, SEEK_SET);
                        temp2 = write(fd, emptyCluster, 4);
                        offset = bpb_information.bpb_rsvdseccnt*bpb_information.bpb_bytspersec+clusterNumber*4;
                        temp = lseek(fd, offset, SEEK_SET);
                        temp2 = read(fd, &clusterNumber, 4);
                    }
                }
                directory = currDirectory;
                temp = lseek(fd, directory, SEEK_SET);
                temp2 = read(fd, &empty, 4);
                while(empty != 0)
                {
                    empty = 0;
                    temp = lseek(fd, directory+clusterBytes, SEEK_SET);

                    for(i=0; i<11; i++)
                    {
                        temp2 = read(fd, &tempstr[i], 1);
                    }
                    // Removing trailing blank spaces
                    for(i = 0; i < 11; i++)
                    {
                        if (!(tempstr[i] == ' ' && tempstr[i+1] == ' '))
                        {
                            fileName[j] = tempstr[i];
                            j++;
                        }
                    }
                    fileName[j-1] = '\0'; // one trailing blank kept showing up
                    j = 0;
                    if (strcmp(fileName, inputTokens->items[1]) == 0)
                    {
                        break;
                    }

                    temp = lseek(fd, 21, SEEK_CUR);
                    temp2 = read(fd, &empty, 4);
                    clusterBytes += 64;
                } // End of While
                temp = lseek(fd, directory+clusterBytes-32, SEEK_SET);
                temp2 = write(fd, E5, 1);
                temp2 = write(fd, emptyCluster, 3); // REMEMBER THIS VALUE WAS 3
                for (i = 0; i < 15; i++)
                {
                    temp2 = write(fd, emptyCluster, 4);   
                }                
            }                      

        }
        if(strcmp(inputTokens->items[0], "cp") == 0){       //cp FILENAME TO
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n");
                continue; 
            }
            else if(strcmp(inputTokens->items[1], "-r") == 0){
                printf("recursive copy is not an implemented command\n"); 
                continue; 
            }
            
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) != 0x20){
                printf("File %s does not exist\n", inputTokens->items[1]); 
                continue;
            } 
            //if destination is not specific or is not a valid destination
            if(inputTokens->items[2] == NULL || find_dir_entry(fd, inputTokens->items[2], currDirectory) != 0x10){
                if(find_dir_entry(fd, inputTokens->items[2], currDirectory) == 0x20){
                    printf("Cannot create copy of %s with name %s. File with name %s already exists.\n", inputTokens->items[1], inputTokens->items[2],inputTokens->items[2]); 
                    continue; 
                }
                //create a copy of the file in current directory with name TO
                printf("Creating copy of %s in current directory with name %s is not an implemented command\n", inputTokens->items[1], inputTokens->items[2]); 
                continue; 
            }

            //if TO is directory, create copy of FILENAME in TO
            if(find_dir_entry(fd, inputTokens->items[2], currDirectory) == 0x10 && find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20){
                printf("Creating copy of %s in %s is not an implemented command\n", inputTokens->items[1], inputTokens->items[2]); 
            }  
        }
        if(strcmp(inputTokens->items[0], "rmdir") == 0){
            if(inputTokens->items[1] == NULL){
                printf("Missing directory operand\n");
                continue; 
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) != 0x10){
                if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20){
                    printf("%s is not a directory\n", inputTokens->items[1]);
                    continue;                   
                }
                else{
                    printf("%s does not exist\n", inputTokens->items[1]);
                    continue;  
                }
            }
            else{
                printf("rmdir is not an implemented command\n"); 
                continue; 
            }
        }
        if(isCommand(inputTokens->items[0]) == -1){
            printf("%s is not a command\n", inputTokens->items[0]); 
        }
    }
    close(fd); 
    return 0; 
}
//MAIN ENDS

/*
    Function: gather_info
    collects bpb information and stores it in the bpb struct
*/
void gather_info(int fd)
{
    bpb_information.bpb_bytspersec = 0; 
    bpb_information.bpb_secperclus = 0; 
    bpb_information.bpb_rsvdseccnt = 0; 
    bpb_information.bpb_numfats = 0; 
    bpb_information.bpb_totsec32 = 0; 
    bpb_information.bpb_fatsz32 = 0; 
    bpb_information.bpb_rootclus = 0; 
    off_t temp_off_t;
    ssize_t temp_ssize_t;
    temp_off_t = lseek(fd, 11, SEEK_CUR);
    temp_ssize_t = read(fd, &bpb_information.bpb_bytspersec, 2);
    temp_ssize_t = read(fd, &bpb_information.bpb_secperclus, 1);
    temp_ssize_t = read(fd, &bpb_information.bpb_rsvdseccnt, 2);
    temp_ssize_t = read(fd, &bpb_information.bpb_numfats, 2);
    temp_off_t = lseek(fd, 14, SEEK_CUR);
    temp_ssize_t = read(fd, &bpb_information.bpb_totsec32, 4);
    temp_ssize_t = read(fd, &bpb_information.bpb_fatsz32, 4);
    temp_off_t = lseek(fd, 4, SEEK_CUR);
    temp_ssize_t = read(fd, &bpb_information.bpb_rootclus, 4);
}
/*
    Function: print_info
    prints the pbp info in the proper format for the "info" command
*/
void print_info(){

    printf("bpb_bytspersec: %d\nbpb_secperclus: %d\nbpb_rsvdseccnt: %d\nbpb_numfats: %d\nbpb_totsec32: %d\nbpb_fatsz32: %d\nbpb_rootclus: %d\n",
    bpb_information.bpb_bytspersec, bpb_information.bpb_secperclus, bpb_information.bpb_rsvdseccnt,
    bpb_information.bpb_numfats, bpb_information.bpb_totsec32, bpb_information.bpb_fatsz32,
    bpb_information.bpb_rootclus);

}

/*
    Function: addFile
    adds node to the end of openFile list
*/
void addFile(int fc, int M, int off, char n[11]){
	struct openFiles *temp = (struct openFiles*)malloc(sizeof(struct openFiles));
	temp->next = NULL;
	temp->firClust = fc;
	temp->Mode = M;
	temp->Offset = off;
	strcpy(temp->name, n);

	if (head == NULL) // empty list
	{
		head = temp;
	}
	else{
		current = head;
		while(current->next != NULL){
			current = current->next;
		}
		current->next = temp;
	}
	// return head;
}

/*
    Function: deleteOpenFile
    removes file from openFile list when file is closed
*/
struct node* deleteOpenFile(char name[11]){
    struct openFiles *current = head;
    struct openFiles *previous = NULL;

    // empty list
    if (head == NULL)
    {
        return NULL;
    }
    while(strcmp(current->name, name) != 0)
    {
        // if last node
        if (current->next == NULL)
            return NULL;
        else {
            previous = current;
            current = current->next;
        }
    }
    // Match found
    if(current == head) {
        head = head->next;
    } else {
        previous->next = current->next;
    }
    return (void *)current;
}

/*
    Function: findOpenFile
    determines if a file with 'name' has been opened
*/
struct openFiles* findOpenFile(char name[11]){
    struct openFiles *current = head;
    if (head == NULL){
        return NULL;
    }

    while(strcmp(current->name, name) != 0)
    {
        if(current->next == NULL)
            return NULL;
        else {
            current = current->next;
        }
    } 
    return (void *)current;
}

/*
    Function: printOpenFiles
    prints the list of open files
*/
void printOpenFiles(){

	struct openFiles *ptr = head;
	while (ptr != NULL){
		printf("%s ", ptr->name);
		ptr = ptr->next;
	}
	printf("\n");
}

/*
    Function: isCommand
    checks if command 'userCmd is a valid command
    returns 0  if valid, returns -1 if not
*/
int isCommand(char * userCmd){
    if(strcmp(userCmd, "exit") == 0)
        return 0; 
    else if(strcmp(userCmd, "info") == 0)
        return 0; 
    else if(strcmp(userCmd, "size") == 0)
        return 0;         
    else if(strcmp(userCmd, "ls") == 0)
        return 0; 
    else if(strcmp(userCmd, "cd") == 0)
        return 0; 
    else if(strcmp(userCmd, "creat") == 0)
        return 0; 
    else if(strcmp(userCmd, "mkdir") == 0)
        return 0; 
    else if(strcmp(userCmd, "mv") == 0)
        return 0; 
    else if(strcmp(userCmd, "open") == 0)
        return 0; 
    else if(strcmp(userCmd, "close") == 0)
        return 0; 
    else if(strcmp(userCmd, "lseek") == 0)
        return 0; 
    else if(strcmp(userCmd, "read") == 0)
        return 0; 
    else if(strcmp(userCmd, "write") == 0)
        return 0; 
    else if(strcmp(userCmd, "rm") == 0)
        return 0; 
    else if(strcmp(userCmd, "cp") == 0)
        return 0; 
    else
             return -1; 
}

/*
    Function: find_dir_entry
    Returns 0x20 for a file, returns 0x10 for a directory
    Will return -1 if no file/directory with provided name can be found
*/
int find_dir_entry(int fd, char* dirEntryName, int curDir){
    int i; 
    int type = 0; 
    off_t temp; 
    ssize_t temp2;
    int clusterBytes = 32;
    int empty = 0; 
    char name[11]; 
    int dataStart = bpb_information.bpb_bytspersec*(bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats*bpb_information.bpb_fatsz32)); 
    int currentClusterNum = 2 + (curDir - dataStart)/512;
    int curDirDataReg = curDir; 

    temp = lseek(fd, curDirDataReg, SEEK_SET);
    temp2 = read(fd, &empty, 4); 

    if(empty == 0)
        return -1; 

    while(empty != 0){

        temp = lseek(fd, curDirDataReg+clusterBytes, SEEK_SET);
        for(i=0; i<11; i++){
            temp2 = read(fd, &name[i], 1);    
        }
        temp2 = read(fd, &type, 1); 

        for(i=0; i<strlen(dirEntryName); i++){
            if(dirEntryName[i] != name[i])
                break; 
            if(dirEntryName[i] == name[i] && i == (strlen(dirEntryName)-1) && name[i+1] == ' ')
                return type; 
        }      

        temp = lseek(fd, 21, SEEK_CUR);
        temp2 = read(fd, &empty, 4); 
        clusterBytes+=64;  

        if(clusterBytes >= 512){
            if(next_cluster_num(fd, currentClusterNum) != -1){
                currentClusterNum = next_cluster_num(fd, currentClusterNum); 
                clusterBytes = 32; 

                curDirDataReg = dataStart + 512*(currentClusterNum-2)*bpb_information.bpb_secperclus;

                temp = lseek(fd, curDirDataReg, SEEK_SET);
                temp2 = read(fd, &empty, 4); 
            }
            else{
                empty = 0; 
            }   
        }

    }

    return -1; 

}

/*
    Function: dir_cluster_num
    Returns the cluster number for a given directory name in the current directory
    Will return -1 if no directory with provided name can be found
*/
int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int curCluster){

    int i; 
    int clusterBytes = 32;
    int currentClusterNum = curCluster; 
    int directory = curDir; 
    int empty = 0; 
    off_t temp; 
    int N = 0; 
    ssize_t temp2;
    int type =0; 
    char name[11]; 
    temp = lseek(fd, directory, SEEK_SET);
    temp2 = read(fd, &empty, 4); 

    if(empty == 0)
        return -1; 
    while(empty != 0){
        N = 0; 
        temp = lseek(fd, directory+clusterBytes, SEEK_SET);
                
        for(i=0; i<11; i++){
            temp2 = read(fd, &name[i], 1);    
        }
        //directory or not
        temp2 = read(fd, &type, 1);  
        
        temp = lseek(fd, directory+clusterBytes+26, SEEK_SET);
        temp2 = read(fd, &N, 2);  

        //type = find_dir_entry(fd, dirName, curDir);

        for(i=0; i<strlen(dirName); i++){
            if(dirName[i] != name[i])
                break; 
            if(dirName[i] == name[i] && i == (strlen(dirName)-1))
                if(type == 16){
                    //printf("found the directory\n"); 
                    //printf("firstDataLoc: %d\n", firstDataLoc); 
                    //printf("bpb_information.bpb_secperclus:%d\n", bpb_information.bpb_secperclus); 
                    return N;
                }
                else{
                    
                    return -1; 
                }
        }

        temp = lseek(fd, directory+clusterBytes+32, SEEK_SET);
        temp2 = read(fd, &empty, 4); 
        clusterBytes+=64; 

        if(clusterBytes >= 512){
            if(next_cluster_num(fd, currentClusterNum) != -1){
                currentClusterNum = next_cluster_num(fd, currentClusterNum); 
                clusterBytes = 32; 

                directory = firstDataLoc + 512*(currentClusterNum-2)*bpb_information.bpb_secperclus;

                temp = lseek(fd, directory, SEEK_SET);
                temp2 = read(fd, &empty, 4); 
            }
            else{
                empty = 0; 
            }   
        }
        
        
    }
    //printf("%s is not a directory\n", dirName); 
    return -1; 
}

/*
    Function: file_cluster_num
    Returns the cluster number for the contents of a given file name in the current directory
    Will return -1 if no file with provided name can be found
*/
int file_cluster_num(int fd, char fileName[11], int curDir, int firstDataLoc, int curCluster){
    int i; 
    int clusterBytes = 32;
    int currentClusterNum = curCluster; 
    int directory = curDir; 
    int empty = 0; 
    off_t temp; 
    int N = 0; 
    ssize_t temp2;
    int type =0; 
    char name[11]; 
    temp = lseek(fd, directory, SEEK_SET);
    temp2 = read(fd, &empty, 4); 

    if(empty == 0)
        return -1; 
    while(empty != 0){
        N = 0; 
        temp = lseek(fd, directory+clusterBytes, SEEK_SET);
                
        for(i=0; i<11; i++){
            temp2 = read(fd, &name[i], 1);    
        }
        //directory or not
        temp2 = read(fd, &type, 1);  
        
        temp = lseek(fd, directory+clusterBytes+26, SEEK_SET);
        temp2 = read(fd, &N, 2);  

        //type = find_dir_entry(fd, dirName, curDir);

         

        for(i=0; i<strlen(fileName); i++){
            if(fileName[i] != name[i])
                break; 
            if(fileName[i] == name[i] && i == (strlen(fileName)-1))
                if(type == FILE_ATTRIBUTE){
                    //printf("found the directory\n"); 
                    //printf("firstDataLoc: %d\n", firstDataLoc); 
                    //printf("bpb_information.bpb_secperclus:%d\n", bpb_information.bpb_secperclus); 
                    return N;
                }
                else{
                    
                    return -1; 
                }
        }

        temp = lseek(fd, 21, SEEK_CUR);
        temp2 = read(fd, &empty, 4); 
        clusterBytes+=64; 

        if(clusterBytes >= 512){
            if(next_cluster_num(fd, currentClusterNum) != -1){
                currentClusterNum = next_cluster_num(fd, currentClusterNum); 
                clusterBytes = 32; 

                directory = firstDataLoc + 512*(currentClusterNum-2)*bpb_information.bpb_secperclus;

                temp = lseek(fd, directory, SEEK_SET);
                temp2 = read(fd, &empty, 4); 
            }
            else{
                empty = 0; 
            }   
        }
        
        
    }
    //printf("%s is not a directory\n", dirName); 
    return -1; 

}

/*
    Function: next_cluster_num
    Navigates the FAT region to determine the cluster that comes after the current cluster
    Returns the next cluster number
    Will return -1 if current cluster is the last cluster
*/
int next_cluster_num(int fd, unsigned int currentClusterNum){
    unsigned int FATdata; 
    off_t temp;
    ssize_t temp2;

    temp = lseek(fd, (currentClusterNum*4 +(512*bpb_information.bpb_rsvdseccnt)) , SEEK_SET);
    temp2 = read(fd, &FATdata, 4); 
             
    if(FATdata >= 0x0FFFFFF8){
        //printf("Terminator FAT Data: %d\n", FATdata); 
        return -1; 
    }
    else{   
        //printf("Valid FAT Data: %d\n", FATdata); 
        return FATdata; 
    } 
}

/*
    Function: last_cluster_num
    Returns the last cluster number for the data for the current cluster
*/
int last_cluster_num(int fd, int currentClusterNum){
    int curCluster = currentClusterNum; 
    int nextCluster = 0; 

    nextCluster = next_cluster_num(fd, currentClusterNum); 

    if(nextCluster >= 0x0FFFFFFF)
        return curCluster; 
    else
        curCluster = nextCluster; 
    while(nextCluster < 0x0FFFFFFF){
        nextCluster = next_cluster_num(fd, currentClusterNum); 

        if(nextCluster >= 0x0FFFFFFF)
            return curCluster; 
        else
            curCluster = nextCluster; 
    }
    return -1; 
}
/*
    Function: file_size
    Returns the size listed on the directory entry for the file specified in the current directory
    Assumed that file is known to exist
*/
int file_size(int fd, char* fileName, int curDir){
    off_t temp;
    int i,j; 
    ssize_t temp2; 
    int directory = curDir;    
    int clusterBytes = 32; 
    int size = 0; 
    int empty = 0; 
    char name[11]; 
    char tempStr[11]; 
    temp = lseek(fd, curDir, SEEK_SET);
    temp2 = read(fd, &empty, 4);
    int dataStart = bpb_information.bpb_bytspersec*(bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats*bpb_information.bpb_fatsz32)); 
    int currentClusterNum = 2 + (curDir - dataStart)/512;
    int curDirDataReg = curDir; 


    while(empty != 0){

        temp = lseek(fd, directory+clusterBytes, SEEK_SET);
                
        for(i=0; i<11; i++){
            temp2 = read(fd, &name[i], 1);    
        }
        
        temp = lseek(fd, directory+clusterBytes+28, SEEK_SET);
        temp2 = read(fd, &size, 4);  

        for(i=0; i<strlen(fileName); i++){
            if(fileName[i] != name[i])
                break; 
            if(fileName[i] == name[i] && i == (strlen(fileName)-1))
                return size;
        }

        temp = lseek(fd, 21, SEEK_CUR);
        temp2 = read(fd, &empty, 4); 
        clusterBytes+=64; 

        if(clusterBytes >= 512){
            if(next_cluster_num(fd, currentClusterNum) != -1){
                currentClusterNum = next_cluster_num(fd, currentClusterNum); 
                clusterBytes = 32; 

                curDirDataReg = dataStart + 512*(currentClusterNum-2)*bpb_information.bpb_secperclus;

                temp = lseek(fd, curDirDataReg, SEEK_SET);
                temp2 = read(fd, &empty, 4); 
            }
            else{
                empty = 0; 
            }   
        }      
    }
}

/*
    Function: find_empty_cluster
    Reads through the FAT until the first empty cluster is found
*/
unsigned int find_empty_cluster(int fd, int dataRegStart){
    int fatregionstart = 0x4000;    //start at fatreg
    int clusterNumber;
    off_t temp;
    ssize_t temp2;
    int i = 0;
    for (i = 0; fatregionstart + (i * 4) < dataRegStart; i++) //fatregionstart + (i * 4) for reading 4 bytes at a time starting at 0x4000
    {
        temp = lseek(fd, fatregionstart + (i * 4), SEEK_SET);
        temp2 = read(fd, &clusterNumber, 4);

        if(clusterNumber == 0x0)
        {
            // printf("%d\n", i);
            return fatregionstart + (i * 4);
        }

    }
    return -1;
}
/*
    Function: data_region_loc
    Returns the data reg location for a given cluster number
*/
int data_region_loc(int clusterNum){

    return ((clusterNum-2)*512) + (bpb_information.bpb_bytspersec*(bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats*bpb_information.bpb_fatsz32))); 
}

/*
    Function: read_file
    Prints to the user the desired number of bytes of information
    from the contents of the specified file. 
    Also updates the offset of the file as the data is read
*/
void read_file(int fd, char fileName[11], int readSize, int fileDataClusterNum){
    off_t temp; 
    ssize_t temp2; 
    int i; 
    int counter = 1; 
    int fileDataRegNum = data_region_loc(fileDataClusterNum); 
    int clusterForData = fileDataClusterNum; 
    int offsetAtCluster = findOpenFile(fileName)->Offset; 
    char dataRead[readSize+1]; //+1 if including null char


    while(offsetAtCluster > bpb_information.bpb_bytspersec){
        offsetAtCluster -=  bpb_information.bpb_bytspersec; 
        clusterForData = next_cluster_num(fd, fileDataClusterNum); 
        fileDataRegNum = data_region_loc(clusterForData); 
    }

    temp = lseek(fd, fileDataRegNum + offsetAtCluster, SEEK_SET);

    for(i=0; i<readSize; i++){
        temp2 = read(fd, &dataRead[i], 1);
        printf("%c", dataRead[i]); 
        //increasing the offset for file by 1 as each byte is read
        current = head; 
        if(findOpenFile(fileName) != NULL){
            while (current->name != NULL){
                if(strcmp(current->name, fileName) == 0)
                    break;
                else{
                    current = current->next;
                }
            } 

            current->Offset++; 
        }
        //if read reaches the end of one cluster
        if(i+offsetAtCluster ==  (counter*bpb_information.bpb_bytspersec)-1){
            counter++; 
            clusterForData = next_cluster_num(fd, clusterForData); 
            fileDataRegNum = data_region_loc(clusterForData);
            offsetAtCluster = 0; 
            lseek(fd, fileDataRegNum, SEEK_SET);  
            //printf("next cluster is in data region at: %d\n", fileDataRegNum); 
        }
    }
    //dataRead[readSize] = '\0'; 
    //printf("read: %s\n", dataRead);  
    printf("\n");  
}
/*
    Function: write_to_file
    Write to the file the string provided by the user at the current
    offset of the file. 
    Updates the offset as data is written
*/
void write_to_file(int fd, char fileName[11], int sizeWrite, char* stringFromInput, int curDir, int fileDataClusterNum){
    off_t temp; 
    ssize_t temp2; 
    //uint8_t buff[1]; 
    int sizeOfWrite = sizeWrite; 
    char stringToWrite[sizeOfWrite]; //+1 for null char
    int i;
    int counter = 0;  

    int sizeOfFile = file_size(fd, fileName, curDir); 
    //int fileDataClusterNum = findOpenFile(fileName)->firClust; 
    int clusterForData = fileDataClusterNum; 
    int fileDataRegNum = data_region_loc(fileDataClusterNum); 
    int offsetAtCluster = findOpenFile(fileName)->Offset; 

    int currentNumClusters, finalNumClusters, extraClusters;  

    //determining start point for write
    while(offsetAtCluster > bpb_information.bpb_bytspersec){
        offsetAtCluster -=  bpb_information.bpb_bytspersec; 
        clusterForData = next_cluster_num(fd, fileDataClusterNum); 
        fileDataRegNum = data_region_loc(clusterForData); 
    }    

    //creating string to write
    for(i = 0; i<sizeOfWrite; i++){
        if(i >= strlen(stringFromInput)-2)
            stringToWrite[i] = 0; 
        else
            stringToWrite[i] = stringFromInput[i+1]; 
    }
    //stringToWrite[sizeOfWrite] = '\0'; 
    //printf("string to write: %s\n", stringToWrite); 

    //determining if extra clusters are needed
    if((findOpenFile(fileName)->Offset + sizeOfWrite) > sizeOfFile){
       // printf("extra clusters must be allocated for write\n"); 
        //filesize / bytes_per_cluster
        currentNumClusters = sizeOfFile / (bpb_information.bpb_bytspersec * bpb_information.bpb_secperclus); 
        if(sizeOfFile%(bpb_information.bpb_bytspersec * bpb_information.bpb_secperclus) >0)
            currentNumClusters ++; 
        finalNumClusters = (findOpenFile(fileName)->Offset + sizeOfWrite) / (bpb_information.bpb_bytspersec * bpb_information.bpb_secperclus); 
        if((findOpenFile(fileName)->Offset + sizeOfWrite) % (bpb_information.bpb_bytspersec * bpb_information.bpb_secperclus) > 0)
            finalNumClusters++; 
        extraClusters = finalNumClusters - currentNumClusters; 
        printf("%d extra cluster(s) are needed, but adding extra clusters to write and updating file size is not implemented\n Will write until file is full.\n", extraClusters);
        sizeOfWrite = sizeOfFile; 
    }
    
    //printf("location to start write: %d\n", fileDataRegNum + offsetAtCluster); 
    temp = lseek(fd, fileDataRegNum + offsetAtCluster, SEEK_SET);
    //writing string to file
    for(i=0; i<sizeOfWrite; i++){
        //printf("%c\n", stringToWrite[i]); 
        //buff = stringToWrite[i]; 
        write(fd, &stringToWrite[i], 1);

        //increasing the offset for file by 1 as each byte is written
        current = head; 
        if(findOpenFile(fileName) != NULL){
            while (current->name != NULL){
                if(strcmp(current->name, fileName) == 0)
                    break;
                else{
                    current = current->next;
                }
            }
            current->Offset++; 
        }
        //if write reaches the end of one cluster
        if(i+offsetAtCluster == (counter*bpb_information.bpb_bytspersec)-1){
            counter++; 
            clusterForData = next_cluster_num(fd, clusterForData); 
            fileDataRegNum = data_region_loc(clusterForData);
            offsetAtCluster = 0; 
            temp = lseek(fd, fileDataRegNum, SEEK_SET);  
            //printf("next cluster is in data region at: %d\n", fileDataRegNum); 
        }
    }


}
/*
    Function: create
    Creates a file or directory (and all the subsequent information involved) 
    passed on the dirCheck flag (1 = file, 0 = directory)
*/
int create(char* name, int dirCheck, int fd, int dataRegStart, int currDirectoryCluster){
 int theFirstDataSector = bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats * bpb_information.bpb_fatsz32);
    DIR_ENTRY * tempdir;
    off_t temp;
    ssize_t temp2;
    int fatregionstart = 0x4000;
    int empty = 0, empty2 = 0, found_space = 0, first_run = 0, i = 0, j =0;
    int offset, offset2, clustNum, saveoffset, firstOffset;
    
    int oldDirectoryCluster = currDirectoryCluster;

    while(1) {    //search for empty space

      if(first_run == 0)
      {
        offset = ((((currDirectoryCluster-2) * bpb_information.bpb_secperclus) + theFirstDataSector) * bpb_information.bpb_bytspersec);
        firstOffset = offset;
      }
      else
      {
        currDirectoryCluster = (next_cluster_num(fd, currDirectoryCluster));
        offset = ((((currDirectoryCluster-2) * bpb_information.bpb_secperclus) + theFirstDataSector) * bpb_information.bpb_bytspersec);
      }

      first_run++;

      for(i = 0; i * 64 < 512; i++){
          saveoffset = (offset + i *64);

          temp = lseek(fd, saveoffset, SEEK_SET);
          temp2 = read(fd, &empty, 1);
          //printf("tempdir: %x\n", tempdir);
          if(empty == 0x00)
          {
            found_space = 1;
            break;
          }

      }
      if(found_space)
        break;

      offset2 = fatregionstart + (4 * currDirectoryCluster);
      temp = lseek(fd, offset2, SEEK_SET);
      read(fd, &empty2, 4);

      if(empty2 == 0x0FFFFFF8 || empty2 == 0x0FFFFFFF || empty2 == 0x0000000)
      {
        found_space = 0;
        break;
      }
    }

    DIR_ENTRY new_entry;
    int calcHiLo;

    unsigned short attr;
    int empClusNumber;

    if(dirCheck == 1) // file
    {
      attr = 0x20;
    }
    else
    {
      attr = 0x10;
    }
    if(found_space)
    {
      int empClus = find_empty_cluster(fd, dataRegStart);

      int alotofF = 0x0FFFFFFF;

      if(dirCheck == 0)
      {
        uint32_t address, addressoffset;

        addressoffset = firstOffset/bpb_information.bpb_bytspersec;
        address = (-(bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats*bpb_information.bpb_fatsz32))
                  + (bpb_information.bpb_rootclus * bpb_information.bpb_secperclus) + addressoffset);

        // printf("address: %x\n", address);
        unsigned short clusHi = address >> 16;
        unsigned short clusLo = address&0x0000FFFF;
        temp2 = pwrite(fd, &alotofF, 4, empClus);
        uint8_t buff[512] = {0x2E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x10, 0x00, 0x64, 0x04, 0x8E, 0x78, 0x4E, 0x78, 0x4E,
                                 0x00, 0x00, 0x04, 0x8E, 0x78, 0x4E, 0xB3, 0x01, 0x00, 0x00,
                                 0x00, 0x00, 0x2E, 0x2E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x10, 0x00, 0x64, 0x04, 0x8E, 0x78, 0x4E,
                                 0x78, 0x4E, clusHi&0x00FF, clusHi>>8, 0x04, 0x8E, 0x78, 0x4E, clusLo&0x00FF, clusLo>>8,
                                 0x00, 0x00, 0x00, 0x00};

        empClusNumber = (empClus-0x4000)/0x4;
        offset = ((((empClusNumber-2) * bpb_information.bpb_secperclus) + theFirstDataSector) * bpb_information.bpb_bytspersec);
        pwrite(fd, buff, 64, offset);
        for(i = 0; i < 512; i++)
        {
          buff[i] = 0x00;
        }
        pwrite(fd, buff, 512-64, offset+64);
      }

      char new_entry[64];
      uint8_t directory[64];
      unsigned short ClusterHi = empClusNumber >> 16;
      unsigned short ClusterLo = empClusNumber&0x0000FFFF;
      for (i= 0, j =0; i < 64; i++)
      {
          if(i < 32)
          directory[i] = 0xFF;
          else if(i >= 32 && i <= 32+10)
          {
            directory[i] = name[j];
            j++;
          }
          else if(i == 32 + 11)
            directory[i] = attr;
          else if(i >= 32+12 && i < 32+20)
            directory[i] = 0x00;
          else if(i == 32+20)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterHi&0x00FF;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i == 32 + 21)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterHi>>8;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i > 32 + 21 && i < 32+26)
            directory[i] = 0x00;
          else if(i == 32 + 26)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterLo&0x00FF;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i == 32 + 27)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterLo >> 8;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i >= 32 + 28){
            directory[i] = 0x00;
          }
        }
        temp2 = pwrite(fd, directory, 64, saveoffset);
    }
    else
    {
        int empClus = find_empty_cluster(fd, dataRegStart);
        int alotofF = 0x0FFFFFFF;
        if(dirCheck == 0)
        {
          uint32_t address, addressoffset;
          addressoffset = firstOffset/bpb_information.bpb_bytspersec;
          // printf("addressoffset: %x\n", addressoffset);
          address = (-(bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats*bpb_information.bpb_fatsz32))
                    + (bpb_information.bpb_rootclus * bpb_information.bpb_secperclus) + addressoffset);

          // printf("address: %x\n", address);
          unsigned short clusHi = address >> 16;
          unsigned short clusLo = address&0x0000FFFF;
          pwrite(fd, &alotofF, 4, empClus);
          uint8_t buff[512] = {0x2E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x10, 0x00, 0x64, 0x04, 0x8E, 0x78, 0x4E, 0x78, 0x4E,
                                   0x00, 0x00, 0x04, 0x8E, 0x78, 0x4E, 0xB3, 0x01, 0x00, 0x00,
                                   0x00, 0x00, 0x2E, 0x2E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x10, 0x00, 0x64, 0x04, 0x8E, 0x78, 0x4E,
                                   0x78, 0x4E, clusHi&0x00FF, clusHi>>8, 0x04, 0x8E, 0x78, 0x4E, clusLo&0x00FF, clusLo>>8,
                                   0x00, 0x00, 0x00, 0x00};

          empClusNumber = (empClus-0x4000)/0x4;
          offset = ((((empClusNumber-2) * bpb_information.bpb_secperclus) + theFirstDataSector) * bpb_information.bpb_bytspersec);
          pwrite(fd, buff, 64, offset);
          for(i = 0; i < 512; i++)
          {
            buff[i] = 0x00;
          }
          pwrite(fd, buff, 512-64, offset+64);
        }

        int empClusNumber, offset3;
        unsigned short ClusterHi = empClusNumber >> 16;
        unsigned short ClusterLo = empClusNumber&0x0000FFFF;
        char new_entry[64];
        char directory[64];

        empClusNumber = (empClus-0x4000)/0x4;

        for (i= 0, j =0; i < 64; i++)
        {
          if(i < 32)
          directory[i] = 0xFF;
          else if(i >= 32 && i <= 32+10)
          {
            directory[i] = name[j];
            j++;
          }
          else if(i == 32 + 11)
            directory[i] = attr;
          else if(i >= 32+12 && i < 32+20)
            directory[i] = 0x00;
          else if(i == 32+20)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterHi&0x00FF;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i == 32 + 21)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterHi>>8;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i > 32 + 21 && i < 32+26)
            directory[i] = 0x00;
          else if(i == 32 + 26)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterLo&0x00FF;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i == 32 + 27)
          {
            if(dirCheck == 0)
            {
              directory[i] = ClusterLo >> 8;
            }
            else
            {
              directory[i] = 0x00;
            }
          }
          else if(i >= 32 + 28){
            directory[i] = 0x00;
          }
        }

        empClus = find_empty_cluster(fd, dataRegStart);
        temp = lseek(fd, empClus, SEEK_SET);
        temp2 = write(fd, &alotofF, 4);
        offset3 = 0x4000 + (4 * oldDirectoryCluster);
        temp = lseek(fd, offset3, SEEK_SET);
        temp2 = write(fd, &empClusNumber, 4);
        offset = ((((empClusNumber-2) * bpb_information.bpb_secperclus) + theFirstDataSector) * bpb_information.bpb_bytspersec);
        temp2 = pwrite(fd, directory, 64, offset);

    }
    return 0;   
}

