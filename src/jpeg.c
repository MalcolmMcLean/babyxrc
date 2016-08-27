/*
  This is a file from the book Basic Algorithms by Malcolm McLean

  JPEG loader
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "jpeg.h"

#define clamp(x, low, high) ((x) < (low) ? (low) : (x) > (high) ? (high) : (x))

/* Huffman tree */
typedef struct huffnode
{
  struct huffnode *child0;   /* child for '0' */
  struct huffnode *child1;   /* child for '1' */
  int symbol;                /* symbol (leaves only) */
  int suppbits;              /* unused extra bits in Huffman code (leaves only) */
} HUFFNODE;

/* information about JPEG file */
typedef struct
{
  int width;                 /* width in pixels */
  int height;                /* height in pixels */
  int qttable[4][64];        /* four qunatisation tables */
  HUFFNODE *dctable[4];      /* four dc Huffman trees */
  HUFFNODE *actable[4];      /* four ac Huffman trees */
  int Ncomponents;           /* number of components in image (up to 4) */
  int component_type[4];     /* type of each 1 = Y, 2 = Cb, 3 = Cr, 4 = I, 5 = Q */
  int vsample[4];            /* sampling rate for vertical */
  int hsample[4];            /* sampling rate for horizontal */
  int usedc[4];              /* index of dc Huffman tree to use */
  int useac[4];              /* index of ac Huffman tree to use */
  int useq[4];               /* index of quantisation tbale to use */
  int dri;                   /* restart interval */

} JPEGHEADER;

/* read bits from files */
typedef struct
{
  FILE *fp;        /* file pointer */
  int rack;        /* character just read */
  int bit;         /* maks for bit to read */
} BITSTREAM;



/*
    SOF0  = $c0   Start Of Frame (baseline JPEG), for details see below
	SOF1  = $c1   dito
	SOF2  = $c2   usually unsupported
	SOF3  = $c3   usually unsupported

	SOF5  = $c5   usually unsupported
	SOF6  = $c6   usually unsupported
	SOF7  = $c7   usually unsupported

	SOF9  = $c9   for arithmetic coding, usually unsupported
	SOF10 = $ca   usually unsupported
	SOF11 = $cb   usually unsupported

	SOF13 = $cd   usually unsupported
	SOF14 = $ce   usually unsupported
	SOF14 = $ce   usually unsupported
	SOF15 = $cf   usually unsupported

	DHT   = $c4   Define Huffman Table, for details see below
	JPG   = $c8   undefined/reserved (causes decoding error)
	DAC   = $cc   Define Arithmetic Table, usually unsupported

   *RST0  = $d0   RSTn are used for resync, may be ignored
   *RST1  = $d1
   *RST2  = $d2
   *RST3  = $d3
   *RST4  = $d4
   *RST5  = $d5
   *RST6  = $d6
   *RST7  = $d7

	SOI   = $d8   Start Of Image
	EOI   = $d9   End Of Image
	SOS   = $da   Start Of Scan, for details see below
	DQT   = $db   Define Quantization Table, for details see below
	DNL   = $dc   usually unsupported, ignore

	SOI   = $d8   Start Of Image
	EOI   = $d9   End Of Image
	SOS   = $da   Start Of Scan, for details see below
	DQT   = $db   Define Quantization Table, for details see below
	DNL   = $dc   usually unsupported, ignore
	DRI   = $dd   Define Restart Interval, for details see below
	DHP   = $de   ignore (skip)
	EXP   = $df   ignore (skip)

	APP0  = $e0   JFIF APP0 segment marker, for details see below
	APP15 = $ef   ignore

	JPG0  = $f0   ignore (skip)
	JPG13 = $fd   ignore (skip)
	COM   = $fe   Comment, for details see below

*/

#define SOF0  0xC0 /*  Start Of Frame (baseline JPEG) */
#define SOF1  0xC1 /*   ditto */
#define DHT   0xC4 /*  Define Huffman Table */
#define DQT   0xDB /*  Define Quantization Table */
#define DRI   0xDD /*  Define Restart Interval */
#define APP0  0xE0 /*  JFIF APP0 segment marker */
#define SOI   0xD8 /*  Start Of Image */
#define EOI   0xD9 /*  End Of Image */
#define SOS   0xDA /*  Start Of Scan, for details see below */
#define COMMENT 0xFE /* comment section */

static JPEGHEADER *loadheader(FILE *fp);
static void killheader(JPEGHEADER *hdr);
static int loadscan(JPEGHEADER *hdr, unsigned char *buff, FILE *fp);
static int loadscanmono(JPEGHEADER *hdr, unsigned char *buff, FILE *fp);
static int loadscanYuv(JPEGHEADER *hdr, unsigned char *buff, FILE *fp);
static int loadscanYuv111(JPEGHEADER *hdr, unsigned char *buff, FILE *fp);
static void pasteblock(unsigned char *buff, int width, int height, unsigned char *block, int x, int y);
static void reconstitute(unsigned char *block, short *lum1, short *lum2, short *lum3, short *lum4, short *cr, short *cb);
static int loadsoi(FILE *fp);
static int loadeoi(FILE *fp);
static int startofframe(JPEGHEADER *hdr, FILE *fp);
static int loadapp0(FILE *fp);
static int dri(JPEGHEADER *hdr, FILE *fp);
static int dqt(JPEGHEADER *hdr, FILE *fp,int length);
static int dht(JPEGHEADER *hdr, FILE *fp, int length);
static int startofscan(JPEGHEADER *hdr, FILE *fp, int length);
static int skipsegment(FILE *fp, int length);
static int segmentheader(FILE *fp, int *size);

