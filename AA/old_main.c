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
#include "hex_helper.h"

typedef struct bpb_info_struct
{
  unsigned short bpb_bytspersec, bpb_secperclus,
                bpb_rsvdseccnt, bpb_numfats;
  unsigned int bpb_fatsz32, bpb_rootclus, bpb_totsec32;
}Bpb_info_struct;

struct DIRENTRY                 //useful for DIRENTRY* parameter for read/write
{
    unsigned char DIR_name[11];
    unsigned char DIR_Attributes;
} __attribute__((packed));      //use __attribute__((packed)) in order to avoid attribute padding

Bpb_info_struct bpb_information;

void info(int fd);
void ls(int fd);

//helper functions
void print_info();

int main(){

    int fd = open("fat32.img", O_RDONLY);
    if (fd == -1)
        printf("Error opening file");
    info(fd);

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
    }

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

  print_info();
}

void print_info()
{
    printf("bpb_bytspersec: %d\nbpb_secperclus: %d\nbpb_rsvdseccnt: %d\nbpb_numfats: %d\nbpb_totsec32: %d\nbpb_fatsz32: %d\nbpb_rootclus: %d\n",
    bpb_information.bpb_bytspersec, bpb_information.bpb_secperclus, bpb_information.bpb_rsvdseccnt,
    bpb_information.bpb_numfats, bpb_information.bpb_totsec32, bpb_information.bpb_fatsz32,
    bpb_information.bpb_rootclus);
}
