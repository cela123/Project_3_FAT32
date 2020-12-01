#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "parser.h"

#define FILE_ATTRIBUTE 0X20
#define DIRECTORY_ATTRIBUTE 0X10

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

//Function Declarations
void gather_info(int fd);
void print_info();
int isCommand(char *);
int find_dir_entry(int fd, char* dirName, int curDir);
int create(char* name, int dirCheck, int fd, int dataRegStart, int currDirectoryCluster);

int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int curCluster);
int next_cluster_num(int fd, unsigned int currentClusterNum);

int file_size(int fd, char* fileName, int curDir);
unsigned int find_empty_cluster(int fd, int dataRegStart);
void create_dir_entry(int type);
void fill_dir_entry(DIR_ENTRY* entry, int dirLoc, int fd);


//MAIN STARTS
int main(){
    off_t temp;
    ssize_t temp2;

    int dataRegStart;
    int test;
    int i, j;
    char fileName[11];
    int empty =0;

    //-------CURRENT DIRECTORY DATA-------
    int currDirectory;
    int currDirectoryCluster;
    //------------------------------------

    int fd = open("fat32.img", O_RDWR);
    if (fd == -1)
        printf("Error opening file");

    gather_info(fd);

    DIR_ENTRY * temp;
    fill_dir_entry()


    dataRegStart = bpb_information.bpb_bytspersec*(bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats*bpb_information.bpb_fatsz32));
    //set current directory to the beginning of data region intially (ie start in root)
    currDirectory = dataRegStart;
    currDirectoryCluster = bpb_information.bpb_rootclus;
    printf("Data Region Start = %d\n", dataRegStart);
    printf("Root Cluster = %d\n", currDirectoryCluster);

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
            int directory;
            int clusterNumber = 0;
            int clusterBytes = 32;
            int counter = 0;

            if(inputTokens->items[1]!= NULL){
                //determine start of directory specified
                printf("ls for a specific file\n");
                if((dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster)) != -1){
                        directory = (dataRegStart + 512*(( (dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster)) -2)*bpb_information.bpb_secperclus));
                        clusterNumber = dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster);
                        printf(".          \t");
                        counter++;

                }
                else
                {
                    printf("%s is not a directory\n", inputTokens->items[1]);
                    continue;
                }

                //printf("directory for ls: %d\n", directory);
            }
            else{
                directory = currDirectory;
                clusterNumber = currDirectoryCluster;
            }

            //printf("directory: %d\n", directory);
            //printf("clusterNumber: %d\n", clusterNumber);

            temp = lseek(fd, directory, SEEK_SET);
            temp2 = read(fd, &empty, 4);

            //reading "." for directories not the root
            if((currDirectory != dataRegStart) && empty != 0){
                temp = lseek(fd, directory, SEEK_SET);
                for(i=0; i<11; i++){
                    temp2 = read(fd, &fileName[i], 1);
                }
                printf("%s\t", fileName);
                counter++;
                temp = lseek(fd, directory+32, SEEK_SET);
                temp2 = read(fd, &empty, 4);

            }

            while(empty != 0){
                //printf("Empty = %d\n", empty);
                empty = 0;
                temp = lseek(fd, directory+clusterBytes, SEEK_SET);

                for(i=0; i<11; i++){
                    temp2 = read(fd, &fileName[i], 1);
                }
                printf("%s\t", fileName);

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
                if(counter == 6){
                    printf("\n");
                    counter = 0;
                }
            }
            printf("\n");

        }
        if(strcmp(inputTokens->items[0], "cd") == 0){
            int newDirectoryCluster = 0;
            if(inputTokens->items[1] == NULL){
                printf("No directory specified for cd you fucking idiot\n");
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

            if(inputTokens->items[1] == NULL){
                printf("Missing operand for creat\n");
                continue;
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20){
                printf("%s is already a file\n", inputTokens->items[1]);
                continue;
            }
            //if(dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster))

            else{
                printf("creating new file\n");
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

            if(find_dir_entry(fd, inputTokens->items[2], currDirectory) == -1){
                printf("rename %s to %s\n", inputTokens->items[1], inputTokens->items[2]);
                //if TO does not exit, rename FROM to TO
            }
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10){
                printf("moving %s to %s\n", inputTokens->items[1], inputTokens->items[2]);
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
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20 && inputTokens->items[2] != NULL){
                if(strcmp(inputTokens->items[2], "r") == 0){
                    printf("Open %s read only\n", inputTokens->items[1]);
                }
                else if(strcmp(inputTokens->items[2], "w") == 0){
                    printf("Open %s write only\n", inputTokens->items[1]);
                }
                else if(strcmp(inputTokens->items[2], "rw") == 0){
                    printf("Open %s read and write\n", inputTokens->items[1]);
                }
                else if(strcmp(inputTokens->items[2], "wr") == 0){
                    printf("Open %s write and read\n", inputTokens->items[1]);
                }
                else{
                    printf("Invalid mode for open: %s\n", inputTokens->items[2]);
                    continue;
                }
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
            //implement a size function and compare function return and offset user provides

        }
        if(strcmp(inputTokens->items[0], "lseek") == 0){    //lseek FILENAME OFFSET
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n");
                continue;
            }
            if(inputTokens->items[2] == NULL){
                printf("Missing offset operand\n");
                continue;
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

        }
        if(strcmp(inputTokens->items[0], "write") == 0){    //write FILENAME SIZE "STRING"
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n");
                continue;
            }
            if(inputTokens->items[2] == NULL){
                printf("Missing size operand\n");
                continue;
            }
            if(inputTokens->items[3] == NULL){
                printf("Missing string to write to %s\n", inputTokens->items[1]);
                continue;
            }
            //error if name DNE or is for a directory not a file
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == -1){
                printf("File %s does not exist\n", inputTokens->items[1]);
                continue;
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
            if(find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x10 || find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20)  {
                printf("Removing %s\n", inputTokens->items[1]);
            }

        }
        if(strcmp(inputTokens->items[0], "cp") == 0){       //cp FILENAME TO
            if(inputTokens->items[1] == NULL){
                printf("Missing file operand\n");
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
                printf("Creating copy of %s in current directory with name %s\n", inputTokens->items[1], inputTokens->items[2]);
                continue;
            }

            //if TO is directory, create copy of FILENAME in TO
            if(find_dir_entry(fd, inputTokens->items[2], currDirectory) == 0x10 && find_dir_entry(fd, inputTokens->items[1], currDirectory) == 0x20){
                printf("Creating copy of %s in %s\n", inputTokens->items[1], inputTokens->items[2]);
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
    printf("spc: %d\n", bpb_information.bpb_secperclus);
    temp_ssize_t = read(fd, &bpb_information.bpb_rsvdseccnt, 2);
    temp_ssize_t = read(fd, &bpb_information.bpb_numfats, 2);
    temp_off_t = lseek(fd, 14, SEEK_CUR);
    temp_ssize_t = read(fd, &bpb_information.bpb_totsec32, 4);
    temp_ssize_t = read(fd, &bpb_information.bpb_fatsz32, 4);
    temp_off_t = lseek(fd, 4, SEEK_CUR);
    temp_ssize_t = read(fd, &bpb_information.bpb_rootclus, 4);
}

void print_info(){

    printf("bpb_bytspersec: %d\nbpb_secperclus: %d\nbpb_rsvdseccnt: %d\nbpb_numfats: %d\nbpb_totsec32: %d\nbpb_fatsz32: %d\nbpb_rootclus: %d\n",
    bpb_information.bpb_bytspersec, bpb_information.bpb_secperclus, bpb_information.bpb_rsvdseccnt,
    bpb_information.bpb_numfats, bpb_information.bpb_totsec32, bpb_information.bpb_fatsz32,
    bpb_information.bpb_rootclus);

}

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


int file_size(int fd, char* fileName, int curDir){off_t temp;
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
    Re
*/
unsigned int find_empty_cluster(int fd, int dataRegStart){
  int fatregionstart = 0x4000;    //start at fatreg
  int clusterNumber;
  off_t temp;
  ssize_t temp2;
  for (int i = 0; fatregionstart + (i * 4) < dataRegStart; i++) //fatregionstart + (i * 4) for reading 4 bytes at a time starting at 0x4000
  {
      temp = lseek(fd, fatregionstart + (i * 4), SEEK_SET);
      temp2 = read(fd, &clusterNumber, 4);

      if(clusterNumber == 0x0)
        return fatregionstart + (i * 4);
  }
  return -1;
}

int create(char* name, int dirCheck, int fd, int dataRegStart, int currDirectoryCluster){

    int theFirstDataSector = bpb_information.bpb_rsvdseccnt + (bpb_information.bpb_numfats * bpb_information.bpb_fatsz32);
    DIR_ENTRY * tempdir;
    //fill_dir_entry(tempdir, 1049600, fd);
    off_t temp;
    ssize_t temp2;
    int fatregionstart = 0x4000;
    int empty = 0;
    int empty2 = 0;
    int offset, offset2;
    int found_space = 0;
    int first_run = 0;
    int clustNum;
    int saveoffset;

    int oldDirectoryCluster = currDirectoryCluster;

    while(1) {    //search for empty space

      if(first_run == 0)
      {
        offset = ((((currDirectoryCluster-2) * bpb_information.bpb_secperclus) + theFirstDataSector) * bpb_information.bpb_bytspersec);
        printf("offset (first_run == 1): %d\n", offset);
      }
      else
      {
        currDirectoryCluster = (next_cluster_num(fd, currDirectoryCluster));
        offset = ((((currDirectoryCluster-2) * bpb_information.bpb_secperclus) + theFirstDataSector) * bpb_information.bpb_bytspersec);
        printf("offset (first_run == 1): %d\n", offset);
      }
      first_run++;

      for(int i = 0; i * 64 < 512; i++){
          saveoffset = (offset + i *64);
          temp = lseek(fd, saveoffset, SEEK_SET);
          temp2 = read(fd, &empty, 1);
          //printf("tempdir: %x\n", tempdir);
          if(empty == 0x00)
          {
            //save i * 64 and use this to insert the data
            printf("saveoffset: %x\n", saveoffset);
            found_space = 1;
            break;
          }

      }
      if(found_space)
        break;

      //might not even be necessary

      offset2 = fatregionstart + (4 * currDirectoryCluster);
      temp = lseek(fd, offset2, SEEK_SET);
      read(fd, &empty2, 4);

      printf("currDirectoryCluster: %d\n", currDirectoryCluster);
      printf("empty2 is %d\n", empty2);

      if(empty2 == 0x0FFFFFF8 || empty2 == 0x0FFFFFFF)
      {
        found_space = 0;
        break;
      }


    }

    printf("found_space: %d\n", found_space);

    if(found_space)
    {
        DIR_ENTRY insert;
        int empClus = find_empty_cluster(fd, dataRegStart);
        strncpy((char*)insert.DIR_Name, name, 11);
        printf("insert.DIR_Name: %s\n", insert.DIR_Name);

/*
        if (dirCheck == 0)    //0 is file 1 is directory
        {
          printf("(dirCheck == 0) insert.DIR_Attributes\n");
          insert.DIR_Attributes = 0x20;
        }
        else
        {
          printf("(dirCheck == 1) insert.DIR_Attributes\n");
          insert.DIR_Attributes = 0x10;
        }
*/
        printf("insert.DIR_Attributes: %s\n", insert.DIR_Attributes);

        insert.DIR_FileSize = 0;
        int holder = find_empty_cluster(fd, dataRegStart);
        printf("holder: %d\n", holder);
        insert.DIR_FstClusHI = holder / 0x100;
        insert.DIR_FstClusLo = holder % 0x100;
        printf("insert.DIR_FstClusHI: %d\ninsert.DIR_FstClusLo: %d\n", insert.DIR_FstClusHI, insert.DIR_FstClusLo);

        //temp = lseek(fd, saveoffset, SEEK_SET);    //need offset of the spot
       /* unsigned char buff[64];
        for(int i = 0; i < 64; i++)
        {
          buff[i] = 0xFF;
        }*/
        temp2 = pwrite(fd, &insert, 64, saveoffset);

    }

    //if not found allocate new cluster to insert data

    currDirectoryCluster = oldDirectoryCluster;
    return 0;
}

void fill_dir_entry(DIR_ENTRY* entry, int dirLoc, int fd){
    printf("begin fill_dir_entry function\n");
    off_t temp;
    ssize_t temp2;
    printf("before lseek\n");
    temp = lseek(fd, dirLoc, SEEK_SET);
    printf("after lseek\n");
    //grab the important data at a direntry to copy
    temp2 = read(fd, &entry->DIR_Name, 11);
    printf("after first read\n");
    entry->DIR_Name[10] = '\0';     //null termination
    temp2 = read(fd, &entry->DIR_Attributes, 1);
    temp = lseek(fd, 8, SEEK_CUR);
    temp2 = read(fd, &entry->DIR_FstClusHI, 2);
    temp = lseek(fd, 4, SEEK_CUR);
    temp2 = read(fd, &entry->DIR_FstClusLo, 2);
    temp2 = read(fd, &entry->DIR_FileSize, 4);

    printf("DIR_Name: %s\nDIR_Attributes: %d\nDIR_FstClusHI: %d\nDIR_FstClusLo: %d\nDIR_FileSize: %d\n",
    entry->DIR_Name, entry->DIR_Attributes, entry->DIR_FstClusHI, entry->DIR_FstClusLo, entry->DIR_FileSize);
    printf("end fill_dir_entry function\n");
}