static void idct8x8(short *block);
static void fastidct8(short *x);
static void unzigzag(short *block);
static int getblock(short *ret, HUFFNODE *dctree, HUFFNODE *tree, BITSTREAM *bs);

static HUFFNODE *buildhufftree(int *codelength, unsigned char *symbols);
static void buildtreerec(HUFFNODE *first, HUFFNODE *last, char **code, unsigned char *symbol, int N, int bit);
static char **buildcanonical(int *codelength);
static char *inttocode(int code, int length);
static int tree_getsymbol(HUFFNODE *node, BITSTREAM *bs);

static BITSTREAM *bitstream(FILE *fp);
static void killbitstream(BITSTREAM *bs);
static int readmarker(BITSTREAM *bs);
static int getsymbol(BITSTREAM *bs, int bits);
static int getbit(BITSTREAM *bs);

static int fget16(FILE *fp);


/*
  load a JPEG file.
  Params: path - nmae of file to load
          width - return pointer for file width
		  height - return pointer for file height
  Returns: image in rgb format, 0 on fail.
*/
unsigned char *loadjpeg(const char *path, int *width, int *height)
{
  FILE *fp;
  unsigned char *answer;
  JPEGHEADER *header;

  *width = -1;
  *height = -1;

  fp = fopen(path, "rb");
  if(!fp)
    return 0;

  header = loadheader(fp);
  if(!header)
  {
    fclose(fp);
	return 0;
  }

  answer = malloc(header->width * header->height * 3);
  if(!answer)
  {
    killheader(header);
	return 0;
  }
  if( loadscan(header, answer, fp) == -1)
  {
    killheader(header);
	free(answer);
    return 0;
  }

  fclose(fp);

  *width = header->width;
  *height = header->height;

  killheader(header);

  return answer;
}

/*
  load information about the JPEG file.
  Params: fp - pointer to file
  Returns: the header, or NULL on fail.
*/
static JPEGHEADER *loadheader(FILE *fp)
{
  JPEGHEADER *answer;
  int length;
  int seg;
  int i;
  
  answer = malloc(sizeof(JPEGHEADER));
  if(!answer)
    return 0;

  for(i=0;i<4;i++)
  {
	answer->dctable[i] = 0;
    answer->actable[i] =0;
  }
  answer->dri = 0;

  if( loadsoi(fp) == -1 )
    goto error_exit;

  /*
  seg = segmentheader(fp, &length);
  while(seg == COMMENT)
  {
    printf("seg %x\n", seg);
    skipsegment(fp, length);
    seg = segmentheader(fp, &length);
  }
  printf("seg %x\n", seg);
  if(seg != APP0)
	goto error_exit;

  if( loadapp0(fp) == -1)
    goto error_exit;
  printf("Loaded app0\n");
  */

  do
  {
        seg = segmentheader(fp, &length);
	switch(seg)
	{
	  case SOF0:
	  case SOF1:
	    if( startofframe(answer, fp) == -1 )
		  goto error_exit;
		break;
	  case DRI:
	    dri(answer, fp);
		//goto error_exit;
		break;
	  case DHT:
	    if( dht(answer, fp, length) == -1)
		  goto error_exit;
		break;
	  case DQT:
	    if( dqt(answer, fp, length) == -1)
		  goto error_exit;
		break;
	  case SOS:
		if( startofscan(answer, fp, length) == -1)
		  goto error_exit;
		break;
	  default:
	    if( skipsegment(fp, length) == -1)
	      goto error_exit;
		break;
	}
  } while(seg != SOS); 

  return answer;

error_exit:
  killheader(answer);
  return 0;
}

/*
  destructor for JPEH header structure
  Params: hdr - structure to destroy.
*/
static void killheader(JPEGHEADER *hdr)
{
  int i;
  if(hdr)
  {
	for(i=0;i<4;i++)
	{
	  if(hdr->dctable[i])
        free( hdr->dctable[i] );
	  if(hdr->actable[i])
	    free( hdr->actable[i] );
	}
    free(hdr);
  }
}

