#include <stdio.h>
#include <stdlib.h>

typedef struct
{
	int width;
	int height;
	unsigned char *rgba;
	int hotx;
	int hoty;
} BBX_CURSOR;


typedef struct
{
	int width;
	int height;
	int bits;
	int upside_down; /* set for a bottom-up bitmap */
	int core;        /* set if bitmap is old version */
	int palsize;     /* number of palette entries */
	int compression; /* type of compression in use */
} BMPHEADER;

/*
struct tagANIHeader {
	DWORD cbSizeOf; // Num bytes in AniHeader (36 bytes)
	DWORD cFrames; // Number of unique Icons in this cursor
	DWORD cSteps; // Number of Blits before the animation cycles
	DWORD cx, cy; // reserved, must be zero.
	DWORD cBitCount, cPlanes; // reserved, must be zero.
	DWORD JifRate; // Default Jiffies (1/60th of a second) if rate chunk not present.
	DWORD flags; // Animation Flag (see AF_ constants)
} ANIHeader;
*/

typedef struct
{
	int size;
	int cframes;
	int csteps;
	int cx;
	int cy;
	int cbitcount;
	int cplanes;
	int jifrate;
	long flags;
} ANIHEADER;

BBX_CURSOR *floadanimatedcursor(FILE *fp, int *err);

BBX_CURSOR *floadcursor(FILE *fp, int *err);
void killcursor(BBX_CURSOR *cur);
static int loadaniheader(FILE *fp, ANIHEADER *hdr);
static int loaddibheader(FILE *fp, BMPHEADER *hdr);
static void loadpalette(FILE *fp, unsigned char *pal, int entries);
static void loadpalettecore(FILE *fp, unsigned char *pal, int entries);
static void loadraster1(FILE *fp, unsigned char *data, int width, int height);
static void loadraster4(FILE *fp, unsigned char *data, int width, int height);
static void loadraster8(FILE *fp, unsigned char *data, int width, int height);
static void loadraster16(FILE *fp, unsigned char *data, int width, int height);
static void loadraster24(FILE *fp, unsigned char *data, int width, int height);
static void loadraster32(FILE *fp, unsigned char *data, int width, int height);
static void rgba_turnupsidedown(unsigned char *rgba, int width, int height);
static int fget16le(FILE *fp);
static long fget32le(FILE *fp);

/*
"RIFF" {Length of File}
"ACON"
"LIST" {Length of List}
"INAM" {Length of Title} {Data}
"IART" {Length of Author} {Data}
"fram"
"icon" {Length of Icon} {Data}; 1st in list
...
"icon" {Length of Icon} {Data}; Last in list(1 to cFrames)
"anih" {Length of ANI header(36 bytes)} {Data}; (see ANI Header TypeDef)
"rate" {Length of rate block} {Data}; ea.rate is a long(length is 1 to cSteps)
"seq " {Length of sequence block} {Data}; ea.seq is a long(length is 1 to cSteps)

- END -

-Any of the blocks("ACON", "anih", "rate", or "seq ") can appear in any order.I've never seen "rate" or "seq " appear before "anih", though. You need the cSteps value from "anih" to read "rate" and "seq". The order I usually see the frames is: "RIFF", "ACON", "LIST", "INAM", "IART", "anih", "rate", "seq ", "LIST", "ICON". You can see the "LIST" tag is repeated and the "ICON" tag is repeated once for every embedded icon. The data pulled from the "ICON" tag is always in the standard 766-byte .ico file format (for the 16-color animated cursors only).

- All{ Length of... } are 4byte DWORDs.

- ANI Header TypeDef :

struct tagANIHeader {
	DWORD cbSizeOf; // Num bytes in AniHeader (36 bytes)
	DWORD cFrames; // Number of unique Icons in this cursor
	DWORD cSteps; // Number of Blits before the animation cycles
	DWORD cx, cy; // reserved, must be zero.
	DWORD cBitCount, cPlanes; // reserved, must be zero.
	DWORD JifRate; // Default Jiffies (1/60th of a second) if rate chunk not present.
	DWORD flags; // Animation Flag (see AF_ constants)
} ANIHeader;

#define AF_ICON =3D 0x0001L // Windows format icon/cursor animation

*/

