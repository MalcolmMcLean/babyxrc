/*********************************************************
* bmp.c - Microsoft bitmap loading functions.            *
* By Malcolm McLean                                      *
* This is a file from the book Basic Algorithms by       *
*   Malcolm McLean                                       *
*********************************************************/
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
  long pixeldataoffset; /* offset from file start to pixel raster data */
  int width;     
  int height;
  int bits;
  int upside_down; /* set for a bottom-up bitmap */
  int core;        /* set if bitmap is old version */
  int palsize;     /* number of palette entries */
  int compression; /* type of compression in use */
} BMPHEADER;

int bmpgetinfo(char *fname, int *width, int *height);
unsigned char *loadbmp(char *fname, int *width, int *height);
unsigned char *loadbmp8bit(char *fname, int *width, int *height, unsigned char *pal);
unsigned char *loadbmp4bit(char *fname, int *width, int *height, unsigned char *pal);

static void loadpalette(FILE *fp, unsigned char *pal, int entries);
static void loadpalettecore(FILE *fp, unsigned char *pal, int entries);
static void savepalette(FILE *fp, unsigned char *pal, int entries);
static int loadheader(FILE *fp, BMPHEADER *hdr);
static void saveheader(FILE *fp, int width, int height, int bits);
static void loadraster8(FILE *fp, unsigned char *data, int width, int height);
static void loadraster4(FILE *fp, unsigned char *data, int width, int height);
static void loadraster1(FILE *fp, unsigned char *data, int width, int height);
static long getfilesize(int width, int height, int bits);
static void swap(void *x, void *y, int len);
static void fput32le(long x, FILE *fp);
static void fput16le(int x, FILE *fp);
static long fget32le(FILE *fp);
static int fget16le(FILE *fp);


/********************************************************
* bmpgetinfo() - get information about a BMP file.      *
* Params: fname - name of the .bmp file.                *
*         width - return pointer for image width.       *
*         height - return pointer for image height.     *
* Returns: bitmap type.                                 *
*           0 - not a valid bitmap.                     *
*           1 - monochrome paletted (2 entries).        *
*           4 - 4-bit paletted.                         *
*           8 - 8 bit paletted.                         *
*           16 - 16 bit rgb.                            *
*           24 - 24 bit rgb.                            *
*           32 - 24 bit rgb with 1 byte wasted.         *
********************************************************/                             
int bmpgetinfo(char *fname, int *width, int *height)
{
  FILE *fp;
  BMPHEADER bmphdr;

  if (width)
    *width = 0;
  if (height)
    *height = 0;
  fp = fopen(fname, "rb");
  if(!fp)
	return 0;
  if(loadheader(fp, &bmphdr) == -1)
  {
    fclose(fp);
	return 0;
  }
  fclose(fp);
  if(width)
	*width = bmphdr.width;
  if(height)
	*height = bmphdr.height;
  return bmphdr.bits;
}