/*
  load the raster scan for a JPEG image.
  Params: hdr - the JPEG header
          buff - output buffer (3 * width * height)
		  fp - poiter to an open file.
  Returns: 0 on success, -1 on fail
  Notes: throws out unusual JPEGS with components in odd
    order, etc.
*/
static int loadscan(JPEGHEADER *hdr, unsigned char *buff, FILE *fp)
{
  /* monochrome jPEG */
  if(hdr->Ncomponents == 1)
  {
    if(hdr->component_type[0] != 1)
	  return -1;
	if(hdr->vsample[0] != 1)
	  return -1;
	if(hdr->hsample[0] != 1)
	  return -1;
	return loadscanmono(hdr, buff, fp);
  }
  /* colour JPEG */
  else if(hdr->Ncomponents == 3)
  {
    if(hdr->component_type[0] != 1)
	  return -1;
	if(hdr->component_type[1] != 2)
	  return -1;
	if(hdr->component_type[2] != 3)
	  return -1;

	if(
	   hdr->hsample[0] == 2 && hdr->vsample[0] == 2 &&
	   hdr->hsample[1] == 1 && hdr->vsample[1] == 1 &&
	   hdr->hsample[2] == 1 && hdr->vsample[2] == 1)
	return loadscanYuv(hdr, buff, fp);
	if(hdr->hsample[0] == 1 && hdr->vsample[0] == 1 &&
	   hdr->hsample[1] == 1 && hdr->vsample[1] == 1 &&
	   hdr->hsample[2] == 1 && hdr->vsample[2] == 1)
	return loadscanYuv111(hdr, buff, fp);
	else
	  return -1;
  }
  else
    return -1;
}

/*
  load raster data for monochrome JPEG.
  Params hdr - header information
         buff - output buffer (24-bit rgb)
		 fp - pointer to an open file
  Returns: 0 on success, -1 on fail.
*/
static int loadscanmono(JPEGHEADER *hdr, unsigned char *buff, FILE *fp)
{
  BITSTREAM *bs;
  short lum[64];
  int target;
  int i;
  int ii;
  int iii;
  int iv;
  int diffdc = 0;
  int ac;
  int dc;

  bs = bitstream(fp);
  if(!bs)
  {
	return -1;
  }

  dc = hdr->usedc[0];
  ac = hdr->useac[0];

  for(i=0;i<hdr->height;i+=8)
    for(ii=0;ii<hdr->width;ii+=8)
	{
      getblock(lum, hdr->dctable[dc], hdr->actable[ac], bs);
	  lum[0] += diffdc;
	  diffdc = lum[0];
	  for(iii=0;iii<64;iii++)
		lum[iii] *= hdr->qttable[0][iii];
	  unzigzag(lum);
	  idct8x8(lum);
	  for(iii=0;iii<8;iii++)
	  {
		if(i + iii >= hdr->height)
		  break;
		for(iv=0;iv<8;iv++)
		{
	      if(ii + iv >= hdr->width)
			break;
		  target = (i + iii) * hdr->width * 3 + (ii + iv) * 3;
		  buff[ target ] = lum[iii*8+iv] / 64 + 128;
		  buff[ target+1] = lum[iii*8+iv] / 64 + 128;
		  buff[ target+2] = lum[iii*8+iv] / 64 + 128;
		}
	  }
	}
  killbitstream(bs);

  if(loadeoi(fp) == 0)
	return 0;
  else
	return -1;
}

/*
  load JPEG Yuv colur raster data
  Params: hdr - the JPEG header
          buff - output buffer
		  fp - pointer to an open file
  Returns: 0 on success, -1 on fail
*/
static int loadscanYuv(JPEGHEADER *hdr, unsigned char *buff, FILE *fp)
{
  BITSTREAM *bs;
  short lum[4][64];
  short cr[64];
  short cb[64];
  unsigned char *block;
  int i;
  int ii;
  int iii;
  int iv;
  int diffdc = 0;
  int dcr = 0;
  int dcb = 0;
  int actableY;
  int actableCb;
  int actableCr;
  int dctableY;
  int dctableCb;
  int dctableCr;
  int count = 0;


  block = malloc(16*16*3);
  bs = bitstream(fp);

  actableY = hdr->useac[0];
  actableCb = hdr->useac[1];
  actableCr = hdr->useac[2];
  dctableY = hdr->usedc[0];
  dctableCb = hdr->usedc[1];
  dctableCr = hdr->usedc[2];

  for(i=0;i<hdr->height;i+=16)
  {
	for(ii=0;ii<hdr->width;ii+=16)
    {
	  if(hdr->dri && (count % hdr->dri) == 0 && count > 0 )
	  {
		readmarker(bs);
		diffdc = 0;
		dcb = 0;
		dcr = 0;
	  }

	  for(iii=0;iii<4;iii++)
	  {
        getblock(lum[iii], hdr->dctable[dctableY], hdr->actable[actableY], bs);
		lum[iii][0] += diffdc;
		diffdc = lum[iii][0];
		
		for(iv=0;iv<64;iv++)
		  lum[iii][iv] *= hdr->qttable[hdr->useq[0]][iv];
		unzigzag(lum[iii]);
		idct8x8(lum[iii]);
		
	  }
	  
	  getblock(cb, hdr->dctable[dctableCb], hdr->actable[actableCb], bs);
	  cb[0] += dcb;
	  dcb = cb[0];
	  for(iv=0;iv<64;iv++)
		cb[iv] *= hdr->qttable[hdr->useq[1]][iv];
	  idct8x8(cb);
	  unzigzag(cb);
	 

	  getblock(cr, hdr->dctable[dctableCr], hdr->actable[actableCr], bs);
	  cr[0] += dcr;
	  dcr = cr[0];
	  for(iv=0;iv<64;iv++)
		cr[iv] *= hdr->qttable[hdr->useq[2]][iv];
	  unzigzag(cr);
	  idct8x8(cr);

	  reconstitute(block, lum[0], lum[1], lum[2], lum[3], cr, cb);
	  pasteblock(buff, hdr->width, hdr->height, block, ii, i);

	  count++;
    }
  }

  killbitstream(bs);
  free(block);


  if(loadeoi(fp) == 0)
	return 0;

  return -1;
}

