#include "walkway.h"
#include <stdio.h>

int main(int argc, char **argv)
{
  int i, ii, iii, iv, v;
  int ch;
  int alpha;
  long offset = 0;
 
  if (argc < 2)
  {
     printf("Enter some text to display\n");
     return -1; 
  }
  for (i =1; i < argc; i++)
  {
     for (ii = 0; argv[i][ii]; ii++)
     {
        ch = argv[i][ii];
        offset = 0;
        for (iii =0; iii < walkway_font.Nchars; iii++)
        {
           if (walkway_font.index[iii] == ch)
              break;
        } 
        if (iii == walkway_font.Nchars)
            continue;
        offset = walkway_font.width * walkway_font.height *iii;
        for (iv = 0; iv < walkway_font.height;iv++)
        {
           for (v = 0; v < walkway_font.widths[iii]; v++)
           {
               alpha = walkway_font.bitmap[offset + iv * 
walkway_font.width + v];
               if (alpha < 128)
                 printf(" ");
               else
                 printf("#"); 
           }
           printf("\n");
        } 
        printf("\n");
     }
  }
  return 0;
} 

