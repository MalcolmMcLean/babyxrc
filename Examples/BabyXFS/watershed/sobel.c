#include <stdlib.h>
#include <string.h>
#include <math.h>

unsigned char *sobel(unsigned char *grey, int width, int height)
{
   int Gx[3][3] = {
   {-1, 0, 1},
   {-2, 0, 2},
   {-1, 0, 1},
};
   int Gy[3][3] = {
   {-1, -2, -1},
   {0, 0, 0},
   {1, 2, 1}, 
};

   unsigned char *answer;
   int S1, S2;
   int x, y, ix, iy;
   double mag;

   answer = malloc(width * height);
   if (!answer)
     return 0;
   memset(answer, 0, width * height);

   for (y = 0; y < height-2; y++)
   {
      for (x = 0; x < width-2; x++)
      {
         S1 = 0;
         S2 = 0;

         for (iy=0;iy<3;iy++)
           for (ix=0;ix<3;ix++)
           {
             S1 += grey[(y+iy)*width+x+ix] * Gx[iy][ix];
             S2 += grey[(y+iy)*width+x+ix] * Gy[iy][ix];
           }
         mag = sqrt(S1*S1 + S2*S2);
         if (mag > 255)
           mag = 255;
         answer[(y+1)*width+x+1] = (unsigned char) mag; 
      }
   }

   return answer;
}