static int loadscanYuv111(JPEGHEADER *hdr, unsigned char *buff, FILE *fp)
{
  short lum[64];
  short Cb[64];
  short Cr[64];
  BITSTREAM *bs;
  int i;
  int ii;
  int iii;
  int iv;
  int diffdc = 0;
  int dcb = 0;
  int dcr = 0;
  int actableY;
  int actableCb;
  int actableCr;
  int dctableY;
  int dctableCb;
  int dctableCr;
  int count = 0;
  int target;
  int luminance;
  int red;
  int green;
  int blue;

  actableY = hdr->useac[0];
  actableCb = hdr->useac[1];
  actableCr = hdr->useac[2];
  dctableY = hdr->usedc[0];
  dctableCb = hdr->usedc[1];
  dctableCr = hdr->usedc[2];

  bs = bitstream(fp);

  for(i=0;i<hdr->height;i+=8)
    for(ii=0;ii<hdr->width;ii+=8)
	{
	  if(hdr->dri && (count % hdr->dri) == 0 && count > 0 )
	  {
		readmarker(bs);
		diffdc = 0;
		dcb = 0;
		dcr = 0;
	  }

      getblock(lum, hdr->dctable[dctableY], hdr->actable[actableY], bs);
	  lum[0] += diffdc;
	  diffdc = lum[0];
		
	  for(iv=0;iv<64;iv++)
		lum[iv] *= hdr->qttable[hdr->useq[0]][iv];
	  unzigzag(lum);
	  idct8x8(lum);
	  
	  getblock(Cb, hdr->dctable[dctableCb], hdr->actable[actableCb], bs);
	  Cb[0] += dcb;
	  dcb = Cb[0];
		
	  for(iv=0;iv<64;iv++)
		Cb[iv] *= hdr->qttable[hdr->useq[1]][iv];
	  unzigzag(Cb);
	  idct8x8(Cb);
	  
	  getblock(Cr, hdr->dctable[dctableCr], hdr->actable[actableCr], bs);
	  Cr[0] += dcr;
	  dcr = Cr[0];
		
	  for(iv=0;iv<64;iv++)
		Cr[iv] *= hdr->qttable[hdr->useq[2]][iv];
	  unzigzag(Cr);
	  idct8x8(Cr);

	  for(iii=0;iii<8;iii++)
	  {
		if( i + iii >= hdr->height)
		  break;
		for(iv=0;iv<8;iv++)
		{
		  if(ii + iv >= hdr->width)
			break;
          target = (i + iii) * hdr->width * 3 + (ii + iv) * 3;
		  luminance = lum[iii*8+iv]/64 + 128;
		  red = (int) (luminance + 1.402  * Cr[iii*8+iv]/64);
	      green = (int) (luminance - 0.34414 * Cb[iii*8+iv]/64 - 0.71414 * Cr[iii*8+iv]/64);
          blue = (int) (luminance + 1.772  * Cb[iii*8+iv]/64);
		  red = clamp(red, 0, 255);
		  green = clamp(green, 0, 255);
		  blue = clamp(blue, 0, 255);
		  buff[target] = red;
		  buff[target+1] = green;
		  buff[target+2] = blue;
		}
	  }

	  count++;
    }
  
  killbitstream(bs);
  if(loadeoi(fp) == 0)
	return 0;

  return -1;
}


/*
  paste a 16 * 16 block to the output buffer.
  Parmas: buff - output buffer
          width - output width
		  height - output height
		  block - 16 * 16 block
		  x - x corodinate to paste
		  y - y co-ordinate to paste
*/
static void pasteblock(unsigned char *buff, int width, int height, unsigned char *block, int x, int y)
{
  int i;
  int ii;
  int target;

  for(i=0;i<16;i++)
	for(ii=0;ii<16;ii++)
	{
	  if(i + y >= height)
		continue;
	  if(ii + x >= width)
		continue;
	  target = (y + i) * width * 3 + (x + ii) * 3;
	  buff[ target ] = block[i*16*3+ii*3];
	  buff[target + 1] = block[i*16*3+ii*3+1];
	  buff[target + 2] = block[i*16*3+ii*3+2];
	}
}