/************************************************************
* loadbmp() - load any bitmap                               *
* Params: fname - pointer to file path                      *
*         width - return pointer for image width            *
*         height - return pointer for image height          *
* Returns: malloced pointer to image, 0 on fail             *
************************************************************/
unsigned char *loadbmp(char *fname, int *width, int *height)
{
  FILE *fp;
  BMPHEADER bmpheader;
  unsigned char *answer;
  unsigned char *raster = 0;
  unsigned char pal[256 * 3];
  int index;
  int col;
  int target;
  int i;
  int ii;


  fp = fopen(fname, "rb");
  if(!fp)
	return 0;

  if(loadheader(fp, &bmpheader) == -1)
  {
    fclose(fp);
	return 0;
  }
  if(bmpheader.bits == 0 || bmpheader.compression != 0)
  {
	fclose(fp);
	return 0;
  }

  answer = malloc(bmpheader.width * bmpheader.height * 3);
  if(!answer)
  {
    fclose(fp);
	return 0;
  }

  if(bmpheader.bits < 16)
  {
    raster = malloc(bmpheader.width * bmpheader.height);
	if(!raster)
	{
	  free(answer);
	  return 0;
	}
  }

  switch(bmpheader.bits)
  {
    case 1:
	  if(bmpheader.core)
		loadpalettecore(fp, pal, 2);
	  else
		loadpalette(fp, pal, bmpheader.palsize);

          fseek(fp, bmpheader.pixeldataoffset, SEEK_SET);
	  loadraster1(fp, raster, bmpheader.width, bmpheader.height);
	  break;
	case 4:
	  if(bmpheader.core)
		loadpalettecore(fp, pal, 256);
	  else
		loadpalette(fp, pal, bmpheader.palsize);

          fseek(fp, bmpheader.pixeldataoffset, SEEK_SET);
	  loadraster4(fp, raster, bmpheader.width, bmpheader.height);
	  break;
	 
	case 8:
	  if(bmpheader.core)
        loadpalettecore(fp, pal, 256);
      else
	    loadpalette(fp, pal, bmpheader.palsize);
		
          fseek(fp, bmpheader.pixeldataoffset, SEEK_SET);
	  loadraster8(fp, raster, bmpheader.width, bmpheader.height);
	  break;
	  
	case 16:
         fseek(fp, bmpheader.pixeldataoffset, SEEK_SET);
      for(i=0;i<bmpheader.height;i++)
	  {
	    for(ii=0;ii<bmpheader.width;ii++)
		{
		  target = (i * bmpheader.width * 3) + ii * 3;
		  col = fget16le(fp);
		  answer[target] = (col & 0x001F) << 3;
		  answer[target+1] = (col & 0x03E0) >> 2;
		  answer[target+2] = (col & 0x7A00) >> 7;
		}
		while(ii < (bmpheader.width + 1)/2 * 2)
		{
          fgetc(fp);
          fgetc(fp);
		  ii++;
		}
	  }
	  break;

	case 24:
          fseek(fp, bmpheader.pixeldataoffset, SEEK_SET);
	  for(i=0;i<bmpheader.height;i++)
	  {
		for(ii=0;ii<bmpheader.width;ii++)
		{
		  target = (i * bmpheader.width * 3) + ii * 3;
	      answer[target+2] = fgetc(fp);
		  answer[target+1] = fgetc(fp);
		  answer[target+0
                 
] = fgetc(fp);
		}
        if (( bmpheader.width * 3) % 4)
        {
          for (ii=0;ii< 4 - ( (bmpheader.width * 3) % 4); ii++)
          {
            fgetc(fp);
          }
        }
	  }
	  break;

	case 32:
          fseek(fp, bmpheader.pixeldataoffset, SEEK_SET);
	  for(i=0;i<bmpheader.height;i++)
		for(ii=0;ii<bmpheader.width;ii++)
		{
		  target = (i * bmpheader.width * 3) + ii * 3;
		  answer[target+2] = fgetc(fp);
		  answer[target+1] = fgetc(fp);
		  answer[target+0] = fgetc(fp);
		  fgetc(fp);
		}
	  break;
  }

  if(bmpheader.bits < 16)
  {
    for(i=0;i<bmpheader.height;i++)
	  for(ii=0;ii<bmpheader.width;ii++)
	  {
	    target = (i * bmpheader.width * 3) + ii * 3;
		index = raster[i * bmpheader.width + ii] * 3;
		answer[target] = pal[ index ];
		answer[target+1] = pal[ index + 1 ];
		answer[target+2] = pal[ index + 2 ];
	  }  

	free(raster);
  }

  if(bmpheader.upside_down)
  {
    for(i=0;i<bmpheader.height/2;i++)
	  swap( answer + i * bmpheader.width * 3, 
	    answer + (bmpheader.height - i - 1) * bmpheader.width * 3,
		bmpheader.width * 3);
  }

  if(ferror(fp))
  {
    free(answer);
	answer = 0;
  }

  *width = bmpheader.width;
  *height = bmpheader.height;

  fclose(fp);

  return answer;
}

