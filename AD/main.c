#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

void info(int, int, int, int, int, int, int);
char *get_input(void); 


int main(){

    while(1){
        char* input = get_input();

        if(strcmp(input, "exit") == 0){
            free(input);
            break; 
        }
        if(strcmp(input, "info") == 0){

            info(1,2,3,4,5,6,7); 
        }
    }
    
    return 0; 
}

void info(int bps, int spc, int rsc, int noF, int totS, int szF, int rc){
  //  char* test = "bytes per sector: %d\nsectors per cluster: %d\nreseverd sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n"; 
    printf("bytes per sector: %d\nsectors per cluster: %d\nreseverd sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n", bps, spc, rsc, noF, totS, szF, rc);
   // char* infoStr = sprintf( "bytes per sector: %d\nsectors per cluster: %d\nreseverd sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n", bps, spc, rsc, noF, totS, szF, rc); 
   // return infoStr
}

char *get_input(void)
{
	char *buffer = NULL;
	int bufsize = 0;

	char line[5];
	while (fgets(line, 5, stdin) != NULL) {
		int addby = 0;
		char *newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;

		buffer = (char *) realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;

		if (newln != NULL)
			break;
	}

	buffer = (char *) realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;

	return buffer;
}