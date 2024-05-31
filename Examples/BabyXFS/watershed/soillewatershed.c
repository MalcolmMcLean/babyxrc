#include <stdlib.h>
#include <math.h>
#include "adaptivethreshold.h"

unsigned char *sobel(unsigned char *grey, int width, int height);

#define INIT -1
#define MASK -2
#define WSHED 0
#define FICTITIOUS -3

typedef struct WatershedPixel
{
  int x;
  int y;
   unsigned char height;
  int label;
  int dist;
  struct  WatershedPixel *neighbours[8];
  int Nneighbours;

}  WatershedPixel;


typedef struct
{
   WatershedPixel *pix;
   WatershedPixel **watershedStructure;
   int width;
   int height;
   WatershedPixel **queue;
   int qcapacity;
   int  qstart;
   int qend;
   
}  Watershed;


static Watershed *setup(unsigned char *binary, int width, int height);
static void killwatershed(Watershed *w);
static int compf(const void *e1, const void *e2);
static int qpush(Watershed *w, WatershedPixel *p);
static WatershedPixel *qpop(Watershed *w);
static int qempty(Watershed *w);
unsigned char *greytogradient(unsigned char *grey, int width, int height, int boxsize);

int *watershed_soille(unsigned char *grey, int width, int height)
{
    int *answer = 0;
    unsigned char *binary = 0;
    Watershed *w = 0;
    int curlab = 1;
    int heightIndex1 = 0;
    int heightIndex2 = 0;
    int h;

     answer = malloc(width * height * sizeof(int));
     if (!answer)
        goto out_of_memory;

     //binary = adaptivethreshold(grey, width, height, 31, 0.0);
     binary = greytogradient(grey, width, height, 3);
     //binary = sobel(grey, width, height);
     if (!binary)
       goto out_of_memory;

     w = setup(binary, width, height);
     if (!w)
       goto out_of_memory;


     for (h = 0; h <= 255; h++)
     {
        for (int pixelIndex = heightIndex1; pixelIndex < width * height; pixelIndex++)
        {
           WatershedPixel *p = w->watershedStructure[pixelIndex];

           if (p->height != h)
           {
              heightIndex1 = pixelIndex;
              break;
           }
           p->label = MASK;
           
           for (int i = 0; i < p->Nneighbours; i++)
           {
             WatershedPixel *q = p->neighbours[i];
             if (q->label >= 0)
             {
               p->dist = 1;
               qpush(w, p);
               break;
             }
           }
        }

        int curdist = 1;
        
        qpush(w, 0);
        printf("Here qstart %d qend %d\n", w->qstart, w->qend);

        while (1)
        {
           WatershedPixel *p = qpop(w);

           if (p == 0)
           {
              if (qempty(w))
              {
                 break;
              }
              else
              {
                qpush(w, 0);
                curdist++;
                p = qpop(w);
              }
           }
           
           for (int i = 0; i < p->Nneighbours; i++)
           {
              WatershedPixel *q = p->neighbours[i];
              if (q->dist <= curdist && q->label >= 0)
              {
                 if (q->label > 0)
                 {
                    if (p->label == MASK)
                      p->label = q->label;
                    else if (p->label != q->label)
                      p->label = WSHED;
                 }
                 else if (p->label == MASK)
                   p->label = WSHED;
              }
              else if (q->label == MASK && q->dist == 0)
              {
                q->dist = curdist + 1;
                qpush(w, q);
              }
           }
        }
 
        for (int pixelIndex = heightIndex2; pixelIndex < width * height; pixelIndex++)
        {
          WatershedPixel *p = w->watershedStructure[pixelIndex];
          if (p->height != h)
          {
             heightIndex2 = pixelIndex;
             break;
          }
          p->dist = 0;

          if (p->label == MASK)
          {
             curlab++;
             p->label = curlab;
             qpush(w, p);

             while (!qempty(w))
             {
                WatershedPixel *q = qpop(w);

                for (int i = 0; i < q->Nneighbours;i++)
                {
                  WatershedPixel *r = q->neighbours[i];
                  if (r->label == MASK)
                  {
                     r->label = curlab;
                     qpush(w, r);
                  }
                }
             }
          }
        }

     }

     for (int i = 0; i < width * height; i++)
     {
         answer[i] = w->pix[i].label;
     }

     free(binary);
     killwatershed(w);
     return answer;

    out_of_memory:
      if (answer)
        free(answer);
      if (binary)
        free (binary);
      killwatershed(w);
      return 0;
  
}

