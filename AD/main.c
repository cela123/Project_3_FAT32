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
    int fd = open("fat32.img", O_RDONLY);
    
   
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
    }

    close(fd); 
    return 0; 
}

void print_info(int bps, int spc, int rsc, int noF, int totS, int szF, int rc){

    printf("bytes per sector: %d\nsectors per cluster: %d\nreserved sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n", bps, spc, rsc, noF, totS, szF, rc);

}
