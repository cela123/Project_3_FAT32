#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "hex_helper.h"

void decimalToHexadecimal(int dec)
{
    size_t start_size = 1000; //figure out way to change
    //printf("start_size: %d\n", start_size);
    char hexaNum[start_size];
    for(int i = 0; dec != 0; i++)
    {
        int temp = 0;
        temp = dec % 16;

        if(temp < 10)
        {
            hexaNum[i] = temp + 48;
        }
        else
        {
            hexaNum[i] = temp + 55;
        }


        dec = dec / 16;
    }

    strrev(hexaNum);
    printf("hexaNum: %s\n", hexaNum);
}

char *strrev(char *str)
{
      char *p1, *p2;

      if (! str || ! *str)
            return str;
      for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
      {
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
      }
      return str;
}