static Watershed *setup(unsigned char *binary, int width, int height)
{
    Watershed *w;
    int x, y, ix, iy;
    int N;
    
     w = malloc(sizeof(Watershed));
     if (!w)
       goto out_of_memory;
     w->pix = 0;
     w->watershedStructure = 0;
     w->queue = 0;

     w->pix = malloc(width * height * sizeof(WatershedPixel));
     if (!w->pix)
       goto out_of_memory;
     w->watershedStructure = malloc(width * height * sizeof(WatershedPixel *));
     if (!w->watershedStructure)
       goto out_of_memory;
     w->queue  = malloc((width + height) * sizeof(WatershedPixel *));
     if (!w->queue)
       goto out_of_memory;
     w->qcapacity = width + height;
     w->qstart = 0;
     w->qend = 0;
     w->width = width;
     w->height = height;

     for (y=0;y<height;y++)
      for (x=0; x < width; x++)
      {
        w->pix[y*width+x].y = y;
        w->pix[y*width+x].x = x;
        w->pix[y*width+x].height = binary[y*width+x];
        w->pix[y*width+x].label = INIT;
        w->pix[y*width+x].dist = 0;
        
        N = 0;
        for (iy=y-1; iy <= y+1; iy++)
          for (ix = x-1; ix <= x+1; ix++)
          {
             if(iy >= 0 && iy < height && ix >= 0 && ix < width)
             {
               if (ix != x || iy != y)
               {
                 w->pix[y*width+x].neighbours[N] = &w->pix[iy*width+ix];
                 N++;
               }
             }
          }

         w->pix[y*width+x].Nneighbours = N;
         w->watershedStructure[y*width+x] = &w->pix[y*width+x];
      }

      qsort(w->watershedStructure, width *height, sizeof(WatershedPixel *),
        compf);

     return w;

  out_of_memory:
     killwatershed(w);
     return 0;
}

static void killwatershed(Watershed *w)
{
   if (w)
   {
      free(w->pix);
      free(w->watershedStructure);
      free(w->queue);
      free(w);
   }
}

static int compf(const void *e1, const void *e2)
{
   const WatershedPixel *const*w1 = e1;
   const WatershedPixel *const*w2 = e2;
  
   return (*w1)->height - (int) (*w2)->height;
}

static int qpush(Watershed *w, WatershedPixel *p)
{
   int newtop = (w->qend + 1) % w->qcapacity;
   
    if (newtop == w->qstart)
    {
      WatershedPixel **temp;
      temp = realloc(w->queue, (w->qcapacity + w->qcapacity/2) * sizeof(WatershedPixel *));
      if (!temp)
       return -1;
      w->queue = temp;
      w->qcapacity = w->qcapacity + w->qcapacity/2;
    }
    w->queue[w->qend] = p;
    w->qend = newtop;
    
    return 0;
    
}

static WatershedPixel *qpop(Watershed *w)
{
   WatershedPixel *answer;

   if (w->qstart == w->qend)
     return 0;
   answer = w->queue[w->qstart];
   w->qstart = (w->qstart + 1) % w->qcapacity;
   
   return answer;
}

static int qempty(Watershed *w)
{
   if (w->qstart == w->qend)
     return 1;
   else
     return 0;
}

unsigned char *greytogradient(unsigned char *grey, int width, int height, int boxsize)
{
   unsigned char *answer;
   int x, y, xi, yi;
   int half = boxsize/2;

   answer = malloc(width * height);
   if (!answer)
     return 0;

   for (y = 0; y < height; y++)
     for (x = 0; x < width; x ++)
     {
        double total = 0.0;
        int N = 0;
        double d;

         for (yi = y - half; yi <= y + half; yi++)
           for (xi = x - half; xi <= x + half; xi++)
           {
              if (yi >= 0 && yi < height && xi >= 0 && xi < width)
              {
                 total += abs(grey[yi*width+xi] - grey[y*width+x]);
                 N++;
              }  
           }
         d = fabs(total/N);
         if (d > 255)
           d = 255;
         if ( d < 4)
            d = 4;
         answer[y*width+x] = (unsigned char) d; 
     }

   return answer;
}
