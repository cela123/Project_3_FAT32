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

int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int curCluster); 
int next_cluster_num(int fd, unsigned int currentClusterNum); 
unsigned int find_empty_cluster(int fd); 

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

    int fd = open("fat32.img", O_RDONLY);
    if (fd == -1)
        printf("Error opening file");

    gather_info(fd); 

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
            short n = 32;
            int size = 0;
            int type = 0;  
            char tempStr[11]; 
            temp = lseek(fd, currDirectory, SEEK_SET);
            temp2 = read(fd, &empty, 4);
            if(inputTokens->items[1] == NULL){
                printf("No file specified for size\n"); 
                continue; 
            }
            while(empty != 0){
                temp = lseek(fd, currDirectory+n, SEEK_SET);

                for(i = 0; i<11; i++){
                    temp2 = read(fd, &tempStr[i], 1);
                }
                temp2 = read(fd, &type, 1);
                // Removing trailing blank spaces
                for(i = 0; i < 11; i++)
                {
                     if (!(tempStr[i] == ' ' && tempStr[i+1] == ' '))
                     {
                         fileName[j] = tempStr[i];
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
                n += 64;
            } // End of While

            if(type != 32)
                printf("%s is not a file\n", inputTokens->items[1]); 
            else{
                temp = lseek(fd, currDirectory+n+28, SEEK_SET);
                temp2 = read(fd, &size, 4);
                printf("%d\n", size);
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

                }
                else
                {
                    continue; 
                }
                
                printf("directory for ls: %d\n", directory); 
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
                if(counter == 10){
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
                if(newDirectoryCluster != -1){

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
            find_empty_cluster(fd); 
            temp = lseek(fd, -4, SEEK_CUR);
            write(fd, 0xFFFFFFFF, 4); 
        }
    }


    close(fd); 
    return 0; 
}

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
                    printf("%s is not a directory\n", dirName);
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
    printf("%s is not a directory\n", dirName); 
    return -1; 
}


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

unsigned int find_empty_cluster(int fd){
    int emptyCluster = 0; 
    off_t temp; 
    ssize_t temp2;
    temp = lseek(fd, (bpb_information.bpb_rsvdseccnt*512) + (4*bpb_information.bpb_rootclus), SEEK_SET);
    temp2 =read(fd, &emptyCluster, 4); 

    while(emptyCluster != 0X0){
        emptyCluster = 0;     
        temp2 =read(fd, &emptyCluster, 4); 
        printf("emptuCluster: %d\n", emptyCluster); 
    }

    return emptyCluster; 

    
}