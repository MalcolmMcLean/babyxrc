#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "bmp.h"
#include "jpeg.h"
#include "gif.h"
#include "lodepng.h"
#include "loadtiff.h"

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "nanosvg.h"
#include "nanosvgrast.h"

#define FMT_UNKNOWN 0
#define FMT_JPEG 1
#define FMT_PNG 2
#define FMT_BMP 3
#define FMT_GIF 4
#define FMT_TIFF 5
#define FMT_SVG 6

static int getformat(char *fname);
static char *getextension(char *fname);
static void makelower(char *str);
static char *mystrdup(const char *str);

unsigned char *loadrgba(char *fname, int *width, int *height, int *err);

unsigned char *loadasjpeg(char *fname, int *width, int *height);
unsigned char *loadasbmp(char *fname, int *width, int *height);
unsigned char *loadaspng(char *fname, int *width, int *height);
unsigned char *loadasgif(char *fname, int *width, int *height);
unsigned char *loadastiff(char *fname, int *width, int *height);
unsigned char *loadassvg(char *fname, int *width, int *height);

unsigned char *loadrgba(char *fname, int *width, int *height, int *err)
{
  int fmt;
  unsigned char *answer = 0;

  if(err)
    *err = 0;

  fmt = getformat(fname);

  switch(fmt)
  {
  case FMT_UNKNOWN:
    if(err)
      *err = -3;
    return 0;
  case FMT_JPEG:
    answer = loadasjpeg(fname, width, height);
    break;
  case FMT_PNG:
    answer =  loadaspng(fname, width, height);
    break;
  case FMT_BMP:
    answer =  loadasbmp(fname, width, height);
    break;
  case FMT_GIF:
    answer =  loadasgif(fname, width, height);
    break;
  case FMT_TIFF:
	 answer = loadastiff(fname, width, height);
	 break;
  case FMT_SVG:
	  answer = loadassvg(fname, width, height);
	  break;
  }
  if(!answer)
    if(err)
      *err = -1;

  return answer;
}

unsigned char *loadasjpeg(char *fname, int *width, int *height)
{
  unsigned char *rgb;
  int w, h;
  unsigned char *answer;
  int i;

  rgb = loadjpeg(fname, &w, &h);
  if(!rgb)
    return 0;
  answer = malloc(w * h * 4);
  if(!answer)
  {
    free(rgb);
    return 0;
  }
  for(i=0;i<w*h;i++)
  {
    answer[i*4] = rgb[i*3];
    answer[i*4+1] = rgb[i*3+1];
    answer[i*4+2] = rgb[i*3+2];
    answer[i*4+3] = 0xFF;
  }
  free(rgb);
  *width = w;
  *height = h;
  return answer;
}



unsigned char *loadasbmp(char *fname, int *width, int *height)
{
  unsigned char *rgb;
  int w, h;
  unsigned char *answer;
  int i;

  rgb = loadbmp(fname, &w, &h);
  if(!rgb)
    return 0;
  answer = malloc(w * h * 4);
  if(!answer)
  {
    free(rgb);
    return 0;
  }
  for(i=0;i<w*h;i++)
  {
    answer[i*4] = rgb[i*3];
    answer[i*4+1] = rgb[i*3+1];
    answer[i*4+2] = rgb[i*3+2];
    answer[i*4+3] = 0xFF;
  }
  free(rgb);
  *width = w;
  *height = h;
  return answer;
}

unsigned char *loadaspng(char *fname, int *width, int *height)
{
  int err;
  unsigned char *argb;
  unsigned int w, h;
  unsigned char red,green, blue, alpha;
  int i;

  argb = 0;
  err = lodepng_decode32_file(&argb, &w, &h, fname); 
  if(err)
  {
    free(argb);
    return 0;
  }
  /*
  for(i=0;i<w*h;i++)
  {
    red = argb[i*4+1];
    green = argb[i*4+2];
    blue = argb[i*4+3];
    alpha = argb[i*4];
    argb[i*4] = red;
    argb[i*4+1] = green;
    argb[i*4+2] = blue;
    argb[i*4+3] = alpha;
  }
  */
  *width = (int) w;
  *height = (int) h;

  return argb;
}

