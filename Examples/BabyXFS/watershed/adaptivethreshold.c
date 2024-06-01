#include <stdlib.h>

unsigned char *adaptivethreshold(unsigned char *grey, int width, int height,
int blocksize, double c)
{
    unsigned char *answer;
    int x, y, ix, iy;
    int half =  blocksize/2;

    answer = malloc(width * height);
    if (!answer)
      return 0;
   
    for (y = 0; y < height; y++)
     for (x = 0; x < width; x++)
     {
        double total = 0.0;
        int N = 0;

        for (iy = y - half; iy <= y + half; iy++)
          for (ix = x - half; ix <= x + half; ix++)
          {
            if (iy >= 0 && iy < height && ix >= 0 && ix < width)
            {
               total += grey[iy*width+ix];
               N++;
            }
          } 
       if (grey[y*width+x] > total/N - c)
           answer[y*width+x] = 1;
        else
           answer[y*width+x] = 0;
     }

     return answer;

}
