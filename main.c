#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "parser.c"


void printInfo(int, int, int, int, int, int, int);
char *get_input(void); 


int main(){

	int fd = open("fat32.img", O_RDONLY);
	if (fd == -1)
		printf("Error opening file");
	// unsigned short (2 bytes)
	// unsigned int (4 bytes)
	unsigned short bps = 0, spc = 0, rsc = 0, noF = 0;
	unsigned int rc = 0, ts32 = 0, fz32 = 0, dataRegStart = 0, currDirectory = 0, empty = 0;
	short i = 0, j = 0;
	unsigned int size = 0;
	char fileName[11];
	char tempStr[11];
	off_t tempS;
	ssize_t tempR;

	tempS = lseek(fd, 11, SEEK_CUR);
	tempR = read(fd, &bps, 2);	
	tempR = read(fd, &spc, 1);
	tempR = read(fd, &rsc, 2);
	tempR = read(fd, &noF, 2);
	tempS = lseek(fd, 14, SEEK_CUR);
	tempR = read(fd, &ts32, 4);
	tempR = read(fd, &fz32, 4);
	tempS = lseek(fd, 4, SEEK_CUR);
	tempR = read(fd, &rc, 4);

	dataRegStart = bps*(rsc + (noF*fz32));
	//initialize directory to root
	currDirectory = dataRegStart;
	while(1){
		
		
		printf("$ ");
		
		char* input = get_input();
		tokenlist *tokens = get_tokens(input); 
		
		// printf("Whole input: %s\n", input);
		// for (int i = 0; i < tokens->size; i++) 
		// {
		// 	printf("token %d: (%s)\n", i, tokens->items[i]);
		// }

		if(strcmp(tokens->items[0], "exit") == 0){
			free(input);
			printf("Exiting Program\n");
			break; 
		}
		if(strcmp(tokens->items[0], "info") == 0){

			printInfo(bps, spc, rsc, noF, ts32, fz32, rc); 
		}
		if(strcmp(tokens->items[0], "ls") == 0){
			short n = 32;
			tempS = lseek(fd, currDirectory, SEEK_SET);
			tempR = read(fd, &empty, 4);
			
			while(empty != 0){
				tempS = lseek(fd, currDirectory+n, SEEK_SET);

				for(i = 0; i<11; i++){
					tempR = read(fd, &tempStr[i], 1);
				}
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
				
				printf("%s ", fileName);

				tempS = lseek(fd, 21, SEEK_CUR);
				tempR = read(fd, &empty, 4);
				n += 64;
			}
			printf("\n");
		}
		if(strcmp(tokens->items[0], "size") == 0){
			short n = 32;
			tempS = lseek(fd, currDirectory, SEEK_SET);
			tempR = read(fd, &empty, 4);
			
			while(empty != 0){
				tempS = lseek(fd, currDirectory+n, SEEK_SET);
				
				for(i = 0; i<11; i++){
					tempR = read(fd, &tempStr[i], 1);
				}				
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
				if (strcmp(fileName, tokens->items[1]) == 0)
				{
					break;
				}
				
				tempS = lseek(fd, 21, SEEK_CUR);
				tempR = read(fd, &empty, 4);
				n += 64;
			} // End of While
			
			tempS = lseek(fd, currDirectory+n+28, SEEK_SET);
			tempR = read(fd, &size, 4);
			printf("%d\n", size);
		}
		if(strcmp(tokens->items[0], "cd") == 0){
			printf("Change Directory\n");
			for (int i = 1; i < tokens->size; i++) {
				printf("%s ", tokens->items[i]);
			}
			printf("\n");
		}
	
		free(input);
		free_tokens(tokens);
	} // End of While
	

	if (close(fd) < 0)
	{
		exit(1);
	}  
	printf("Closed the fd. \n");

    return 0; 
}

void printInfo(int bps, int spc, int rsc, int noF, int totS, int szF, int rc){

    printf("bytes per sector: %d\nsectors per cluster: %d\nreseverd sector count: %d\nnumber of FATs: %d\ntotal sectors: %d\nFATsize: %d\nroot cluster: %d\n", bps, spc, rsc, noF, totS, szF, rc);

}