unsigned char *loadasgif(char *fname, int *width, int *height)
{
  unsigned char *index;
  unsigned char pal[256*3];
  int w, h;
  int i;
  int transparent;
  unsigned char *answer;

  index = loadgif(fname, &w, &h, pal, &transparent);
  if(!index)
    return 0;
  answer = malloc(w * h * 4);
  if(!answer)
  {
    free(index);
    return 0;
  }

  for(i=0;i<w*h;i++)
  {
    answer[i*4] = pal[ index[i] * 3];
    answer[i*4+1] = pal[ index[i] * 3 + 1];
    answer[i*4+2] = pal[ index[i] * 3 + 2];
    if(index[i] == transparent)
      answer[i*4+3] = 0;
    else
      answer[i*4+3] = 0xFF;
  }

  free(index);
  *width = w;
  *height = h;
  return answer;
}

unsigned char *loadastiff(char *fname, int *width, int *height)
{
	unsigned char *rgba;
	FILE *fp;
	long i;

	fp = fopen(fname, "rb");
	if (!fp)
		return 0;
	rgba = floadtiff(fp, width, height);
	fclose(fp);
	if (!rgba)
		return 0;
	for (i = 0; i < *width * *height; i++)
		rgba[i * 4] = 255;

	return rgba;
}

unsigned char *loadassvg(char *fname, int *width, int *height)
{
	struct NSVGimage *image = 0;
	struct NSVGrasterizer *rast = 0;
	unsigned char *img = 0;

	image = nsvgParseFromFile(fname, "px", 96);
	if (!image)
		goto error_exit;
	rast = nsvgCreateRasterizer();
	if (!rast)
		goto error_exit;
    img = malloc(image->width * image->height * 4);
	if (!img)
		goto error_exit;

	// Rasterize
	nsvgRasterize(rast, image, 0, 0, 1, img, image->width, image->height, image->width * 4);

	*width = image->width;
	*height = image->height;
	nsvgDeleteRasterizer(rast);
	nsvgDelete(image);

	return img;
error_exit:
	nsvgDeleteRasterizer(rast);
	nsvgDelete(image);
	free(img);
	return 0;
}

/*
  load an svg file, scaling the image to fit
*/
unsigned char *loadassvgwithsize(char *fname, int width, int height)
{
	struct NSVGimage *image = 0;
	struct NSVGrasterizer *rast = 0;
	unsigned char *img = 0;
	double xscale, yscale;
	double xoffset = 0;
	double yoffset = 0;
	double scale;

	if (width <= 0 || height <= 0)
		goto error_exit;

	image = nsvgParseFromFile(fname, "px", 96);
	if (!image)
		goto error_exit;
	rast = nsvgCreateRasterizer();
	if (!rast)
		goto error_exit;
	img = malloc(width * height * 4);
	if (!img)
		goto error_exit;

	xscale = ((double) width) / image->width;
	yscale = ((double) height) / image->height;
	if (xscale < yscale && yscale)
		xoffset = (width - width * xscale / yscale) / 2;
	if (yscale < xscale && xscale)
		yoffset = (height - height * yscale / xscale) / 2;

	scale = (xscale < yscale) ? xscale : yscale;

	// Rasterize
	nsvgRasterize(rast, image, xoffset, yoffset, scale, img, width, height, width * 4);

	nsvgDeleteRasterizer(rast);
	nsvgDelete(image);

	return img;
error_exit:
	nsvgDeleteRasterizer(rast);
	nsvgDelete(image);
	free(img);
	return 0;
}

static int getformat(char *fname)
{
  char *ext;
  int answer = FMT_UNKNOWN;

  ext = getextension(fname);
  if(!ext)
    return FMT_UNKNOWN;
  makelower(ext);
  if(!strcmp(ext, "jpeg") || !strcmp(ext, "jpg"))
    answer = FMT_JPEG;
  if(!strcmp(ext, "png"))
    answer = FMT_PNG;
  if(!strcmp(ext, "bmp"))
    answer = FMT_BMP;
  if(!strcmp(ext, "gif"))
    answer = FMT_GIF;
  if (!strcmp(ext, "tif") || !strcmp(ext, "tiff"))
	  answer = FMT_TIFF;
  if (!strcmp(ext, "svg"))
	  answer = FMT_SVG;
  
  free(ext);
  return answer;
}

static char *getextension(char *fname)
{
  char *answer;
  
  answer = strrchr(fname, '.');
  if(!answer)
    return 0;
  if(strchr(answer, '/'))
    return 0;
  if(strchr(answer, '\\'))
    return 0;
  
  if(answer == fname)
    return 0;

  return mystrdup(answer+1);  
}

static void makelower(char *str)
{
  int i;

  for(i=0;str[i];i++)
    str[i] = tolower(str[i]);
}
  
static char *mystrdup(const char *str)
{
  char *answer;
 
  answer = malloc(strlen(str) + 1);
  if(answer)
    strcpy(answer, str);

  return answer;
}