/*
  reconstitute blcok from Yuv parameters.
  Parmas: block - 16 8 16 rgb output block.
  lum1 - 64 luminance coefficients
  lum2 - 64 luminance coefficients
  lum3 - 64 luminance coefficients
  lum4 - 64 luminance coefficients
  cr - red chrominance
  cb - blue chrominance

*/
static void reconstitute(unsigned char *block, short *lum1, short *lum2, short *lum3, short *lum4, short *cr, short *cb)
{
  int i;
  int ii;
  int Cr;
  int Cb;
  int luminance;
  double red;
  double green;
  double blue;

  for(i=0;i<16;i++)
	for(ii=0;ii<16;ii++)
	{
	  if(i < 8 && ii < 8)
		luminance = lum1[i*8+ii];
	  if(i < 8 && ii >= 8)
		luminance = lum2[i*8+ii-8];
	  if(i>=8 && ii < 8)
		luminance = lum3[(i-8)*8+ii];
	  if(i>=8 && ii >=8)
		luminance = lum4[(i-8)*8+ii-8];
	  luminance = luminance/64 + 128;
	  Cr = cr[i/2 * 8 + ii/2]/64 + 128;
	  Cb = cb[i/2 * 8 + ii/2]/64 + 128;
	  red = luminance + 1.402  * (Cr-128);
	  green = luminance - 0.34414 * (Cb-128) - 0.71414*(Cr-128);
      blue = luminance + 1.772  *(Cb-128);

	  if(red < 0)
		red = 0;
	  if(red > 255)
		red = 255;
	  if(green < 0)
		green = 0;
	  if(green > 255)
		green = 255;
	  if(blue < 0)
		blue = 0;
	  if(blue > 255)
		blue = 255;

	  block[i*16*3+ii*3] = (unsigned char) red;
	  block[i*16*3+ii*3+1] = (unsigned char) green;
	  block[i*16*3+ii*3+2] = (unsigned char) blue;
	}
}


/*
  load the SOI (start of information) marker
  Params: fp - pointer to an open file
  Returns: 0 if marker read, -1 on fail 
*/
static int loadsoi(FILE *fp)
{
  if(fgetc(fp) != 0xFF)
    return -1;
  if(fgetc(fp) != 0xD8)
    return -1;
  return 0;
}

/*
  load the EOI (end of infromation) marker
  Parmas: fp - pointer to an open file
  Returns: 0 if marker read, -1 of fail
*/
static int loadeoi(FILE *fp)
{
  int ch;

  if(fgetc(fp) != 0xFF)
    return -1;
  while( (ch = fgetc(fp)) == 0xFF )
	continue;
  if(ch != 0xD9)
    return -1;
  return 0;
}
  
/*
 process start of frame segment
 Params: hdr - the JPEG header
         fp - open file
 Returns: 0 on success, -1 on fail
*/
static int startofframe(JPEGHEADER *hdr, FILE *fp)
{
  int precision;
  int sampling;
  int i;

  precision = fgetc(fp);
  if(precision != 8)
	return -1;
  hdr->height = fget16(fp);
  hdr->width = fget16(fp);
  hdr->Ncomponents = fgetc(fp);

  if(hdr->Ncomponents < 1 || hdr->Ncomponents > 4)
	return -1;

  for(i=0;i<hdr->Ncomponents;i++)
  {
    hdr->component_type[i] = fgetc(fp);
	if(hdr->component_type[i] < 1 || hdr->component_type[i] > 5)
	  return -1;
    sampling = fgetc(fp);
	hdr->vsample[i] = sampling & 0x0F;
	hdr->hsample[i] = sampling >> 4;
    hdr->useq[i] = fgetc(fp);
	if(hdr->useq[i] < 0 || hdr->useq[i] > 3)
	  return -1;
  }

  return 0;
}

/*
  load the app0 chunk of the JPEG file
  Params: fp - pointer to an open file
  Returns: 0 on success, -1 on fail
  Notes: chunk may contain a thumbnail, which we ignore
   basically we are just looking for the the version
   of the JPEG IFF file.

*/
static int loadapp0(FILE *fp)
{
  int major_revision;
  int minor_revision;
  int density_units;
  int xdensity;
  int ydensity;
  int thumbwidth;
  int thumbheight;
  int i;
  int ii;

  /* 'JFIF'#0 ($4a, $46, $49, $46, $00), identifies JFIF */


 if(fgetc(fp) != 0x4a)
   return -1;
 if(fgetc(fp) != 0x46)
   return -1;
 if(fgetc(fp) != 0x49)
   return -1;
 if(fgetc(fp) != 0x46)
   return -1;
 if(fgetc(fp) != 0)
   return -1;

  major_revision = fgetc(fp);
  if(major_revision != 1)
	return -1;
  minor_revision = fgetc(fp);
  density_units = fgetc(fp);
  xdensity = fget16(fp);
  ydensity = fget16(fp);
  thumbwidth = fgetc(fp);
  thumbheight = fgetc(fp);

  for(i=0;i<thumbheight;i++)
	for(ii=0;ii<thumbwidth;ii++)
	{
	  fgetc(fp);
	  fgetc(fp);
	  fgetc(fp);
	}
  
  return 0;
}

/*
  read restart interval (ignored)
*/
static int dri(JPEGHEADER *hdr, FILE *fp)
{
  int answer;
  answer = fget16(fp);

  hdr->dri = answer;
  
  return answer;
}

