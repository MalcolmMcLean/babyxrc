#include <stdio.h>
#include <stdlib.h>

#include "lodepng.h"
#include "adaptivethreshold.h"

int *watershed(unsigned char *grey, int width, int height, int *Nlabels);
unsigned char *sobel(unsigned char *grey, int width, int height);
int *watershed_soille(unsigned char *grey, int width, int height);

unsigned char *labelstorgba(int *labels, int width, int height)
{
   int maxlab = 0;
   int i, j;
   unsigned char *cols = 0;
   unsigned char *answer = 0;

   for (i= 0; i < width * height; i++)
     if (maxlab < labels[i])
       maxlab = labels[i];

   cols = malloc( 3 * (maxlab + 1));
   if (!cols)
     goto out_of_memory;

   for (i= 0; i < maxlab + 1; i++)
   {
     cols[i*3] = rand() % 256;
     cols[i*3+1] = rand() % 256;
     cols[i*3+2] = rand() % 256;
   }
   cols[0] = 255;
   cols[1] = 255;
   cols[2] = 0;

   cols[3] = 0;
   cols[4] = 0;
   cols[5] = 0;
   answer = malloc(width * height * 4);
   if (!answer)
     goto out_of_memory;
   for (i = 0; i <width * height; i++)
   {
     if (labels[i] >= 0)
       j = labels[i];
     else
     {
       printf("bad\n");
       j = 0;
     }
     answer[i*4] = cols[j*3];
     answer[i*4+1] = cols[j*3+1];
     answer[i*4+2] = cols[j*3+2];
     answer[i*4+3] = 255;
   }

   free(cols);
   return answer;

out_of_memory:
   free(cols);
   free(answer);
   return 0;
}

unsigned char *binarytorgba(unsigned char *binary, int width, int height)
{
   unsigned char *rgba = 0;
   int i;

   rgba = malloc(width * height * 4);
   if (!rgba)
     return 0;
   for (i = 0; i < width * height; i++)
   {
      if (binary[i])
      {
         rgba[i*4]  = 255;
         rgba[i*4+1] = 255;
         rgba[i*4+2] = 255;
         rgba[i*4+3] = 255; 
      }
      else
      {
        rgba[i*4] = 0;
        rgba[i*4+1] = 0;
        rgba[i*4+2] = 0;
        rgba[i*4+3] = 255;
      }
   }

   return rgba;
}

unsigned char *greytorgba(unsigned char *grey, int width, int height)
{
   unsigned char *rgba;
   int i;

    rgba = malloc(width * height * 4);
    if (!rgba)
      return 0;

    for (i =0; i < width * height; i++)
    {
       rgba[i*4] = grey[i];
       rgba[i*4+1] = grey[i];
       rgba[i*4+2] = grey[i];
       rgba[i*4+3] = 255;
    }

    return rgba;
}

unsigned char *rgbatogrey(unsigned char *rgba, int width, int height)
{
   int i;
   unsigned char *answer = 0;
   
   answer = malloc(width * height);
   if (!answer)
     return 0;

   for (i=0; i < width * height; i++)
   {
      answer[i] = ((int)rgba[i*4] + rgba[i*4+1] + rgba[i*4+2])/3; 
   }

   return answer;
}


void usage()
{
  printf("Usage : <testfile.png>\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
   unsigned w, h;
   unsigned char *rgba = 0;
   unsigned char *grey = 0;
   unsigned char *out = 0;
   int *labels = 0;
   unsigned err;

   if (argc != 2)
     usage();

   err = lodepng_decode32_file(&rgba, &w, &h, argv[1]);    
   printf( "err %u\n", err);
   if (err)
     exit(EXIT_FAILURE);

   grey = rgbatogrey(rgba, (int) w, (int) h);
   labels = watershed_soille(grey, (int) w, (int) h);
   int Nlabels;
//   labels = watershed(grey, (int) w, (int) h, &Nlabels);
   out = labelstorgba(labels, (int) w, (int) h);

   err = lodepng_encode32_file("test.png", out, w, h);


   unsigned char *binary = adaptivethreshold(grey, (int) w, (int) h, 31, 0.0);
   unsigned char *binout = binarytorgba(binary, (int) w, (int) h);
   lodepng_encode32_file("bin.png", binout, w, h);

   unsigned char *edges = sobel(grey, (int)w,(int) h);
   unsigned char *edgeout = greytorgba(edges, (int) w, (int) h);
   lodepng_encode32_file("sobel.png", edgeout, w, h);

 
   free(rgba);
   free(grey);
   free(labels);
   free(out);

   return 0;
}
