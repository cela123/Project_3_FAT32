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

typedef struct bpb_info_struct
{
  unsigned short bpb_bytspersec, bpb_secperclus,
                bpb_rsvdseccnt, bpb_numfats;
  unsigned int bpb_fatsz32, bpb_rootclus, bpb_totsec32;
}Bpb_info_struct;

typedef struct { // DIRECTORY ENTRY
    unsigned char takeUpSpace[32];
    unsigned char DIR_Name[11];
} __attribute__((packed)) DIR_ENTRY;

Bpb_info_struct bpb_information;
void gather_info(int fd);
void print_info();
int dirLocation(int fd, char dirName[11], int curDir, int firstDataLoc, int BPB_SecPerClus);

int main(){

    int BPB_BytsPerSec, BPB_SecPerClus, BPB_RsvdSecCnt, BPB_NumFATs, BPB_TotSec32, BPB_FATSz32, BPB_RootClus;
    BPB_BytsPerSec = 0;
    BPB_SecPerClus = 0;
    BPB_RsvdSecCnt = 0;
    BPB_NumFATs = 0;
    BPB_TotSec32 = 0;
    BPB_FATSz32 = 0;
    BPB_RootClus = 0;
    off_t temp;
    ssize_t temp2;

    int dataRegStart;
    int test;
    int i;
    char fileName[11];
    int empty =0;

    int fd = open("fat32.img", O_RDONLY);
    gather_info(fd);

    int currDirectory;
    int prevDirectory;

    if (fd == -1)
        printf("Error opening file");

    temp = lseek(fd, 11, SEEK_CUR);
    temp2 = read(fd, &BPB_BytsPerSec, 2);
    temp2 = read(fd, &BPB_SecPerClus, 1);
    temp2 = read(fd, &BPB_RsvdSecCnt, 2);
    temp2 = read(fd, &BPB_NumFATs, 2);
    temp = lseek(fd, 14, SEEK_CUR);
    temp2 = read(fd, &BPB_TotSec32, 4);
    temp2 = read(fd, &BPB_FATSz32, 4);
    temp = lseek(fd, 4, SEEK_CUR);
    temp2 = read(fd, &BPB_RootClus, 4);

    dataRegStart = BPB_BytsPerSec*(BPB_RsvdSecCnt + (BPB_NumFATs*BPB_FATSz32));   //512*(32+(2*1009)) = 1049600 or 0x00100400
    //set current directory to the beginning of data region intially (ie start in root)
    currDirectory = dataRegStart;
    printf("Data Region Start = %d\n", dataRegStart);

    while(1){

        printf("$ ");
        char* input = get_input();
		    tokenlist *inputTokens = get_tokens(input);

        if(strcmp(inputTokens->items[0], "exit") == 0){
            free(input);
            break;
        }
        if(strcmp(inputTokens->items[0], "info") == 0){
            /*printf("bytes per sector: %d\nsectors per cluster: %d\nreserved sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n",
            BPB_BytsPerSec, BPB_SecPerClus, BPB_RsvdSecCnt, BPB_NumFATs, BPB_TotSec32, BPB_FATSz32, BPB_RootClus);*/
            print_info();
        }
        if(strcmp(inputTokens->items[0], "size") == 0)
        {

        }
        if(strcmp(inputTokens->items[0], "ls") == 0){
            int directory;
            if(inputTokens->items[1]!= NULL){       //if your trying to ls into a directory you are currently not within
                //determine start of directory specified
                printf("ls for a specific file\n");
                if((dirLocation(fd, inputTokens->items[1], currDirectory, dataRegStart, BPB_SecPerClus)) != -1)
                    directory = dirLocation(fd, inputTokens->items[1], currDirectory, dataRegStart, BPB_SecPerClus);
                else
                {
                        continue;
                }
                printf("directory for ls: %d\n", directory);
            }
            else{
                directory = currDirectory;
            }

            int n = 32;
            temp = lseek(fd, directory, SEEK_SET);
            temp2 = read(fd, &empty, 4);
            while(empty != 0){
                printf("Empty = %d\n", empty);
                temp = lseek(fd, directory+n, SEEK_SET);

                for(i=0; i<11; i++){
                temp2 = read(fd, &fileName[i], 1);

                }
                printf("file within empty!=0 loop: %s\n", fileName);

                temp = lseek(fd, 21, SEEK_CUR);
                temp2 = read(fd, &empty, 4);
                n+=64;
            }
        }
        if(strcmp(inputTokens->items[0], "cd") == 0)
        {

        }
        if(strcmp(inputTokens->items[0], "creat") == 0)
        {

        }
        if(strcmp(inputTokens->items[0], "mkdir") == 0)
        {

        }
        if(strcmp(inputTokens->items[0], "mv") == 0)
        {

        }
        if(strcmp(inputTokens->items[0], "open") == 0)
        {

        }


    }

    close(fd);
    return 0;
}

void gather_info(int fd)
{
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

void print_info()
{
    printf("bpb_bytspersec: %d\nbpb_secperclus: %d\nbpb_rsvdseccnt: %d\nbpb_numfats: %d\nbpb_totsec32: %d\nbpb_fatsz32: %d\nbpb_rootclus: %d\n",
    bpb_information.bpb_bytspersec, bpb_information.bpb_secperclus, bpb_information.bpb_rsvdseccnt,
    bpb_information.bpb_numfats, bpb_information.bpb_totsec32, bpb_information.bpb_fatsz32,
    bpb_information.bpb_rootclus);
}


int dirLocation(int fd, char dirName[11], int curDir, int firstDataLoc, int BPB_SecPerClus){
    //printf("directory name: %s\n", dirName);
    printf("length of dirName: %d\n", strlen(dirName));
    int i;
    int n = 32;
    int empty = 0;
    off_t temp;
    int N = 0;
    ssize_t temp2;
    int type =0;
    char name[11];
    temp = lseek(fd, curDir, SEEK_SET);
    temp2 = read(fd, &empty, 4);

    if(empty == 0)
        return -1;
    while(empty != 0){
        N = 0;
        temp = lseek(fd, curDir+n, SEEK_SET);

        for(i=0; i<11; i++){
            temp2 = read(fd, &name[i], 1);
        }
        //directory or not
        temp2 = read(fd, &type, 1);

        temp = lseek(fd, curDir+n+26, SEEK_SET);
        temp2 = read(fd, &N, 2);
        printf("N: %d\n", N);

         printf("%stype:%d\n", name, type);
        for(i=0; i<strlen(dirName); i++){
            if(dirName[i] != name[i])
                break;
            if(dirName[i] == name[i] && i == (strlen(dirName)-1))
                if(type == 16){
                    //printf("found the directory\n");
                    //printf("firstDataLoc: %d\n", firstDataLoc);
                    //printf("BPB_SecPerClus:%d\n", BPB_SecPerClus);
                    return (firstDataLoc + 512*((N-2)*BPB_SecPerClus));
                }
                else{
                    printf("%s is not a directory\n", dirName);
                    return -1;
                }
        }
        // if(strcmp(dirName, name)==0 && type == 16)
        //     return (firstDataLoc + (N-2)*BPB_SecPerClus);

        // else if(strcmp(dirName, name)==0 && type == 16){
        //     printf("%s is not a directory\n", dirName);
        //     return -1;
        // }

        temp = lseek(fd, 21, SEEK_CUR);
        temp2 = read(fd, &empty, 4);
        n+=64;
    }
    printf("%s is not a directory\n", dirName);
    return -1;
}