/************************************************************
* loadbmp8bit() - load an 8-bit bitmap.                     *
* Params: fname - pointer to file path.                     *
*         width - return pointer for image width.           *
*         height - return pointer for image height.         *
*         pal - return pointer to 256 rgb palette entries.  *
* Returns: malloced pointer to image data, 0 on fail.       *
************************************************************/
unsigned char *loadbmp8bit(char *fname, int *width, int *height, unsigned char *pal)
{
  BMPHEADER bmphdr;
  FILE *fp;
  unsigned char *answer;
  int i;

  fp = fopen(fname, "rb");
  if(!fp)
	return 0;
  
  if(loadheader(fp, &bmphdr) == -1)
  {
    fclose(fp);
	return 0;
  }
  if(bmphdr.bits != 8)
  {
    fclose(fp);
	return 0;
  }
  if(bmphdr.compression != 0)
  {
    fclose(fp);
	return 0;
  }
  if(bmphdr.core)
    loadpalettecore(fp, pal, 256);
  else
	loadpalette(fp, pal, bmphdr.palsize);
  answer = (unsigned char *) malloc(bmphdr.width * bmphdr.height);
  if(!answer)
  {
    fclose(fp);
	return 0;
  }

  fseek(fp, bmphdr.pixeldataoffset, SEEK_SET);
  loadraster8(fp, answer, bmphdr.width, bmphdr.height);
  if(bmphdr.upside_down)
  {
    for(i=0;i<bmphdr.height/2;i++)
	  swap(answer + i * bmphdr.width, 
	    answer + (bmphdr.height - i - 1) * bmphdr.width, 
		bmphdr.width);
  }

  if(ferror(fp))
  {
    free(answer);
	answer = 0;
  }

  fclose(fp);
  *width = bmphdr.width;
  *height = bmphdr.height;

  return answer;
}

/************************************************************
* loadbmp4bit() - load a 4-bit bitmap from disk.            *
* Params: fname - pointer to the file path.                 *
*         width - return pointer for image width.           *
*         height - return pointer for image height.         *
*         pal - return pointer for 16 rgb palette entries.  *
* Returns: malloced pointer to 4-bit image data.            *
************************************************************/
unsigned char *loadbmp4bit(char *fname, int *width, int *height, unsigned char *pal)
{
  BMPHEADER bmphdr;
  FILE *fp;
  unsigned char *answer;
  int i;

  fp = fopen(fname, "rb");
  if(!fp)
	return 0;
 
  if(loadheader(fp, &bmphdr) == -1)
  {
    fclose(fp);
	return 0;
  }
  if(bmphdr.bits != 4)
  {
    fclose(fp);
	return 0;
  }
  if(bmphdr.compression != 0)
  {
    fclose(fp);
	return 0;
  }

  if(bmphdr.core)
    loadpalettecore(fp, pal, 16);
  else
	loadpalette(fp, pal, bmphdr.palsize);
  answer = (unsigned char *) malloc(bmphdr.width * bmphdr.height);
  if(!answer)
  {
    fclose(fp);
	return 0;
  }
  fseek(fp, bmphdr.pixeldataoffset, SEEK_SET);
  loadraster4(fp, answer, bmphdr.width, bmphdr.height);

  if(bmphdr.upside_down)
  {
    for(i=0;i<bmphdr.height/2;i++)
	{
	  swap(answer + i * bmphdr.width,
		answer + (bmphdr.height - i - 1) * bmphdr.width,
		bmphdr.width);
    }
  }

  if(ferror(fp))
  {
    free(answer);
	answer = 0;
  }
  
  fclose(fp);
  *width = bmphdr.width;
  *height = bmphdr.height;
  return answer;
}

/***********************************************************
* save a24-bit bmp file.                                   *
* Params: fname - name of file to save.                    *
*         rgb - raster data in rgb format                  *
*         width - image width                              *
*         height - image height                            *
* Returns: 0 on success, -1 on fail                        *
***********************************************************/             
int savebmp(char *fname, unsigned char *rgb, int width, int height)
{
  FILE *fp;
  int i;
  int ii;

  fp = fopen(fname, "wb");
  if(!fp)
	return -1;

  saveheader(fp, width, height, 24);
  for(i=0;i<height;i++)
  {
	for(ii=0;ii<width;ii++)
	{
      fputc(rgb[2], fp);
	  fputc(rgb[1], fp);
	  fputc(rgb[0], fp);
	  rgb += 3;
	}
	if(( width * 3) % 4)
	{
	  for(ii=0;ii< 4 - ( (width * 3) % 4); ii++)
	  {
	    fputc(0, fp);
	  }
	}
  }

  if(ferror(fp))
  {
    fclose(fp);
	return -1;
  }

  return fclose(fp);
}

