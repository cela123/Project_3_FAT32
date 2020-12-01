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

#define FILE_ATTRIBUTE 0X20
#define DIRECTORY_ATTRIBUTE 0X10
#define READ 0x30
#define WRITE 0x40
#define READWRITE 0x50
#define WRITEREAD 0x60


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

// adds node to the end
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

struct node* delete(char name[11]){
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
    return current;
}

//display list
void printOpenFiles(){

	struct openFiles *ptr = head;
	while (ptr != NULL){
		printf("%s ", ptr->name);
		ptr = ptr->next;
	}
	printf("\n");
}

// Function Declarations
void gather_info(int fd);
void print_info();
int isCommand(char *);
int find_dir_entry(int fd, char* dirName, int curDir);

int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int curCluster); 
int next_cluster_num(int fd, unsigned int currentClusterNum); 
unsigned int find_empty_cluster(int fd);
void create_dir_entry(int type);

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
	//initialize directory to root
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
				printf("No file specificed for size\n");
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
			
			if (type != 32)
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

			if(inputTokens->items[1]!=NULL){
				// determine start of directory specified
				printf("ls for a specific file\n");
				if((dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster)) != -1){
					directory = (dataRegStart + 512*(( (dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster)) -2)*bpb_information.bpb_secperclus));
					clusterNumber = dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster);
				}
				else
				{	
					printf("%s is not a directory\n", inputTokens->items[1]);
					continue;	
				}

				//printf("directory for ls: %d\n", directory);
			}
			else 
			{
				directory = currDirectory;
				clusterNumber = currDirectoryCluster;
			}

			temp = lseek(fd, currDirectory, SEEK_SET);
			temp2 = read(fd, &empty, 4);
			
			//reading "." for directories not for the root
			if((currDirectory != dataRegStart) && empty != 0){
				temp = lseek(fd, directory, SEEK_SET);
				for (i = 0; i<11; i++){
					temp2 = read(fd, &fileName[i], 1);
				}
				printf("%s\t", fileName);
				counter++;
				temp = lseek(fd, directory+32, SEEK_SET);
				temp2 = read(fd, &empty, 4);
			}

			while(empty != 0){
				empty = 0;
				temp = lseek(fd, currDirectory+clusterBytes, SEEK_SET);

				for(i = 0; i<11; i++){
					temp2 = read(fd, &fileName[i], 1);
				}
				printf("%s\t", fileName);

				temp = lseek(fd, 21, SEEK_CUR);
				temp2 = read(fd, &empty, 4);

				clusterBytes += 64;

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
			if (inputTokens->items[1] == NULL){
				printf("No Directory specified for cd\n");
			}
			else if(strcmp(inputTokens->items[1], "..") == 0 && currDirectory == dataRegStart){
				printf("No directory '..' for root\n");
			}
			else{
				newDirectoryCluster = dir_cluster_num(fd, inputTokens->items[1], currDirectory, dataRegStart, currDirectoryCluster);
				if(newDirectoryCluster == -1)
					printf("%s is not a directory\n", inputTokens->items[1]);
				else 
				{
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
				printf("Creating new file\n");
				newFileCluster = find_empty_cluster(fd);
				temp = lseek(fd, -4, SEEK_CUR);
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
			
            if (inputTokens->size < 3);
                printf("Not enough parametes passed\n");
            else {
                short n = 32;
                int size = 0;
                int type = 0;
                char tempStr[11];
                temp = lseek(fd, currDirectory, SEEK_SET);
                temp2 = read(fd, &empty, 4);
                if(inputTokens->items[1] == NULL){
                    printf("No file specificed for size\n");
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
                
                if (type != 32)
                    printf("%s is not a file\n", inputTokens->items[1]);
                else{
                    temp = lseek(fd, currDirectory+n+28, SEEK_SET);
                    temp2 = read(fd, &size, 4);
                    printf("%d\n", size);
                }	
                    
                    addFile(0, inputTokens->items[2], 0, inputTokens->items[1]);
                    printOpenFiles();
                }
            
            
            
            
            
        }
        if(strcmp(inputTokens->items[0], "close") == 0){

        }
        if(strcmp(inputTokens->items[0], "lseek") == 0){

        }
        if(strcmp(inputTokens->items[0], "read") == 0){

        }
        if(strcmp(inputTokens->items[0], "write") == 0){

        }
        if(strcmp(inputTokens->items[0], "rm") == 0){

        }
        if(strcmp(inputTokens->items[0], "cp") == 0){

        }
        
        if(isCommand(inputTokens->items[0]) == -1){
            printf("%s is not a command\n", inputTokens->items[0]); 
        }

	
		free(input);
		free_tokens(inputTokens);
	} // End of While
	

	if (close(fd) < 0)
	{
		exit(1);
	}  
	printf("Closed the fd. \n");

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


void print_info()
{
    printf("bpb_bytspersec: %d\nbpb_secperclus: %d\nbpb_rsvdseccnt: %d\nbpb_numfats: %d\nbpb_totsec32: %d\nbpb_fatsz32: %d\nbpb_rootclus: %d\n",
    bpb_information.bpb_bytspersec, bpb_information.bpb_secperclus, bpb_information.bpb_rsvdseccnt,
    bpb_information.bpb_numfats, bpb_information.bpb_totsec32, bpb_information.bpb_fatsz32,
    bpb_information.bpb_rootclus);
}


int isCommand(char * userCmd)
{
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
int find_dir_entry(int fd, char* dirEntryName, int curDir)
{
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
            if(dirEntryName[i] == name[i] && i == (strlen(dirEntryName)-1))
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
int dir_cluster_num(int fd, char dirName[11], int curDir, int firstDataLoc, int curCluster)
{
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
    Function: find_empty_cluster
    Reads through the FAT until the first empty cluster is found
*/
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