BBX_CURSOR *loadanimatedcursor(char *filename, int *err)
{
	FILE *fp;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		if (err)
			*err = -2;
		return 0;
	}
	floadanimatedcursor(fp, err);
	fclose(fp);

	return 0;

}

BBX_CURSOR *floadanimatedcursor(FILE *fp, int *err)
{
	char tag[5];
	long flen;
	long blen;
	long i;
	int iconerror = 0;
	ANIHEADER aniheader;

	tag[4] = 0;
	fread(tag, 1, 4, fp);
	printf("%s\n", tag);
	if (strncmp(tag, "RIFF", 4))
		goto parse_error;
	flen = fget32le(fp);
	fread(tag, 1, 4, fp);
	if (strncmp(tag, "ACON", 4))
		goto parse_error;
	

	while (flen > 0)
	{
		if (fread(tag, 1, 4, fp) == 0)
		{
			printf("clean quit\n");
			break;
		}
		printf("%s\n", tag);
		
		if (!strncmp(tag, "LIST"))
		{
			blen = fget32le(fp);
			fget32le(fp);
			flen -= 8;
			continue;
		}

		blen = fget32le(fp);
		printf("len %ld\n", blen);
		if (!strcmp(tag, "anih"))
		{
			loadaniheader(fp, &aniheader);
		}
		else if (!strcmp(tag, "icon"))
		{

			floadcursor(fp, &iconerror);
			if (iconerror)
				goto parse_error;
			if (i % 2)
				fgetc(fp);
		}
		else
		{
			for (i = 0; i < blen; i++)
				fgetc(fp);
			if (i % 2)
				fgetc(fp);
		}
		flen -= blen + 8 + (blen % 2);
		printf("flen %ld\n", flen);
		if (feof(fp))
		{
			printf("bad\n");
			break;
		}
	}
	return 0;

parse_error:
	return 0;
}

BBX_CURSOR *loadcursor(char *filename, int *err)
{
	FILE *fp;
	BBX_CURSOR *cur;

	fp = fopen(filename, "rb");
	if (!fp)
		return 0;

	cur = floadcursor(fp, err);

	fclose(fp);
	return cur;
}