/**********************************************************
* save an 8-bit palettised bitmap .                       *
* Params: fname - the name of the file.                   *
*         data - the raster data                          *
*         width - image width                             *
*         height - image height                           *
*         pal - palette (RGB format)                      *
* Returns: 0 on success, -1 on failure                    *
**********************************************************/
int savebmp8bit(char *fname, unsigned char *data, int width, int height, unsigned char *pal)
{
  FILE *fp;
  int i;
  int ii;

  fp = fopen(fname, "wb");
  if(!fp)
	return -1;

  saveheader(fp, width, height, 8);
  savepalette(fp, pal, 256);
  
  for(i=0;i<height;i++)
	for(ii=0;ii< (width + 3)/4;ii++)
	{
	  fputc(data[i*width + ii * 4], fp);
	  if(ii * 4 + 1 < width)
		fputc(data[i*width + ii * 4 + 1], fp);
	  else
		fputc(0, fp);
	  if(ii * 4 + 2 < width)
		fputc(data[i*width + ii * 4 + 2], fp);
	  else
		fputc(0, fp);
	  if(ii * 4 + 3 < width)
		fputc(data[i*width + ii * 4 + 3], fp);
	  else
		fputc(0, fp);
	}

  if(ferror(fp))
  {
    fclose(fp);
	return -1;
  }

  return fclose(fp);
}

/*******************************************************
* save a 4-bit palettised bitmap.                      *
* Params: fname - the name of the file.                *
*         data - raster data                           *
*         width - image width                          *
*         height - image height                        *
*         pal - the palette (RGB format)               *
* Returns: 0 on success, -1 on failure                 *
*******************************************************/ 
int savebmp4bit(char *fname, unsigned char *data, int width, int height, unsigned char *pal)
{
  FILE *fp;
  int i;
  int ii;
  int pix;

  fp = fopen(fname, "wb");
  if(!fp)
	return -1;

  saveheader(fp, width, height, 4);
  savepalette(fp, pal, 16);

  for(i=0;i<height;i++)
    for(ii=0;ii< (width + 7)/8;ii++)
	{
      pix = data[i * width + ii * 8] << 4;
	  if(ii * 8 + 1 < width)
		pix |= data[i * width + ii * 8 + 1];
	  fputc(pix, fp);

	  pix = 0;
	  if(ii * 8 + 2 < width)
		pix = data[ i * width + ii * 8 + 2] << 4;
	  if(ii * 8 + 3 < width)
		pix |= data[ i * width + ii * 8 + 3];
	  fputc(pix, fp);

	  pix = 0;
	  if(ii * 8 + 4 < width)
		pix = data[ i * width + ii * 8 + 4] << 4;
	  if(ii * 8 + 5 < width)
		pix |= data[i * width + ii * 8 + 5];
	  fputc(pix, fp);

	  pix = 0;
	  if(ii * 8 + 6 < width)
		pix = data[ i * width + ii * 8 + 6] << 4;
	  if(ii * 8 + 7 < width)
		pix |= data[i * width + ii * 8 + 7];
	  fputc(pix, fp);
	}

  if(ferror(fp))
  {
    fclose(fp);
	return -1;
  }

  return fclose(fp);
}

