#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "parser.h"

void print_info(int, int, int, int, int, int, int);


int main(){

    int fd = open("fat32.img", O_RDONLY);

    if (fd == -1)
        printf("Error opening file");

    while(1){

        printf("$ ");
        char* input = get_input();
		tokenlist *inputTokens = get_tokens(input);

        if(strcmp(inputTokens->items[0], "exit") == 0){
            free(input);
            break; 
        }
        if(strcmp(inputTokens->items[0], "info") == 0){

            print_info(1,2,3,4,5,6,7); 
        }
    }
    
    return 0; 
}

void print_info(int bps, int spc, int rsc, int noF, int totS, int szF, int rc){

    printf("bytes per sector: %d\nsectors per cluster: %d\nreseverd sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n", bps, spc, rsc, noF, totS, szF, rc);

}

// char *get_input(void)
// {
// 	char *buffer = NULL;
// 	int bufsize = 0;

// 	char line[5];
// 	while (fgets(line, 5, stdin) != NULL) {
// 		int addby = 0;
// 		char *newln = strchr(line, '\n');
// 		if (newln != NULL)
// 			addby = newln - line;
// 		else
// 			addby = 5 - 1;

// 		buffer = (char *) realloc(buffer, bufsize + addby);
// 		memcpy(&buffer[bufsize], line, addby);
// 		bufsize += addby;

// 		if (newln != NULL)
// 			break;
// 	}

// 	buffer = (char *) realloc(buffer, bufsize + 1);
// 	buffer[bufsize] = 0;

// 	return buffer;
// }