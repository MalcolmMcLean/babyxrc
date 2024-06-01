#include <stdlib.h>
#include <string.h>


typedef struct
{
  int rank;
  int p;
  int size;
} ELEMENT;

typedef struct
{
  ELEMENT *elts;
  int N; 
} UNIVERSE;

static unsigned char *getgradients(unsigned char *grey, int width, int height);
static int placelocalminima(int *answer, unsigned char * gradient, int width, int height);
static void mergeminima(int *answer, unsigned char *gradient, int width, int height);
static void tracewaterdrop(unsigned char *gradient, int *answer, int width, int height, int x, int y);
static int mergelowregions(unsigned char *gradient, int *index, int width, int height, int minval, int Nminina);
static void getneighbours(unsigned char *ret, unsigned char *vals, int width, int height, int x, int y);
static void getneighboursindex(int *ret, int *vals, int width, int height, int x, int y);
static int mini(unsigned char *vals, int N);

static UNIVERSE *universe(int N);
static void killuniverse(UNIVERSE *u);
static int universe_find(UNIVERSE *u, int x);
static void universe_join(UNIVERSE *u, int x, int y);
static int universe_size(UNIVERSE *u, int x);
static int universe_num_sets(UNIVERSE *uni);



/*
  Runs watershed algorithm for image segmentation
  Params: grey - an 8 bit greyscale image
          width - image width
		  height - image height
		  Nsegs - return for number of segments
  Returns: set of segments. No values should be set to zero, Nsegs = number segments +1
*/
int *watershed(unsigned char *grey, int width, int height, int *Nsegs)
{
  unsigned char *gradient = 0;
  int *answer = 0;
  int i, ii;
  int Nminima;
  int minheight = 4;

  gradient = getgradients(grey, width, height);
  if(!gradient)
	  goto error_exit;
  answer = malloc(width * height * sizeof(int));
  if(!answer)
	  goto error_exit;
  memset(answer, 0, width*height*sizeof(int));

  Nminima = placelocalminima(answer, gradient, width, height);
  mergeminima(answer, gradient, width, height);

  for(i=0;i<height;i++)
    for(ii=0;ii<width;ii++)
	  tracewaterdrop(gradient, answer, width, height, ii, i);

  mergelowregions(gradient, answer, width, height, minheight, Nminima);

  free(gradient);
  if(Nsegs)
	  *Nsegs = Nminima;

  return answer;

error_exit:
  free(gradient);
  free(answer);
  if(Nsegs)
    *Nsegs = -1;

  return 0;

}

static unsigned char *getgradients(unsigned char *grey, int width, int height)
{
  unsigned char *answer;
  int x, y;
  unsigned char neighbours[8];
  int i;
  int diff;

  answer = malloc(width * height);
  if(!answer)
	  return 0;

  for(y=0;y<height;y++)
    for(x=0;x<width;x++)
	{
	   diff = 0;
       getneighbours(neighbours, grey, width, height, x, y);
	   for(i=0;i<8;i++)
	     diff += abs(neighbours[i] - grey[y*width+x]);
	   answer[y*width+x] = diff/8;
	   if(answer[y*width+x] == 255)
	     answer[y*width+x] = 254;
	}

  return answer;
}

static int placelocalminima(int *answer, unsigned char *gradient, int width, int height)
{
  int x, y;
  int minimaindex = 1;
  unsigned char neighbours[8];
  int lowest;

  for(y=0;y<height;y++)
	for(x=0;x<width;x++)
	{
	  getneighbours(neighbours, gradient, width, height, x, y);
	  lowest = mini(neighbours, 8);
	  if(gradient[y*width+x] < neighbours[lowest])
	    answer[y*width+x] = minimaindex++;
	  else if(gradient[y*width+x] == neighbours[lowest] && lowest < 4)
	    answer[y*width+x] = minimaindex++;
	}
  return minimaindex;
}

static void mergeminima(int *answer, unsigned char *gradient, int width, int height)
{
  int neighboursi[8];
  unsigned char neighbours[8];
  int flag = 1;
  int x, y;
  int i;

  while(flag)
  {
    flag = 0;
    for(y=0;y<height;y++)
    {
      for(x=0;x<width;x++)
      {
        getneighbours(neighbours, gradient, width, height, x, y);
	getneighboursindex(neighboursi, answer, width, height, x, y);
	for(i=0;i<8;i++)
	{
	  if(gradient[y*width+x] == neighbours[i])
	  {
	    if(neighboursi[i] != 0 && answer[y*width+x] != neighboursi[i])
	    {
	      if(answer[y*width+x] > neighboursi[i])
	      {
	        answer[y*width+x] = neighboursi[i];
		flag = 1;
	      }
	    }
	  }
        }  
      }
    }
    if(!flag)
      continue;

    /* now do the other direction to speed up convergence */
    for(y=height-1;y>=0;y--)
    {
      for(x=width-1;x>=0;x--)
      {
        getneighbours(neighbours, gradient, width, height, x, y);
	getneighboursindex(neighboursi, answer, width, height, x, y);
	for(i=0;i<8;i++)
	{
	  if(gradient[y*width+x] == neighbours[i])
	  {
	    if(neighboursi[i] != 0 && answer[y*width+x] != neighboursi[i])
	    {
	      if(answer[y*width+x] > neighboursi[i])
	      {
	        answer[y*width+x] = neighboursi[i];
		flag = 1;
	      }
	    }
	  }
	}
      }
    }
  }
}