/***************************************************************
* save a 2-bit palettised bitmap.                              *
* Params: fname - name of file to write.                       *
*         data - raster data, one byte per pixel.              *
*         width - image width.                                 *
*         height - image height.                               *
*         pal - the palette (0 = black/white)                  *
* Returns: 0 on success, -1 on fail.                           *
***************************************************************/ 
int savebmp1bit(char *fname, unsigned char *data, int width, int height, unsigned char *pal)
{
  FILE *fp;
  unsigned char defpal[6] = {0, 0, 0, 255, 255, 255 };
  int i;
  int ii;
  int iii;
  int pix;

  fp = fopen(fname, "wb");
  if(!fp)
	return -1;
  
  saveheader(fp, width, height, 1);
  if(pal)
	savepalette(fp, pal, 2);
  else
	savepalette(fp, defpal, 2);

  for(i=0;i<height;i++)
	for(ii=0;ii<width;ii+=32)
	{
	  pix = 0;
      for(iii=0;iii<8;iii++)
		if(ii + iii < width)
			pix |= data[i * width + ii + iii] ? (1 << (7-iii) ) : 0;
	  fputc(pix, fp);

      pix = 0;
	  for(iii=0;iii<8;iii++)
		if(ii + iii + 8 < width)
			pix |= data[i * width + ii + iii + 8] ? (1 << (7 - iii)) : 0;
	  fputc(pix, fp);

	  pix = 0;
	  for(iii=0;iii<8;iii++)
		if(ii + iii + 16 < width)
			pix |= data[i * width + ii + iii + 16] ? (1 << (7 - iii)) : 0;
      fputc(pix, fp);

	  pix = 0;
	  for(iii=0;iii<8;iii++)
		if(ii + iii + 24 < width)
			pix |= data[i * width + ii + iii + 24] ? (1 << (7 - iii)) : 0;

      fputc(pix, fp);
	}
  
  if(ferror(fp))
  {
    fclose(fp);
	return -1;
  }

  return fclose(fp);
}

/**************************************************************
* loadpalette() - load palette for a new format BMP.          *
* Params: fp - pointer to an open file.                       *
*         pal - return pointer for palette entries.           *
*         entries - number of entries in palette.             *
**************************************************************/
static void loadpalette(FILE *fp, unsigned char *pal, int entries)
{
  int i;
  for(i=0;i<entries;i++)
  {
    pal[2] = fgetc(fp);
	pal[1] = fgetc(fp);
	pal[0] = fgetc(fp);
	fgetc(fp);
	pal += 3;
  }
}

/******************************************************
* loadpalettecore() - load a palette for a core BMP   *
* Params: fp - pointer to an open file.               *
*         pal - return pointer for palette entries.   *
*         entries - number of entries to read.        *
******************************************************/
static void loadpalettecore(FILE *fp, unsigned char *pal, int entries)
{
  int i;
  for(i=0;i<entries;i++)
  {
    pal[2] = fgetc(fp);
	pal[1] = fgetc(fp);
	pal[0] = fgetc(fp);
	pal += 3;
  }
}

/*************************************************************
* saves a palette                                            *
* Params: fp - pointer to an open file                       *
*         pal - the palette                                  *
*         entries - number of palette entries.               *
*************************************************************/
static void savepalette(FILE *fp, unsigned char *pal, int entries)
{
  int i;

  for(i=0;i<entries;i++)
  {
    fputc(pal[2], fp);
	fputc(pal[1], fp);
	fputc(pal[0], fp);
	fputc(0, fp);
	pal += 3;
  }
}

/*
typedef struct tagBITMAPFILEHEADER { // bmfh 
    WORD    bfType; 
    DWORD   bfSize; 
    WORD    bfReserved1; 
    WORD    bfReserved2; 
    DWORD   bfOffBits; 
} BITMAPFILEHEADER; 

typedef struct tagBITMAPINFOHEADER{ // bmih 
    DWORD  biSize; 
    LONG   biWidth; 
    LONG   biHeight; 
    WORD   biPlanes; 
    WORD   biBitCount 
    DWORD  biCompression; 
    DWORD  biSizeImage; 
    LONG   biXPelsPerMeter; 
    LONG   biYPelsPerMeter; 
    DWORD  biClrUsed; 
    DWORD  biClrImportant; 
} BITMAPINFOHEADER;

*/