/*
  load quantisation tables
  Params: hdr - the header
          fp - pointer to an open file
		 length - length of segment
  Returns: 0 on success, -1 on fail
  Notes: files may contain up to four qunatisation tables
*/
static int dqt(JPEGHEADER *hdr, FILE *fp,int length)
{
  int qtinformation;
  int precision;
  int i;
  int tablenumber;


  length -= 2;

  while(length > 0)
  {
    qtinformation = fgetc(fp);
	precision = (qtinformation >> 4) & 0xFF;
    tablenumber = qtinformation & 0x0F;
	length--; 

    if(precision)
	{
      for(i=0;i<64;i++)
	    hdr->qttable[tablenumber][i] = fget16(fp);
	  length -= 128;
	}
    else
	{
      for(i=0;i<64;i++)
	    hdr->qttable[tablenumber][i] = fgetc(fp);
	  length -= 64;
	}
	
  }

  if(length == 0)
	return 0;
  else
	return -1;

}

/*
  load the huffman table
  Params: hdr - the header
          fp - pointer to an open file
		  length - table length
  Returns: 0 on success, -1 on fail
*/
static int dht(JPEGHEADER *hdr, FILE *fp, int length)
{
  int tot;
  int information;
  int tablenumber;
  int tabletype;
  int codeswithlength[16];
  unsigned char symbol[256];
  int i;
  HUFFNODE *tree;

  length -= 2;

  while(length > 0)
  {
    information = fgetc(fp);
	tablenumber = (information & 0x0F);
	tabletype = (information & 0x10) ? 1 : 0;
	tot = 0;
	for(i=0;i<16;i++)
	{
	  codeswithlength[i] = fgetc(fp);
	  tot += codeswithlength[i];
	}
	for(i=0;i<tot;i++)
	  symbol[i] = fgetc(fp);
	if(tot > 256)
	  return -1;
	tree = buildhufftree(codeswithlength, symbol);
	if(tabletype == 0)
	  hdr->dctable[tablenumber] = tree;
	else if(tabletype == 1)
	  hdr->actable[tablenumber] = tree;
	length -= 1 + 16 + tot;
  }
  if(length == 0)
	return 0;
  else
	return -1;
}

/*
  parse start of scan
  Params: hdr - the header
          fp - pointer to an open file
  Returns: 0 on success, -1 on fail
*/
static int startofscan(JPEGHEADER *hdr, FILE *fp, int length)
{
  int i;
  int hufftable;
  //int x1, x2, x3;
  int ncomponents;

  length -= 2;

  ncomponents = fgetc(fp);
  if(ncomponents != hdr->Ncomponents)
	return -1;
  for(i=0;i<hdr->Ncomponents;i++)
  {
    hdr->component_type[i] = fgetc(fp);
    hufftable = fgetc(fp);
	hdr->usedc[i] = (hufftable >> 4) & 0x0F;
	hdr->useac[i] = hufftable & 0x0F;
	if(hdr->usedc[i] > 3 || hdr->useac[i] > 3)
	  return -1;
	if(hdr->dctable[hdr->usedc[i]] == 0)
	  return -1;
	if(hdr->actable[hdr->useac[i]] == 0)
	  return -1;
  }
  length -= 1 + hdr->Ncomponents * 2;

  /* unused ? */
  //x1 = fgetc(fp);
  //x2 = fgetc(fp);
  //x3 = fgetc(fp);

  while(length--)
	fgetc(fp);

  return 0;
}

/*
  skip past an unknown segment
  Parmas: fp - pointer to an open file
          length - segment length (includes size field).
  Returns: 0 on success, -1 premature EOF
*/
static int skipsegment(FILE *fp, int length)
{
  if(length < 2)
    return -1;
  length -= 2;
  while(length--)
  {
    if( fgetc(fp) == -1 )
	  return -1;
  }
  return 0;
}

/*
  Load segment header
  Parmas: fp - pointer to open file
	        size - return pointer for segment size
  Returns: segment id
  Notes: a segment consists one of more 0xFF bytes, followed
    by a segment size, which include the size field itself
*/
static int segmentheader(FILE *fp, int *size)
{
  int answer;
  int ch;

  ch = fgetc(fp);

  if(ch != 0xFF)
    return -1;
 
  while(ch == 0xFF)
	ch = fgetc(fp);
  if(ch == -1)
	return -1;
  answer = ch;

  /* RSTn are used for resync, may be ignored */
  if(ch == 1 || (ch >= 0xD0 && ch <= 0xD7) )
	*size = 2;
  else 
    *size = fget16(fp);

  return answer;
}



/*
  perform 2d inverse cosine transform on 8*8 block.
  Parmas: block - 64 coefficients to transform
*/
static void idct8x8(short *block)
{
  short col[8];
  int i;
  int ii;

  
  for(i=0;i<8;i++)
	fastidct8(block + i * 8);

  for(i=0;i<64;i++)
    block[i] >>= 3;

  for(i=0;i<8;i++)
  {
    for(ii=0;ii<8;ii++)
	  col[ii] = block[ii*8+i];
	fastidct8(col);
	for(ii=0;ii<8;ii++)
	  block[ii*8+i] = col[ii];
  }
  
}