static void tracewaterdrop(unsigned char *gradient, int *answer, int width, int height, int x, int y)
{
	int tx = x;
	int ty = y;
	unsigned char neighbours[8];
    int lowest; 

	while(answer[ty * width + tx] == 0)
	{
       getneighbours(neighbours, gradient, width, height, tx, ty);
	   lowest = mini(neighbours, 8);
	   switch(lowest)
	   {
	         case 0: tx = tx - 1; ty = ty -1; break;
		 case 1: ty = ty -1; break;
		 case 2: tx = tx + 1; ty = ty -1; break;
		 case 3: tx = tx - 1; break;
		 case 4: tx = tx + 1; break;
		 case 5: tx = tx -1; ty = ty+1; break; 
		 case 6: ty = ty + 1; break;
		 case 7: tx = tx + 1, ty = ty + 1; break;
	   }

	}
	answer[y * width + x] = answer[ty * width + tx]; 

}

static int mergelowregions(unsigned char *gradient, int *index, int width, int height, int minval, int Nminima)
{
  int i, ii, iii;
  int oldval, newval;
  int *parent;
  UNIVERSE *uni;

  uni = universe(Nminima);
  if(!uni)
    return -1;
  

  for(i=0;i<height;i++)
    for(ii=0;ii<width-1;ii++)
	  if(index[i*width+ii] != index[i*width+ii+1] && gradient[i*width+ii] < minval && gradient[i*width+ii+1] < minval)
	  {
	    newval = index[i*width+ii];
	    oldval = index[i*width+ii+1];
            universe_join(uni, universe_find(uni, newval), universe_find(uni, oldval));
	  }

  for(i=0;i<width;i++)
    for(ii=0;ii<height-1;ii++)
      if(index[ii*width+i] != index[(ii+1)*width+i] && gradient[ii*width+i] < minval && gradient[(ii+1)*width+i] < minval)
	  {
	    newval = index[ii*width+i];
	    oldval = index[(ii+1)*width+i];
            universe_join(uni, universe_find(uni, newval), universe_find(uni, oldval));
	  }

  for(i=0;i<width*height;i++)
    index[i] = universe_find(uni, index[i]);

  killuniverse(uni);

  return 0;
}

 /*
static void flood(unsigned char *image, int width, int height, int x, int y, int target, int dest)
{
  if(image[y*width+x] != target)
    return;
  image[y*width+x] = dest;
  if(y > 0)
    flood(image, width, height, x, y-1, target, dest);
  if(y < height -1)
    flood(image, width, height, x, y+1, target, dest);
  if(x > 0)
    flood(image, width, height, x-1, y, target, dest);
  if(x < width-1)
    flood(image, width, height, x+1, y, target, dest);
}
 */

static void getneighbours(unsigned char *ret, unsigned char *vals, int width, int height, int x, int y)
{
	ret[0] = x > 0 && y > 0 ? vals[(y-1)*width+(x-1)] : 255;
	ret[1] = y > 0 ? vals[(y-1)*width +x] : 255;
	ret[2] = y > 0 && x < width -1 ? vals[(y-1)*width+x+1] : 255;
	ret[3] = x > 0 ? vals[y*width+x-1] : 255;
	ret[4] = x < width - 1 ? vals[y*width+x+1] : 255;
	ret[5] = y < height -1 && x > 0 ? vals[(y+1)*width+x-1] : 255;
	ret[6] = y < height -1 ? vals[(y+1)*width+x] : 255;
	ret[7] = y < height - 1 && x < width-1 ? vals[(y+1)*width+x+1] : 255;
}

static void getneighboursindex(int *ret, int *vals, int width, int height, int x, int y)
{
	ret[0] = x > 0 && y > 0 ? vals[(y-1)*width+(x-1)] : 0;
	ret[1] = y > 0 ? vals[(y-1)*width +x] : 0;
	ret[2] = y > 0 && x < width -1 ? vals[(y-1)*width+x+1] : 0;
	ret[3] = x > 0 ? vals[y*width+x-1] : 0;
	ret[4] = x < width - 1 ? vals[y*width+x+1] : 0;
	ret[5] = y < height -1 && x > 0 ? vals[(y+1)*width+x-1] : 0;
	ret[6] = y < height -1 ? vals[(y+1)*width+x] : 0;
	ret[7] = y < height - 1 && x < width-1 ? vals[(y+1)*width+x+1] : 0;
}


static int mini(unsigned char *vals, int N)
{
  int answer = 0;
  int i;

  for(i=1;i<N;i++)
	if(vals[i] < vals[answer])
	  answer = i;

  return answer;
}

static UNIVERSE *universe(int N)
{
  UNIVERSE *u = malloc(sizeof(UNIVERSE));
  int i;
  
  if(!u)
    return 0;
  u->N = N;
  u->elts = malloc(N * sizeof(ELEMENT));
  if(!u->elts)
  {
    killuniverse(u);
    return 0;
  }  

  for(i=0;i<N;i++)
  {
    u->elts[i].rank =0;
    u->elts[i].size = 1;
    u->elts[i].p = i;
  }
  u->N = N;

  return u;
}

static void killuniverse(UNIVERSE *u)
{
  if(u)
  {
    free(u->elts);
    free(u);
  }
}

static int universe_find(UNIVERSE *u, int x)
{
  int y = x;

  while(y != u->elts[y].p)
    y = u->elts[y].p;
  u->elts[x].p = y;
  return y;
}

static void universe_join(UNIVERSE *u, int x, int y)
{
  if (u->elts[x].rank > u->elts[y].rank) 
  {
    u->elts[y].p = x;
    u->elts[x].size += u->elts[y].size;
  } 
  else 
  {
    u->elts[x].p = y;
    u->elts[y].size += u->elts[x].size;
    if (u->elts[x].rank == u->elts[y].rank)
      u->elts[y].rank++;
  }
  u->N--;
}

static int universe_size(UNIVERSE *u, int x)
{
  return u->elts[x].size;
}

static int universe_num_sets(UNIVERSE *uni)
{
  return uni->N;
}