/******************************************************
* loadheader() - load the bitmap header information.  *
* Params: fp - pinter to an opened file.              *
*         hdr - return pointer for header information.*
* Returns: 0 on success, -1 on fail.                   *
******************************************************/
static int loadheader(FILE *fp, BMPHEADER *hdr)
{
  int size;
  int hdrsize;
  int id;
  int i;

  id = fget16le(fp);
  /* is it genuinely a BMP ? */
  if(id != 0x4D42)
	return -1;
  /* skip rubbish */
  fget32le(fp);
  fget16le(fp);
  fget16le(fp);
  /* offset to bitmap bits */
  hdr->pixeldataoffset = fget32le(fp);
  
  hdrsize = fget32le(fp);
  if(hdrsize == 40)
  {
    hdr->width = fget32le(fp);
	hdr->height = fget32le(fp);
	fget16le(fp);
    hdr->bits = fget16le(fp);
	hdr->compression = fget32le(fp);
	/* skip rubbish */
    for(i=0;i<12;i++)
	  fgetc(fp);
	hdr->palsize = fget32le(fp);
	if(hdr->palsize == 0 && hdr->bits < 16)
	  hdr->palsize = 1 << hdr->bits;
	fget32le(fp);
	if(hdr->height < 0)
	{
	  hdr->upside_down = 0;
	  hdr->height = -hdr->height;
	}
	else
	  hdr->upside_down = 1;
    hdr->core = 0;
  }
  else if(hdrsize == 12)
  {
    hdr->width = fget16le(fp);
	hdr->height = fget16le(fp);
	fget16le(fp);
	hdr->bits = fget16le(fp);
	hdr->compression = 0;
	hdr->upside_down = 1;
	hdr->core = 1;
	hdr->palsize = 1 << hdr->bits;
  }
  else
	return 0;
  if(ferror(fp))
	return -1;
  return 0;
}

/****************************************************************
* write a bitmap header.                                        *
* Params: fp - pointer to an open file.                         *
*         width - bitmap width                                  *
*         height - bitmap height                                *
*         bit - bit depth (1, 4, 8, 16, 24, 32)                 *
****************************************************************/
static void saveheader(FILE *fp, int width, int height, int bits)
{
  long sz;
  long offset;

  /* the file header */
  /* "BM" */
  fputc(0x42, fp);
  fputc(0x4D, fp);

  /* file size */
  sz = getfilesize(width, height, bits) + 40 + 14;
  fput32le(sz, fp);

  /* reserved */
  fput16le(0, fp);
  fput16le(0, fp);
  /* offset of raster data from header */
  if(bits < 16)
    offset = 40 + 14 + 4 * (1 << bits);
  else
	offset = 40 + 14;
  fput32le(offset, fp);

  /* the infoheader */

  /* size of structure */
  fput32le(40, fp);
  fput32le(width, fp);
  /* height negative because top-down */
  fput32le(-height, fp);
  /* bit planes */
  fput16le(1, fp);
  fput16le(bits, fp);
  /* compression */
  fput32le(0, fp);
  /* size of image (can be zero) */
  fput32le(0, fp);
  /* pels per metre */
  fput32le(600000, fp);
  fput32le(600000, fp);
  /* colours used */
  fput32le(0, fp);
  /* colours important */
  fput32le(0, fp);
}

/*********************************************************************
* load 8-bit raster data                                             *
* Params: fp - pointer to an open file.                              *
*         data - return pointer for data (one byte per pixel)        *
*         width - image width                                        *
*         height - image height                                      *
*********************************************************************/
static void loadraster8(FILE *fp, unsigned char *data, int width, int height)
{
  int linewidth;
  int i;
  int ii;

  linewidth = (width + 3)/4 * 4;

  for(i=0;i<height;i++)
  {
    for(ii=0;ii<width;ii++)
	  *data++ = fgetc(fp);
	while(ii < linewidth)
	{
	  fgetc(fp);
	  ii++;
	}
  }
}

/**********************************************************************
* load 4-bit raster data                                              *
* Params: fp - pointer to an open file.                               *
*         data - return pointer for data (one byte per pixel)         *
*         width - image width                                         *
*         height - iamge height                                       *
**********************************************************************/ 
static void loadraster4(FILE *fp, unsigned char *data, int width, int height)
{
  int linewidth;
  int i;
  int ii;
  int pix;

  linewidth = (((width + 1)/2) + 3)/4 * 4;
  
  for(i=0;i<height;i++)
  {
    for(ii=0;ii<linewidth;ii++)
	{
	  pix = fgetc(fp);
	  if(ii * 2 < width)
		*data++ = pix >> 4;
	  if(ii * 2 + 1 < width)
		*data++ = pix & 0x0F;
    }
  }
}