/*
  fast 8 point idct.
*/
static void fastidct8(short *x)
{
  int x0, x1, x2, x3, x4, x5, x6, x7, x8;
  int W1 = 2841;
  int W2 = 2676;
  int W3 = 2408;
  int W5 = 1609;
  int W6 = 1108;
  int W7 = 565;

  if(!((x1 = x[4] << 11) | (x2 = x[6]) | (x3 = x[2]) | 
	  (x4 = x[1]) | (x5 = x[7]) | (x6 = x[5]) | (x7 = x[3])))
  {
    x[0] = x[1] = x[2] = x[3] = x[4] = x[5] = x[6] = x[7] = x[0] << 3;
	return;
  }
  x0 = (x[0] << 11) + 128;

  /* first stage */
  x8 = W7 * (x4 + x5);
  x4 = x8 + (W1 - W7) * x4;
  x5 = x8 - (W1 + W7) * x5;
  x8 = W3 * (x6 + x7);
  x6 = x8 - (W3 - W5) * x6;
  x7 = x8 - (W3 + W5) * x7;

  /* second stage */
  x8 = x0 + x1;
  x0 -= x1;
  x1 = W6 * (x3 + x2);
  x2 = x1 - (W2 + W6) * x2;
  x3 = x1 + (W2 - W6) * x3;
  x1 = x4 + x6;
  x4 -= x6;
  x6 = x5 + x7;
  x5 -= x7;

  /* third stage */
  x7 = x8 + x3;
  x8 -= x3;
  x3 = x0 + x2;
  x0 -= x2;
  x2 = (181 * (x4 + x5) + 128) >> 8;
  x4 = (181 * (x4 - x5) + 128) >> 8;

  /* fourth stage */
  x[0] = (x7 + x1) >> 8;
  x[1] = (x3 + x2) >> 8;
  x[2] = (x0 + x4) >> 8;
  x[3] = (x8 + x6) >> 8;
  x[4] = (x8 - x6) >> 8;
  x[5] = (x0 - x4) >> 8;
  x[6] = (x3 - x2) >> 8;
  x[7] = (x7 - x1) >> 8;
}

/*
  undo zigzagging on coefficients.
  Params: block - 64 coefficients to dezigzag
*/
static void unzigzag(short *block)
{
  static int zigzag[64] =
  {0, 1, 5, 6,14,15,27,28,
   2, 4, 7,13,16,26,29,42,
   3, 8,12,17,25,30,41,43,
   9,11,18,24,31,40,44,53,
   10,19,23,32,39,45,52,54,
   20,22,33,38,46,51,55,60,
   21,34,37,47,50,56,59,61,
   35,36,48,49,57,58,62,63 };
 short temp[64];
 int i;

 for(i=0;i<64;i++)
   temp[i] = block[ zigzag[i] ];
 for(i=0;i<64;i++)
   block[i] = temp[i];
}

/*
  get a block of dct coefficients from file.
  Parmas: ret - return pointer for 64 coefficients
          dctree - Huffman tree for DC coefficient
		  actree - Huffman tree for 63 ac coefficients
		  bs - the bitstream.
  Returns: 0 on success, -1 on failure
*/
static int getblock(short *ret, HUFFNODE *dctree, HUFFNODE *actree, BITSTREAM *bs)
{
  int byte;
  int bits;
  int zeroes;
  int nread = 0;
  int i;

  bits = tree_getsymbol(dctree, bs);
  ret[nread++] = (short) getsymbol(bs, bits);

  do
  {
    byte = tree_getsymbol(actree, bs);

    if(byte == 0xF0)
	{
	  if(nread + 16 > 64)
		return -1;

      for(i=0;i<16;i++)
	    ret[nread++] = 0;
	}
    zeroes = byte >> 4;
    bits = byte & 0x0F;
  
    if(bits != 0)
	{
	  if(nread + zeroes + 1 > 64)
		return -1;
      for(i=0;i<zeroes;i++)
	    ret[nread++] = 0;
      ret[nread++] = getsymbol(bs, bits);
	  if(nread == 64)
	    break;
	}
  } while(byte);

  while(nread<64)
	  ret[nread++] = 0;

  return 0;
}

/*
  build the huffman tree
  Params: codelength - 16 integer giving number of codes each length
          symbols - the symbols for each code.
  Returns: pointer to constructed huffman tree
  Notes: builds canonical huffman codes.
*/
static HUFFNODE *buildhufftree(int *codelength, unsigned char *symbols)
{
  char **codes;
  HUFFNODE *answer;
  int i;
  int tot = 0;

  for(i=0;i<16;i++)
	tot += codelength[i];

  answer = malloc( (tot + tot - 1) * sizeof(HUFFNODE));
  if(!answer)
	return 0;

  codes = buildcanonical(codelength);
  if(!codes)
  {
    free(answer);
	return 0;
  }

  buildtreerec(answer, answer + 2 * tot - 1, codes, symbols, tot, 0);

  for(i=0;i<tot;i++)
	free(codes[i]);
  free(codes);

  return answer;
}