BBX_CURSOR *floadcursor(FILE *fp, int *err)
{
	BBX_CURSOR *cur;
	int magic;
	int type;
	int N;
	int width, height;
	int Ncol;
	int hotx, hoty;
	long bmpsize;
	long bmpoffset;
	unsigned char *data8 = 0;
	unsigned char *bitmask = 0;

	BMPHEADER bmpheader;
	unsigned char pal[256 * 3];
	unsigned char *rgba = 0;

	cur = malloc(sizeof(BBX_CURSOR));
	if (!cur)
		goto out_of_memory;

	magic = fget16le(fp);
	if (magic != 0)
		goto parse_error;
	type = fget16le(fp);
	if (type != 2)
		goto parse_error;
	N = fget16le(fp);
	width = fgetc(fp);
	height = fgetc(fp);
	Ncol = fgetc(fp);
	magic = fgetc(fp);
	hotx = fget16le(fp);
	hoty = fget16le(fp);
	bmpsize = fget32le(fp);
	bmpoffset = fget32le(fp);


	loaddibheader(fp, &bmpheader);
	
	
	data8 = malloc(width * height);
	if (!data8)
		goto out_of_memory;
	bitmask = malloc(width * height);
	if (!bitmask)
		goto out_of_memory;

	rgba = malloc(width * height * 4);
	if (!rgba)
		goto out_of_memory;

	if(bmpheader.core)
		loadpalettecore(fp, pal, 4);
	else
       loadpalette(fp, pal, bmpheader.palsize);
 
	switch (bmpheader.bits)
	{
		case 1:
			loadraster1(fp, data8, width, height);
			loadraster1(fp, bitmask, width, height);
			break;
		case 4:
			loadraster4(fp, data8, width, height);
			loadraster1(fp, bitmask, width, height);
			break;
		case 8:
			loadraster8(fp, data8, width, height);
			loadraster1(fp, bitmask + width *height, width, height);
			break;
		case 16:
			loadraster16(fp, rgba, width, height);
			loadraster1(fp, bitmask, width, height);
			break;
		case 24:
			loadraster24(fp, rgba, width, height);
			loadraster1(fp, bitmask, width, height);
			break;
		case 32:
			loadraster32(fp, rgba, width, height);
			loadraster1(fp, bitmask, width, height);
			break;
	}

	int i;
	if (bmpheader.bits < 16)
	{
		for (i = 0; i < width*height; i++)
		{
			rgba[i * 4] = pal[data8[i]*3];
			rgba[i * 4+1] = pal[data8[i]*3+1];
			rgba[i * 4+2] = pal[data8[i]*3+2];
		}
	}
	for (i = 0; i < width*height; i++)
	{
		rgba[i * 4 + 3] = bitmask[i] ? 0x0 : 0xFF;
	}

	if (bmpheader.upside_down)
		rgba_turnupsidedown(rgba, width, height);

	cur->height = height;
	cur->width = width;
	cur->hotx = hotx;
	cur->hoty = hoty;
	cur->rgba = rgba;
	
	return cur;


parse_error:
	free(data8);
	free(bitmask);
	free(rgba);
	free(cur);
	//killcursor(cur);
	if (err)
		*err = -3;
	return 0;

out_of_memory:
	killcursor(cur);
	if (err)
		*err = -1;
	return 0;
}

void killcursor(BBX_CURSOR *cur)
{
	if (cur)
	{
		free(cur->rgba);
		free(cur);
	}
}