/*******************************************************************
* load 1 bit raster data                                           *
* Params: fp - pointer to an open file.                            *
*         data - return pointer for data (one byte per pixel)      *
*         width - image width.                                     *
*         height - image height.                                   *
*******************************************************************/
static void loadraster1(FILE *fp, unsigned char *data, int width, int height)
{
  int linewidth;
  int i;
  int ii;
  int iii;
  int pix;

  linewidth = ((width + 7)/8 + 3)/4 * 4;

  for(i=0;i<height;i++)
  {
	for(ii=0;ii<linewidth;ii++)
	{
	  pix = fgetc(fp);
	  if(ii * 8 < width)
	  {
	    for(iii=0;iii<8;iii++)
	      if(ii * 8 + iii < width)
		    *data++ = (pix & (1 << (7 - iii))) ? 1 : 0;
	  }
	}
  }

}

/*****************************************************
*  get the size of the file to be written.           *
* Params: width - image width                        *
*         height - image height                      *
*         bits - image type                          *
* Returns: size of image data (excluding headers)    *
*****************************************************/
static long getfilesize(int width, int height, int bits)
{
  long answer = 0;
  switch(bits)
  {
    case 1:
	  answer = (width + 7)/8;
	  answer = (answer + 3)/4 * 4;
	  answer *= height;
	  answer += 2 * 4;
	  break;
    case 4:
	  answer = 16 * 4 + (width + 1)/2;
	  answer = (answer + 3)/4 * 4;
	  answer *= height;
	  answer += 16 * 4;
	  break;
	case 8:
	  answer = (width + 3)/4 * 4;
	  answer *= height;
	  answer += 256 * 4;
	  break;
	case 16:
	  answer = (width * 2 + 3)/4 * 4;
	  answer *= height;
	  break;
	case 24:
	  answer = (width * 3 + 3)/4 * 4;
	  answer *= height;
	  break;
	case 32:
	  answer = width * height * 4;
	  break;
	default:
	  return 0;
  }

  return answer;
}

/***************************************************************
* swap an area of memory                                       *
* Params: x - pointer to first buffer                          *
*         y - pointer to second buffer                         *
*         len - length of memory to swap                       *
***************************************************************/
static void swap(void *x, void *y, int len)
{
  unsigned char *ptr1 = x;
  unsigned char *ptr2 = y;
  unsigned char temp;
  int i;

  for(i=0;i<len;i++)
  {
    temp = ptr1[i];
	ptr1[i] = ptr2[i];
	ptr2[i] = temp;
  }
}

/***************************************************************
* write a 32-bit little-endian number to a file.               *
* Params: x - the number to write                              *
*         fp - pointer to an open file.                        *
***************************************************************/
static void fput32le(long x, FILE *fp)
{
  fputc(x & 0xFF, fp);
  fputc( (x >> 8) & 0xFF, fp);
  fputc( (x >> 16) & 0xFF, fp);
  fputc( (x >> 24) & 0xFF, fp);
}

/***************************************************************
* write a 16-bit little-endian number to a file.               *
* Params: x - the nmuber to write                              *
*         fp - pointer to an open file                         *
***************************************************************/                
static void fput16le(int x, FILE *fp)
{
  fputc(x & 0xFF, fp);
  fputc( (x >> 8) & 0xFF, fp);
}

/***************************************************************
* fget32le() - read a 32 bit little-endian number from a file. *
* Params: fp - pointer to an open file.                        *
* Returns: value read as a signed integer.                     *
***************************************************************/
static long fget32le(FILE *fp)
{
	int c1, c2, c3, c4;

	c1 = fgetc(fp);
	c2 = fgetc(fp);
	c3 = fgetc(fp);
	c4 = fgetc(fp);
	return ((c4 ^ 128) - 128) * 256 * 256 * 256 + c3 * 256 * 256 + c2 
* 256 + c1;
}

/***************************************************************
* fget16le() - read a 16 bit little-endian number from a file. *
* Params: fp - pointer to an open file.                        *
* Returns: value read as a signed integer.                     *
***************************************************************/
static int fget16le(FILE *fp)
{
	int c1, c2;

	c1 = fgetc(fp);
	c2 = fgetc(fp);

	return ((c2 ^ 128) - 128) * 256 + c1;
}