/*
  recursive tree-building function
  Params: first - local root of tree
          last - end of tree (for safety)
		  code - list of canonical codes
		  symbol - list of attached symbols
		  N - number of codes
		  bit - index of bit to split on
  Notes: fills the root with two children, and calls recursively.
*/
static void buildtreerec(HUFFNODE *first, HUFFNODE *last, char **code, unsigned char *symbol, int N, int bit)
{
  int i;

  if(N == 0)
	return;

  assert(last - first == 2 * N - 1);

  if(N == 1)
  {
    first->child0 = 0;
	first->child1 = 0;
	first->symbol = symbol[0];
	first->suppbits = 0;
	while(code[0][bit] != 0)
	{
	  first->suppbits++;
	  bit++;
	}
	return;
  }
  for(i=0;i<N;i++)
	if(code[i][bit] == '1')
	  break;
  first->child0 = first+1;
  first->child1 = first + 2 * i;
  first->symbol = -1;
  first->suppbits = 0;

  buildtreerec(first->child0, first->child1, code, symbol, i, bit + 1);
  buildtreerec(first->child1, last, code + i, symbol + i, N - i, bit + 1); 
}

/*
  constructs list of canonical huffman codes for list
  Params: codelength - 16 integers giving number of codes of length 1 - 16
  Returns: list of codes in ASCII ('0','1') format.
  Notes: allocated array of allocated strings.
*/
static char **buildcanonical(int *codelength)
{
  int code = 0;
  int length = 1;
  char **answer;
  int i;
  int ii;
  int j = 0;
  int N = 0;

  for(i=0;i<16;i++)
	N += codelength[i];

  answer = malloc(N * sizeof(char *));
  if(!answer)
	return 0;

  for(i=0;i<16;i++)
  {
    for(ii=0;ii<codelength[i];ii++)
	{
      answer[j] = inttocode(code, length);
	  if(!answer[j])
		goto error_exit;
	  j++;
	  code++;
	}
	code <<= 1;
	length++; 
  }
 
  return answer;
error_exit:
  for(i=0;i<j;i++)
	free(answer[i]);
  free(answer);

  return 0;
}

/*
  convert code held as integer to ASCII string.
  Params: code - the huffman code
          length - number of bits
  Returns: ASCII representation of code.

*/
static char *inttocode(int code, int length)
{
  char *answer;
  int i;

  answer = malloc(length + 1);
  if(!answer)
	return 0;

  for(i=length-1;i>=0;i--)
    answer[length - i - 1] = (code & (1 << i)) ? '1' : '0';
  answer[length] = 0;

  return answer;
}

/*
  get a symbol from the Huffman tree
  Params: node - the node root
          bs - the bitstream to read bits from
  Returns: symbol read. 
*/
static int tree_getsymbol(HUFFNODE *node, BITSTREAM *bs)
{
  int i;

  while(node->child0)
  {
    if(getbit(bs))
	  node = node->child1;
	else
	  node = node->child0;
  }

  if(node->suppbits > 0)
  {
	for(i=0;i<node->suppbits;i++)
      getbit(bs);
  }

  return node->symbol;
}



/*
  bitstream constructor
  Parmas: fp - file to read bits from
  Returns: pointer to constructed object
*/
static BITSTREAM *bitstream(FILE *fp)
{
  BITSTREAM *answer;

  answer = malloc(sizeof(BITSTREAM));
  if(!answer)
	return 0;
  answer->fp = fp;
  answer->bit = 0;
  answer->rack = 0;

  return answer;
}

/*
  bitstream destructor
  Parmas: bs - pointer to object to destroy
*/
static void killbitstream(BITSTREAM *bs)
{
  free(bs);
}

static int readmarker(BITSTREAM *bs)
{
  int x;

  while(bs->bit != 0)
	getbit(bs);
  x = fgetc(bs->fp);
  x = fgetc(bs->fp);

  return 0;
}
/*
  get a symbolf from a bitstream:
  Parmas: bs - the bitstream
          bits - size of symbol
  Returns: symbol value
  Notes: negative symbols have leading zeroes
*/
static int getsymbol(BITSTREAM *bs, int bits)
{
  int answer = 0;
  int N = bits;

  if(bits == 0)
	return 0;

  while(N--)
  {
    answer <<= 1;
	answer |= getbit(bs);
  }
  if((answer & (1 << (bits-1))) == 0)
	answer -= (1 << bits ) -1; 
 

  return answer;
}

/*
  get a bit from a bitstream:
  Parmas: bs - the bitstream
  Returns: the bit read.
  Notes: the stream uses 0xFF as an escape.
*/
static int getbit(BITSTREAM *bs)
{
  int answer;
  int ch;

  if(bs->bit == 0)
  {
    bs->rack = fgetc(bs->fp);
	while(bs->rack == 0xFF)
	{
	  while( (ch = fgetc(bs->fp)) == 0xFF )
		continue;
	  if(ch == 0)
		break;
	  bs->rack = fgetc(bs->fp);
    }
	bs->bit = 0x80;
  }
  
  answer = (bs->rack & bs->bit) ? 1 : 0;
  bs->bit >>= 1;

  return answer;
}

/*
  load 16 bits from a file, big-endian
  Parmas: fp - pointer to open file
  Returns: value read
*/
static int fget16(FILE *fp)
{
  int ch;
  int answer;

  ch = fgetc(fp);
  answer = ch << 8;
  ch = fgetc(fp);
  answer |= ch;

  return answer;
}