static int loadaniheader(FILE *fp, ANIHEADER *hdr)
{
	int i;

	hdr->size = fget32le(fp);
	hdr->cframes = fget32le(fp);
	hdr->csteps = fget32le(fp);
	hdr->cx = fget32le(fp);
	hdr->cy = fget32le(fp);
	hdr->cbitcount = fget32le(fp);
	hdr->cplanes = fget32le(fp);
	hdr->jifrate = fget32le(fp);
	hdr->flags = fget32le(fp);

	for (i = 36; i < hdr->size; i++)
	   fgetc(fp);

  return 0;
}
/******************************************************
* loadheader() - load the bitmap header information.  *
* Params: fp - pinter to an opened file.              *
*         hdr - return pointer for header information.*
* Returns: 0 on success, -1 on fail.                   *
******************************************************/
static int loaddibheader(FILE *fp, BMPHEADER *hdr)
{
	int size;
	int hdrsize;
	int id;
	int i;

	hdrsize = fget32le(fp);
	if (hdrsize == 40)
	{
		hdr->width = fget32le(fp);
		hdr->height = fget32le(fp);
		fget16le(fp);
		hdr->bits = fget16le(fp);
		hdr->compression = fget32le(fp);
		/* skip rubbish */
		for (i = 0; i<12; i++)
			fgetc(fp);
		hdr->palsize = fget32le(fp);
		if (hdr->palsize == 0 && hdr->bits < 16)
			hdr->palsize = 1 << hdr->bits;
		fget32le(fp);
		if (hdr->height < 0)
		{
			hdr->upside_down = 0;
			hdr->height = -hdr->height;
		}
		else
			hdr->upside_down = 1;
		hdr->core = 0;
	}
	else if (hdrsize == 12)
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
	if (ferror(fp))
		return -1;
	return 0;
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
	for (i = 0; i<entries; i++)
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
	for (i = 0; i<entries; i++)
	{
		pal[2] = fgetc(fp);
		pal[1] = fgetc(fp);
		pal[0] = fgetc(fp);
		pal += 3;
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

	linewidth = ((width + 7) / 8 + 3) / 4 * 4;

	for (i = 0; i<height; i++)
	{
		for (ii = 0; ii<linewidth; ii++)
		{
			pix = fgetc(fp);
			if (ii * 8 < width)
			{
				for (iii = 0; iii<8; iii++)
					if (ii * 8 + iii < width)
						*data++ = (pix & (1 << (7 - iii))) ? 1 : 0;
			}
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

	linewidth = (((width + 1) / 2) + 3) / 4 * 4;

	for (i = 0; i<height; i++)
	{
		for (ii = 0; ii<linewidth; ii++)
		{
			pix = fgetc(fp);
			if (ii * 2 < width)
				*data++ = pix >> 4;
			if (ii * 2 + 1 < width)
				*data++ = pix & 0x0F;
		}
	}
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

	linewidth = (width + 3) / 4 * 4;

	for (i = 0; i<height; i++)
	{
		for (ii = 0; ii<width; ii++)
			*data++ = fgetc(fp);
		while (ii < linewidth)
		{
			fgetc(fp);
			ii++;
		}
	}
}

static void loadraster16(FILE *fp, unsigned char *data, int width, int height)
{
	int i, ii;

	int target;
	int col;

	for (i = 0; i < height; i++)
	{
		for (ii = 0; ii < width; ii++)
		{
			target = (i * width * 3) + ii * 3;
			col = fget16le(fp);
			data[target] = (col & 0x001F) << 3;
			data[target + 1] = (col & 0x03E0) >> 2;
			data[target + 2] = (col & 0x7A00) >> 7;
		}
		while (ii < (width + 1) / 2 * 4)
		{
			fgetc(fp);
			ii++;
		}
	}
}


static void loadraster24(FILE *fp, unsigned char *data, int width, int height)
{
	int i, ii;
	int target;

	for (i = 0; i < height; i++)
	{
		for (ii = 0; ii < width; ii++)
		{
			target = (i * width * 3) + ii * 3;
			data[target] = fgetc(fp);
			data[target + 1] = fgetc(fp);
			data[target + 2] = fgetc(fp);
		}
		while (ii < (width + 3) / 4 * 4)
		{
			fgetc(fp);
			ii++;
		}
	}
}

static void loadraster32(FILE *fp, unsigned char *data, int width, int height)
{
	int i, ii;
	int target;

	for (i = 0; i < height; i++)
		for (ii = 0; ii < width; ii++)
		{
			target = (i * width * 4) + ii * 4;
			data[target+2] = fgetc(fp);
			data[target + 1] = fgetc(fp);
			data[target + 0] = fgetc(fp);
			data[target + 3] = fgetc(fp);
		}
}

static void rgba_turnupsidedown(unsigned char *rgba, int width, int height)
{
	int i, ii;
	unsigned char temp;
	unsigned char *pup, *pdown;

	for (i = 0; i < height / 2; i++)
	{
		pup = rgba + i * width * 4;
		pdown = rgba + (height - i - 1) * width * 4;
		for (ii = 0; ii < width * 4; ii++)
		{
			temp = pup[ii];
			pup[ii] = pdown[ii];
			pdown[ii] = temp;
		}
	}
}


static int fget16le(FILE *fp)
{
	int c1, c2;

	c1 = fgetc(fp);
	c2 = fgetc(fp);

	return ((c2 ^ 128) - 128) * 256 + c1;
}

static long fget32le(FILE *fp)
{
	int c1, c2, c3, c4;

	c1 = fgetc(fp);
	c2 = fgetc(fp);
	c3 = fgetc(fp);
	c4 = fgetc(fp);
	return ((c4 ^ 128) - 128) * 256 * 256 * 256 + c3 * 256 * 256 + c2 * 256 + c1;
}
