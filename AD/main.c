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

void print_info(int, int, int, int, int, int, int);
int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int spc); 
int next_cluster_num(int fd, unsigned int currentClusterNum, int rsc); 

int main(){

    int bps, spc, rsc, noF, totS, szF, rc;
    bps = 0; 
    spc = 0; 
    rsc = 0; 
    noF = 0; 
    totS = 0; 
    szF = 0; 
    rc = 0; 
    off_t temp; 
    ssize_t temp2;

    int dataRegStart; 
    int test; 
    int i; 
    char fileName[11]; 
    int empty =0; 

    int fd = open("fat32.img", O_RDONLY);
    
    int currDirectory;

    if (fd == -1)
        printf("Error opening file");

    temp = lseek(fd, 11, SEEK_CUR);
    temp2 = read(fd, &bps, 2); 
    temp2 = read(fd, &spc, 1); 
    temp2 = read(fd, &rsc, 2); 
    temp2 = read(fd, &noF, 2); 
    temp = lseek(fd, 14, SEEK_CUR);
    temp2 = read(fd, &totS, 4); 
    temp2 = read(fd, &szF, 4); 
    temp = lseek(fd, 4, SEEK_CUR);
    temp2 = read(fd, &rc, 4);   

    dataRegStart = bps*(rsc + (noF*szF)); 
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

            print_info(bps,spc,rsc,noF,totS,szF,rc); 
        }
        if(strcmp(inputTokens->items[0], "ls") == 0){
            int directory; 
            int clusterNumber = 0; 
            int clusterBytes = 32; 

            if(inputTokens->items[1]!= NULL){
                //determine start of directory specified
                printf("ls for a specific file\n"); 
                if((dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, spc)) != -1){
                        directory = (dataRegStart + 512*(( (dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, spc)) -2)*spc));
                        clusterNumber = dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, spc); 

                }
                else
                {
                    continue; 
                }
                
                printf("directory for ls: %d\n", directory); 
            }
            else{
                directory = currDirectory; 
                temp = lseek(fd, directory+(3*clusterBytes)+26, SEEK_SET);
                temp2 = read(fd, &clusterNumber, 2); 
            }       

            printf("directory: %d\n", directory); 
            printf("clusterNumber: %d\n", clusterNumber); 

            temp = lseek(fd, directory, SEEK_SET);
            temp2 = read(fd, &empty, 4); 
            while(empty != 0){
                //printf("Empty = %d\n", empty); 
                empty = 0; 
                temp = lseek(fd, directory+clusterBytes, SEEK_SET);
                
                for(i=0; i<11; i++){
                    temp2 = read(fd, &fileName[i], 1);            
                }
                printf("%s\n", fileName); 

                temp = lseek(fd, 21, SEEK_CUR);
                temp2 = read(fd, &empty, 4); 

                clusterBytes+=64; 

                if(clusterBytes >= 512){
                    if(next_cluster_num(fd, clusterNumber, rsc) != -1){
                        clusterNumber = next_cluster_num(fd, clusterNumber, rsc); 
                        clusterBytes = 32; 

                        directory = dataRegStart + 512*(clusterNumber-2)*spc;

                        temp = lseek(fd, directory, SEEK_SET);
                        temp2 = read(fd, &empty, 4); 
                    }
                    else
                    {
                        empty = 0; 
                    }
                    
                }


            }
        }
    }

    close(fd); 
    return 0; 
}

void print_info(int bps, int spc, int rsc, int noF, int totS, int szF, int rc){

    printf("bytes per sector: %d\nsectors per cluster: %d\nreserved sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n", bps, spc, rsc, noF, totS, szF, rc);

}

int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int spc){

    int i; 
    int clusterBytes = 32;
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
        temp = lseek(fd, curDir+clusterBytes, SEEK_SET);
                
        for(i=0; i<11; i++){
            temp2 = read(fd, &name[i], 1);    
        }
        //directory or not
        temp2 = read(fd, &type, 1);  
        
        temp = lseek(fd, curDir+clusterBytes+26, SEEK_SET);
        temp2 = read(fd, &N, 2);  
        //printf("N: %d\n", N); 

         //printf("%stype:%d\n", name, type); 
        for(i=0; i<strlen(dirName); i++){
            if(dirName[i] != name[i])
                break; 
            if(dirName[i] == name[i] && i == (strlen(dirName)-1))
                if(type == 16){
                    //printf("found the directory\n"); 
                    //printf("firstDataLoc: %d\n", firstDataLoc); 
                    //printf("spc:%d\n", spc); 
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
    }
    printf("%s is not a directory\n", dirName); 
    return -1; 
}


int next_cluster_num(int fd, unsigned int currentClusterNum, int rsc){
    unsigned int FATdata; 
    off_t temp;
    ssize_t temp2;

    printf("Cluster Caclulation: %d\n", (currentClusterNum*4 +(512*rsc))); 
    temp = lseek(fd, (currentClusterNum*4 +(512*rsc)) , SEEK_SET);
    temp2 = read(fd, &FATdata, 4); 
             
    if(FATdata >= 0x0FFFFFF8){
        printf("Terminator FAT Data: %d\n", FATdata); 
        return -1; 
    }
    else{   
        printf("Valid FAT Data: %d\n", FATdata); 
        return FATdata; 
    }
    
    
}