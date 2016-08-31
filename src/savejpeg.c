#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeg.h"

#define SOF0  0xC0 /*  Start Of Frame (baseline JPEG) */
#define SOF1  0xC1 /*   ditto */
#define DHT   0xC4 /*  Define Huffman Table */
#define DQT   0xDB /*  Define Quantization Table */
#define DRI   0xDD /*  Define Restart Interval */
#define APP0  0xE0 /*  JFIF APP0 segment marker */
#define SOI   0xD8 /*  Start Of Image */
#define EOI   0xD9 /*  End Of Image */
#define SOS   0xDA /*  Start Of Scan, for details see below */

typedef struct
{
  char *code;         /* Huffman code in ASCII format */
  int symbol;         /* the symbo, it represents */
} HUFFENTRY;

typedef struct
{
  char **codes;      /* list of codes, indexed by symbol */
  int N;             /* number of codes (not max symbol) */
} HUFFTABLE;

typedef struct
{
  HUFFTABLE *Ydc;         /* luminace DC Huffman table */
  HUFFTABLE *Yac;         /* luminace AC Huffman table */
  HUFFTABLE *Cdc;         /* chrominace DC Huffman table */
  HUFFTABLE *Cac;         /* chrominace AC Huffman table */
  unsigned char qlum[64];   /* luminace qunatisation table */
  unsigned char qchrom[64]; /* chrominance quantisation table */
} TABLES;

typedef struct
{
  FILE *fp;       /* file */
  int rack;       /* byte to write */
  int bit;        /* mask, 0x80 when initialised */
} BITSTREAM;

static TABLES *maketables(unsigned char *buff, int width, int height);
static void killtables(TABLES *tab);

static int saveheader(FILE *fp, TABLES *tab, int width, int height);
static int savescan(FILE *fp, TABLES *tab, unsigned char *buff, int width, int height);
static void rgbtoYuv(unsigned char *block, short *Y0, short *Y1, short *Y2, short *Y3,
			  short *Cb, short *Cr);
static void getblock16x16(unsigned char *block, unsigned char *buff, int width, int height, int x, int y);

static void savesoi(FILE *fp);
static void saveapp0(FILE *fp);
static void savesof0(FILE *fp, int width, int height);
static void savedht(FILE *fp, HUFFTABLE *Ydc, HUFFTABLE *Yac, HUFFTABLE *chromdc, HUFFTABLE *chromac);
static void savedqt(FILE *fp, unsigned char *lum, unsigned char *chrom);
static void savesos(FILE *fp);

static void dct8x8(short *block);
static void fastdct8(short *x);
static void zigzag(short *block);
static void saveblock(short *block, HUFFTABLE *dc, HUFFTABLE *ac, BITSTREAM *bs);
static int inttowrite(int x, int len);
static int symbollen(int x);
static void writehuff(HUFFTABLE *ht, int symbol, BITSTREAM *bs);

static BITSTREAM *bitstream(FILE *fp);
static void killbitstream(BITSTREAM *bs);
static void writebits(BITSTREAM *bs, int x, int bits);
static void putbit(BITSTREAM *bs, int bit);
static void flushbitstream(BITSTREAM *bs);

static void lumqt(unsigned char *qt);
static void chromqt(unsigned char *qt);
static HUFFTABLE *lumdc(void);
static HUFFTABLE *chromdc(void);
static HUFFTABLE *lumac(void);
static HUFFTABLE *chromac(void);

static HUFFTABLE *buildhuff(HUFFENTRY *table, int N);
static void killhuff(HUFFTABLE *ht);
static int getlength(HUFFTABLE *ht, int *len);
static int compentries(const void *e1, const void *e2);
static void getsymbols(HUFFTABLE *ht, unsigned char *sym);

static void fput16(int x, FILE *fp);
static char *mystrdup(const char *str);

/*
  save a 24-bit colour image in JPEG format
  Params: path - name of file to save
          rgb - the raster data
		  width - image width
		  height - image height
  Returns: 0 on success, -1 on fail
*/
int savejpeg(char *path, unsigned char *rgb, int width, int height)
{
  FILE *fp;
  int answer;
  TABLES *tables;

  tables = maketables(rgb, width, height);
  if(!tables)
	return -1;

  fp = fopen(path, "wb");
  if(!fp)
	return -1;
  saveheader(fp, tables, width, height);
  if( savescan(fp, tables, rgb, width, height) == -1)
  {
	killtables(tables);
    return -1;
  }

  killtables(tables);

  answer = ferror(fp);
  
  if( fclose(fp) == -1)
	return -1;

  return answer;
}

/*
  create the tables we use for the JPEG codec
  Params: buff - the image
         width - image width
		 height - image height
  Returns: a tables structure with quantisation and Huffman tables
  Notes: currently uses default baseline tables
    and ignores the parameters
*/
static TABLES *maketables(unsigned char *buff, int width, int height)
{
  TABLES *answer;

  answer = malloc(sizeof(TABLES));

  answer->Ydc = lumdc();
  answer->Yac = lumac();
  answer->Cdc = chromdc();
  answer->Cac = chromac();

  if(!answer->Ydc || !answer->Yac || !answer->Cdc || !answer->Cac)
  {
    killtables(answer);
	return 0;
  }
  
  lumqt(answer->qlum);
  chromqt(answer->qchrom);

  return answer;
}

/*
  destroy tables
  Parmas: tab - structure to destroy
*/
static void killtables(TABLES *tab)
{
  if(tab)
  {
    killhuff(tab->Ydc);
	killhuff(tab->Yac);
	killhuff(tab->Cdc);
	killhuff(tab->Cac);
    free(tab);
  }
}

/*
  save the header information
  Params: fp - pointer to an open file
          tab - the tables 
		  width - image width
		  height - image height
  Returns: 0 on success, -1 on fail
*/
static int saveheader(FILE *fp, TABLES *tab, int width, int height)
{
  savesoi(fp);
  saveapp0(fp);
  savedqt(fp, tab->qlum, tab->qchrom);
  savesof0(fp, width, height);
  savedht(fp, tab->Ydc, tab->Yac, tab->Cdc, tab->Cac);
  savesos(fp);

  return 0;
}

/*
  save the scan information
  Parmas: fp -pointer to open file
          tab - the tables used for the image
		  buff - rgb input buffer
		  width - image width
		  height - image height
*/
static int savescan(FILE *fp, TABLES *tab, unsigned char *buff, int width, int height)
{
  int i;
  int ii;
  int iii;
  int iv;
  short lum[4][64];  /* blocks for luminance */
  short Cb[64];      /* block for blue chrominance */
  short Cr[64];      /* block for red chrominance */
  BITSTREAM *bs;
  unsigned char *block; /* buffer for 16 x 16 block */
  int olddcy = 0;       /* keep track of luminace dc */
  int olddcb = 0;       /* keep track of Cb dc */
  int olddcr = 0;       /* keep track of Cr dc */

  block = malloc(16 * 16 * 3);
  if(!block)
	return -1;

  bs = bitstream(fp);
  if(!bs)
  {
    free(block);
	return -1;
  }

  for(i=0;i<height;i+=16)
	for(ii=0;ii<width;ii+=16)
	{
      getblock16x16(block, buff, width, height, ii, i);
      rgbtoYuv(block, lum[0], lum[1], lum[2], lum[3], Cb, Cr);

	  /* four luminace blocks encoded first */
	  for(iii=0;iii<4;iii++)
      {
		dct8x8(lum[iii]);
		zigzag(lum[iii]);

		for(iv=0;iv<64;iv++)
		  lum[iii][iv] /= tab->qlum[iv];
		lum[iii][0] -= olddcy;
		olddcy += lum[iii][0];
		saveblock(lum[iii], tab->Ydc, tab->Yac, bs);
      }
	  
      /* blue chrominance block */
	  dct8x8(Cb);
	  zigzag(Cb);
	  for(iv=0;iv<64;iv++)
		Cb[iv] /= tab->qchrom[iv];
	  Cb[0] -= olddcb;
	  olddcb += Cb[0];
	  saveblock(Cb, tab->Cdc, tab->Cac, bs);
  
	  /* red chrominance block */
	  dct8x8(Cr);
	  zigzag(Cr);
	  for(iv=0;iv<64;iv++)
		Cr[iv] /= tab->qchrom[iv];
	  Cr[0] -= olddcr;
	  olddcr += Cr[0];
	  saveblock(Cr, tab->Cdc, tab->Cac, bs);
	}

  flushbitstream(bs);
  killbitstream(bs);
  free(block);

  /* save EOI marker */
  fputc(0xFF, fp);
  fputc(EOI, fp);

  return 0;
}

/*
  convert a 16 x 16 block to Yuv colur space
  Params: block - the rgb block
          Y0 - return pointer for first chrominance block
		  Y1 - return pointer for second chrominance block 
		  Y2 - return pointer for thrid chrominace block
		  Cb - return pointer for blue chrominace
		  Cr - return pointer for red chrominance
*/
static void rgbtoYuv(unsigned char *block, 
			  short *Y0, short *Y1, short *Y2, short *Y3,
			  short *Cb, short *Cr)
{
   double lum;
   double cb = 0;
   double cr = 0;
   int i;
   int ii;
   

   for(i=0;i<16;i++)
	for(ii=0;ii<16;ii++)
	{
	  lum = block[i * 16 * 3 + ii * 3] * 0.299 +
		    block[i * 16 * 3 + ii * 3 + 1] * 0.587 +
            block[i * 16 * 3 + ii * 3 + 2] * 0.114;
	 
	  lum -= 128;
	  
	  if(i < 8 && ii < 8)
	    *Y0++ = (short) lum;
	  else if( ii >= 8 && i < 8)
	    *Y1++ = (short) lum;
	  else if( ii < 8 && i >= 8)
	    *Y2++ = (short) lum;
	  else 
	    *Y3++ = (short) lum;
	  /* chrominance - average four pixels */
	  if( (i % 2) == 0 && (ii % 2) == 0)
	  {
	    cb = block[i * 16 * 3 + ii * 3] * -0.1687 +
		    block[i * 16 * 3 + ii * 3 + 1] * -0.3313 +
		    block[i * 16 * 3 + ii * 3 + 2] * 0.5;
		cb += block[i * 16 * 3 + ii * 3 + 3] * -0.1687 +
		    block[i * 16 * 3 + ii * 3 + 1 + 3] * -0.3313 +
		    block[i * 16 * 3 + ii * 3 + 2 + 3] * 0.5;
		cb += block[(i+1) * 16 * 3 + ii * 3] * -0.1687 +
		    block[(i+1) * 16 * 3 + ii * 3 + 1] * -0.3313 +
		    block[(i+1) * 16 * 3 + ii * 3 + 2] * 0.5;
		cb += block[(i+1) * 16 * 3 + ii * 3+3] * -0.1687 +
		    block[(i+1) * 16 * 3 + ii * 3 + 1+3] * -0.3313 +
		    block[(i+1) * 16 * 3 + ii * 3 + 2+3] * 0.5;

	    cr = block[i * 16 * 3 + ii * 3]  * 0.5 + 
		    block[i * 16 * 3 + ii * 3 + 1] * -0.4187 +
		    block[i * 16 * 3 + ii * 3 + 2] * -0.0813;
	    cr += block[i * 16 * 3 + ii * 3 + 3]  * 0.5 + 
		    block[i * 16 * 3 + ii * 3 + 1 + 3] * -0.4187 +
		    block[i * 16 * 3 + ii * 3 + 2 + 3] * -0.0813;
		cr += block[ (i+1) * 16 * 3 + ii * 3]  * 0.5 + 
		    block[ (i+1) * 16 * 3 + ii * 3 + 1] * -0.4187 +
		    block[ (i+1) * 16 * 3 + ii * 3 + 2] * -0.0813;
		cr += block[(i+1) * 16 * 3 + ii * 3 + 3]  * 0.5 + 
		    block[(i+1)* 16 * 3 + ii * 3 + 1 + 3] * -0.4187 +
		    block[(i+1) * 16 * 3 + ii * 3 + 2 + 3] * -0.0813;
		*Cb++ = (short) (cb/4);
        *Cr++ = (short) (cr/4);
	  }
	}
}

/*
  extract a 16 x 16 block from image
  Params: block - return pointer for block
          buff - the image buffer
		  width - image width
		  height - image height
		  x - x coordinate for top left
		  y - y coordinate of top left
  Notes: areas outside the image are filled with grey
*/
static void getblock16x16(unsigned char *block, unsigned char *buff, int width, int height, int x, int y)
{
  int i;
  int ii;

  for(i=0;i<16;i++)
	for(ii=0;ii<16;ii++)
	{
      if(i + y < height && ii + x < width)
	  {
	    *block++ = buff[(i+y) * width * 3 + (ii + x) * 3];
		*block++ = buff[(i+y) * width * 3 + (ii + x) * 3 + 1];
		*block++ = buff[(i+y) * width * 3 + (ii + x) * 3 + 2];
	  }
	  else
	  {
	    *block++ = 128;
		*block++ = 128;
		*block++ = 128;
	  }
    }
}

/*
  save start of information marker
  Parmas: fp - pointer to an open file
*/
static void savesoi(FILE *fp)
{
  fputc(0xFF, fp);
  fputc(SOI, fp);
}

/*
  save the JPEG IFF format tag
  Params: fp - pointer to an open file
  Notes: no thumbnail. Assume 1:1 aspect ratio
*/
static void saveapp0(FILE *fp)
{
  
  fputc(0xFF, fp);
  fputc(APP0, fp);
  fput16(16, fp);  /* segment size */

  /* 'JFIF'#0 ($4a, $46, $49, $46, $00), identifies JFIF */

  fputc(0x4A, fp);
  fputc(0x46, fp);
  fputc(0x49, fp);
  fputc(0x46, fp);
  fputc(0, fp);

  fputc(1, fp); /* major revision */
  fputc(0, fp); /* minor revision */
  
  fputc(0, fp); /* density units - none */
  fput16(1, fp); /* x density */
  fput16(1, fp); /* y density */

  fputc(0, fp); /* thumbnail width */
  fputc(0, fp); /* thumbnail height */

}

/*
  save start of frame marker
  Params: fp - pointer to an open file
          width - image width
		  height - image height
  Notes: always use a 2:1:1 Yuv format
*/
static void savesof0(FILE *fp, int width, int height)
{
  fputc(0xFF, fp);
  fputc(SOF0, fp); /* start of frame */
  fput16(17, fp);  /* segment length */

  fputc(8, fp); /* precision */
  fput16(height, fp);
  fput16(width, fp);
  fputc(3, fp); /* number of components */

  fputc(1, fp);             /* luminance */
  fputc( (2 << 4) | 2, fp); /* sampling */
  fputc(0, fp);             /* quantisation table */

  fputc(2, fp);             /* Cb */
  fputc( (1 << 4) | 1, fp); /* sampling */
  fputc(1, fp);             /* quantisation table */ 

  fputc(3, fp);             /* Cr */
  fputc( (1 << 4) | 1, fp); /* sampling */
  fputc(1, fp);             /* quantisation table */
}

/*
  save the Huffman tables
  Parmas: fp - pointer to an open file
          Ydc - luminace dc Huffman table
		  Yac - luminance ac Huffman table
		  chromdc - chrominance dc Huffman table
		  chromac - chrominance ac Huffman table
*/
static void savedht(FILE *fp, HUFFTABLE *Ydc, HUFFTABLE *Yac, HUFFTABLE *chromdc, HUFFTABLE *chromac)
{
  unsigned char symbol[256];
  int len[16];
  int inf;
  int length;
  int i;
  int type;
  int tablenumber;

  fputc(0xFF, fp);
  fputc(DHT, fp);

  length = 2 + (1 + 16) * 4 + Ydc->N + Yac->N + chromdc->N + chromac->N;
  fput16(length, fp);

  type = 0;
  tablenumber = 0;
  inf = (type << 4) | tablenumber;  
  fputc(inf, fp);
  getlength(Ydc, len);
  for(i=0;i<16;i++)
	fputc(len[i], fp);
  getsymbols(Ydc, symbol);
  for(i=0;i<Ydc->N;i++)
    fputc(symbol[i], fp);

  type = 0;
  tablenumber = 1;
  inf = (type << 4) | tablenumber;  
  fputc(inf, fp);
  getlength(chromdc, len);
  for(i=0;i<16;i++)
	fputc(len[i], fp);
  getsymbols(chromdc, symbol);
  for(i=0;i<chromdc->N;i++)
    fputc(symbol[i], fp);

  type = 1;
  tablenumber = 0;
  inf = (type << 4) | tablenumber;  
  fputc(inf, fp);
  getlength(Yac, len);
  for(i=0;i<16;i++)
	fputc(len[i], fp);
  getsymbols(Yac, symbol);
  for(i=0;i<Yac->N;i++)
    fputc(symbol[i], fp);
 
  type = 1;
  tablenumber = 1;
  inf = (type << 4) | tablenumber;  
  fputc(inf, fp);
  getlength(chromac, len);
  for(i=0;i<16;i++)
	fputc(len[i], fp);
  getsymbols(chromac, symbol);
  for(i=0;i<chromac->N;i++)
    fputc(symbol[i], fp);

}

/*
  save quantisation tables
  Parmas: fp - pointer toan open file
          lum - luminace quantisation table
		  chrom - chrominance quantisation table
*/
static void savedqt(FILE *fp, unsigned char *lum, unsigned char *chrom)
{
  fputc(0xFF, fp);
  fputc(DQT, fp);

  fput16(2 + 65 * 2, fp);    /* segment length */

  fputc( (0 << 4) | 0, fp); /* precision and table number */
  fwrite(lum, 1, 64, fp);
  
  fputc( (0 << 4) | 1, fp);  /* precision and table number */
  fwrite(chrom, 1, 64, fp);
  
}

/*
  save start of scan
  Params: fp - pointer to an open file
  Notes:
*/
static void savesos(FILE *fp)
{
  fputc(0xFF, fp);
  fputc(SOS, fp);
  fput16(12, fp); /* segment length */
  fputc(3, fp);   /* N components */
  fputc(1, fp);  /* luminance */
  fputc( (0 << 4) | 0, fp); /* huff tables 0 */
  
  fputc(2, fp); /* Cb */
  fputc( (1 << 4) | 1, fp); /* huff tables 1 */

  fputc(3, fp); /* Cr */
  fputc( (1 << 4) | 1, fp); /* huff tables 1 */

  /* undocumented bytes ? */
  fputc(0, fp);
  fputc(64, fp);
  fputc(0, fp);
}

#include <math.h>
#define PI 3.1415926535897932384626433832795

void cosinet(double *real, double *trans, int N)
{
  int i;
  int ii;
  double tot = 0;

  for(i=0;i<N;i++)
  {
	tot = 0;
	for(ii=0;ii<N;ii++)
    {
	  tot += real[ii] * cos( (PI * i * (ii * 2 + 1) )/( 2 * N ));
    }
	if(i==0)
	  trans[i] = tot/sqrt(N);
	else
	  trans[i] = tot * sqrt(2.0/N);
  }
}

/*
 8 x 8 2d dct
 Params: block - 64 coefficients
*/
static void dct8x8(short *block)
{
  int i;
  int ii;
  short col[8];
  
  for(i=0;i<8;i++)
	fastdct8(&block[i*8]);
  for(i=0;i<8;i++)
  {
    for(ii=0;ii<8;ii++)
	  col[ii] = block[ii*8+i];
	fastdct8(col);
	for(ii=0;ii<8;ii++)
	  block[ii*8+i] = col[ii];
  }
}

/*
  fast discrete cosine transform
  Params: x - vector to transform (8 shorts)
*/
static void fastdct8(short *x)
{
  int blk[7][8];
  int *X;

  int a1 = 2896; /* cos(PI * 4.0/16.0); */
  int a2 = 2216; /* cos(PI * 2.0/16.0) - cos(PI * 6.0/16.0); */
  int a3 = 2896; /* cos(PI * 4.0/16.0); */
  int a4 = 5351; /* cos(PI * 2.0/16.0) + cos(PI * 6.0/16.0); */
  int a5 = 1567; /* cos(PI * 6.0/16.0); */

  int s0 = 1448; /* 1.0/(2 * sqrt(2.0)); */
  int s1 = 1044; /* 1.0/(4.0 * cos(PI * 1/16.0)); */
  int s2 = 1108; /* 1.0/(4.0 * cos(PI * 2/16.0)); */
  int s3 = 1231; /* 1.0/(4.0 * cos(PI * 3/16.0)); */
  int s4 = 1448; /* 1.0/(4.0 * cos(PI * 4/16.0)); */
  int s5 = 1843; /* 1.0/(4.0 * cos(PI * 5/16.0)); */
  int s6 = 2675; /* 1.0/(4.0 * cos(PI * 6/16.0)); */
  int s7 = 5248; /* 1.0/(4.0 * cos(PI * 7/16.0)); */

  int store;

  blk[0][0] = x[0] + x[7];
  blk[0][1] = x[1] + x[6];
  blk[0][2] = x[2] + x[5];
  blk[0][3] = x[3] + x[4];
  blk[0][4] = x[3] - x[4];
  blk[0][5] = x[2] - x[5];
  blk[0][6] = x[1] - x[6];
  blk[0][7] = x[0] - x[7];

  X = blk[0];

  blk[1][0] = X[0] + X[3];
  blk[1][1] = X[1] + X[2];
  blk[1][2] = X[1] - X[2];
  blk[1][3] = X[0] - X[3];
  blk[1][4] = -X[4] - X[5];
  blk[1][5] = X[5] + X[6];
  blk[1][6] = X[7] + X[6];
  blk[1][7] = X[7];

  X = blk[1];

  blk[2][0] = X[0] + X[1];
  blk[2][1] = X[0] - X[1];
  blk[2][2] = X[2] + X[3];
  blk[2][3] = X[3];
  blk[2][4] = X[4];
  blk[2][5] = X[5];
  blk[2][6] = X[6];
  blk[2][7] = X[7];

  X = blk[2];

  blk[3][0] = X[0];
  blk[3][1] = X[1];
  blk[3][2] = ((X[2] * a1) + 2048) >> 12;
  blk[3][3] = X[3];
  blk[3][4] = ((X[4] * a2) + 2048) >> 12;
  blk[3][5] = ((X[5] * a3) + 2048) >> 12;
  blk[3][6] = ((X[6] * a4) + 2048) >> 12;
  blk[3][7] = X[7];
  
  store = ((X[4] + X[6]) * a5) >> 12;
  
  X = blk[3];
  blk[4][0] = X[0];
  blk[4][1] = X[1];
  blk[4][2] = X[2];
  blk[4][3] = X[3];
  blk[4][4] = -store - X[4];
  blk[4][5] = X[5];
  blk[4][6] = X[6] - store;
  blk[4][7] = X[7];

  X = blk[4];
  blk[5][0] = X[0];
  blk[5][1] = X[1];
  blk[5][2] = X[2] + X[3];
  blk[5][3] = X[3] - X[2];
  blk[5][4] = X[4];
  blk[5][5] = X[5] + X[7];
  blk[5][6] = X[6];
  blk[5][7] = X[7] - X[5];

  X = blk[5];
  blk[6][0] = X[0];
  blk[6][1] = X[1];
  blk[6][2] = X[2];
  blk[6][3] = X[3];
  blk[6][4] = X[4] + X[7];
  blk[6][5] = X[5] + X[6];
  blk[6][6] = X[5] - X[6];
  blk[6][7] = X[7] - X[4];

  X = blk[6];
  x[0] = (short) ((X[0] * s0 + 2048) >> 12);
  x[4] = (short) ((X[1] * s4 + 2048) >> 12);
  x[2] = (short) ((X[2] * s2 + 2048) >> 12);
  x[6] = (short) ((X[3] * s6 + 2048) >> 12);
  x[5] = (short) ((X[4] * s5 + 2048) >> 12);
  x[1] = (short) ((X[5] * s1 + 2048) >> 12);
  x[7] = (short) ((X[6] * s7 + 2048) >> 12);
  x[3] = (short) ((X[7] * s3 + 2048) >> 12);
  
}

static void slowdct8(short *x)
{
  double real[8];
  double trans[8];
  int i;

  for(i=0;i<8;i++)
	real[i] = x[i];
  cosinet(real, trans, 8);
  for(i=0;i<8;i++)
	x[i] = (short) trans[i];
}

/*
  do zigzagging on coefficients.
  Params: block - 64 coefficients to dezigzag
*/
static void zigzag(short *block)
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
   temp[ zigzag[i]] = block[i];
 for(i=0;i<64;i++)
   block[i] = temp[i];
}

/*
  save a block 
  Parmas: block - 64 transformed and zigzagged parameters
          dc - table for dc coefficient
		  ac - table for ac coefficient
		  bs - the bitstream
*/
static void saveblock(short *block, HUFFTABLE *dc, HUFFTABLE *ac, BITSTREAM *bs)
{
  int len;
  int zeroes;
  int i = 1;

  /* write the dc */
  len = symbollen(block[0]);
  writehuff(dc, len, bs);
  writebits(bs, inttowrite(block[0], len), len);
 
  /* write ac */
  while(i < 64)
  {
	zeroes = 0;
    while(block[i] == 0)
    {
	  zeroes++;
	  i++;
	  if(i==64)
	  {
		writehuff(ac, 0, bs);
	    return;
	  }
	}
	while(zeroes >= 16)
	{
      writehuff(ac, 0xF0, bs);
	  zeroes -= 16;
    }
    len = symbollen(block[i]);
	writehuff(ac, (zeroes << 4) | len, bs);
    writebits(bs, inttowrite(block[i], len), len);
	i++;
  }

}

/*
  convert a block coefficinet to the integer that goes into bitstream
  Params: x - the value
          len - number of bits to encode
  Returns: bits to write. Negative has leading 0 and 0 represents the
    lowest possible value.
*/
static int inttowrite(int x, int len)
{
  if(x >= 0)
	return x;
  else
	return x + (1 << len) - 1;
}

/*
  get the number of bits needed to encode a value
  Params: x - the value
  Returns: number of significant bits
*/
static int symbollen(int x)
{
  int answer = 0;

  if(x < 0)
	x = -x;
  while(x)
  {
	answer++;
	x >>= 1;
  }

  return answer;
}

/*
  write a Huffman code to bitstream
  Parmas: ht - the Huffman table
          symbol - the symbol to write
		  bs - the bitstream
*/
static void writehuff(HUFFTABLE *ht, int symbol, BITSTREAM *bs)
{
  int i = 0;

  while(ht->codes[symbol][i])
  {
    if(ht->codes[symbol][i] == '1')
	  putbit(bs, 1);
	else
	  putbit(bs, 0);
	i++;
  }
}

/*
  create a bitstream
  Parmas: fp - pointer to open file
  Returns: bitstream opened for writing
*/
static BITSTREAM *bitstream(FILE *fp)
{
  BITSTREAM *answer;

  answer = malloc(sizeof(BITSTREAM));
  if(!answer)
    return 0;
  answer->fp = fp;
  answer->bit = 0x80;
  answer->rack = 0;

  return answer;
}

/*
  bitstream destructor
  Params: bs - object to destory
  Notes: remember to flush before destroying
*/
static void killbitstream(BITSTREAM *bs)
{
  free(bs);
}

/*
  write bits to a bitstream
  Params: bs - the bitstream
          x - value to write
		  bits - number of bits to write
*/
static void writebits(BITSTREAM *bs, int x, int bits)
{
  while(bits--)
  {
    putbit(bs, (x & (1 << bits)) ? 1 : 0); 
  }
}

/*
  write a bit to a bitstream
  Parmas: bs - the bitstream
          bit - true to write set bit, zero for clear bit
*/
static void putbit(BITSTREAM *bs, int bit)
{
  if(bit)
    bs->rack |= bs->bit;
  bs->bit >>= 1;
  if(bs->bit == 0)
  {
    fputc(bs->rack, bs->fp);
	/* 0xFF is the JPEG escape sequence */
    if(bs->rack == 0xFF)
	  fputc(0, bs->fp);
	bs->rack =  0;
	bs->bit = 0x80;
  }
}

/*
  write cached bits to stream
  Params: bs - the bitstream
  Notes: call before destroying
*/
static void flushbitstream(BITSTREAM *bs)
{
  while(bs->bit != 0x80)
    putbit(bs, 1);
}

/*
  get the standard luminance quantisation table
*/
static void lumqt(unsigned char *qt)
{
   static unsigned char harshlum[64] =
   {
    16, 11, 12, 14, 12, 10, 16, 14, 13, 14, 18, 17, 16, 19, 24, 40, 26, 24, 22, 22,
    24, 49, 35, 37, 29, 40, 58, 51, 61, 60, 57, 51, 56, 55, 64, 72, 92, 78, 64, 68,
    87, 69, 55, 56, 80, 109, 81, 87, 95, 98, 103, 104, 103, 62, 77, 113, 121, 112, 
	100, 120, 92, 101, 103, 99,
   };

   static unsigned char lum[64] = 
   {
	   4, 2, 2, 4, 6, 10, 12, 15, 
       3, 3, 3, 4, 6, 14, 15, 13, 
       3, 3, 4, 6, 10, 14, 17, 14, 
       3, 4, 5, 7, 12, 21, 20, 15, 
       4, 5, 9, 14, 17, 27, 25, 19, 
       6, 8, 13, 16, 20, 26, 28, 23,  
       12, 16, 19, 21, 25, 30, 30, 25, 
       18, 23, 23, 24, 28, 25, 25, 24 
   };

  
  memcpy(qt, lum, 64);
}  

/*
  get the standard chrominance quantisation table
*/
static void chromqt(unsigned char *qt)
{
   static unsigned char harshchrom[64] =
   {
     17, 18, 18, 24, 21, 24, 47, 26, 26, 47, 99, 66, 56, 66, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99,
   };

   static unsigned char chrom[64] =
   {
		4, 4, 6, 11, 24, 24, 24, 24, 
		4, 5, 6, 16, 24, 24, 24, 24, 
		6, 6, 14, 24, 24, 24, 24, 24, 
		11, 16, 24, 24, 24, 24, 24, 24, 
		24, 24, 24, 24, 24, 24, 24, 24, 
		24, 24, 24, 24, 24, 24, 24, 24, 
		24, 24, 24, 24, 24, 24, 24, 24, 
		24, 24, 24, 24, 24, 24, 24, 24
   };

  memcpy(qt, chrom, 64);
}

/*
  get the standard luminace DC Huffman table
*/
static HUFFTABLE *lumdc(void)
{
/* dc table 0 */
 static HUFFENTRY table[12] =
 {
	 {"00", 0},
	 {"010", 1},
	 {"011", 2},
	 {"100", 3},
	 {"101", 4},
	 {"110", 5},
	 {"1110", 6},
	 {"11110", 7},
	 {"111110", 8},
	 {"1111110", 9},
	 {"11111110", 10},
	 {"111111110", 11},
 };

 return buildhuff(table, 12);
}

/*
  get the standard chrominance DC Huffman table
*/
static HUFFTABLE *chromdc(void)
{
  static HUFFENTRY table[12] =
  {
/* dc table 1 */
	{"00", 0},
	{"01", 1},
	{"10", 2},
	{"110", 3},
	{"1110", 4},
	{"11110", 5},
	{"111110", 6},
	{"1111110", 7},
	{"11111110", 8},
	{"111111110", 9},
	{"1111111110", 10},
	{"11111111110", 11},
  };

  return buildhuff(table, 12);
}

/*
  get the standard luminace AC Huffman table
*/
static HUFFTABLE *lumac(void)
{
  static HUFFENTRY table[] =
  {
/* ac tree 0 */
	  {"00", 1},
	  {"01", 2},
	  {"100", 3},
	  {"1010", 0},
	  {"1011", 4},
	  {"1100", 17},
	  {"11010", 5},
	  {"11011", 18},
	  {"11100", 33},
	  {"111010", 49},
	  {"111011", 65},
	  {"1111000", 6},
	  {"1111001", 19},
	  {"1111010", 81},
	  {"1111011", 97},
	  {"11111000", 7},
	  {"11111001", 34},
	  {"11111010", 113},
	  {"111110110", 20},
	  {"111110111", 50},
	  {"111111000", 129},
	  {"111111001", 145},
	  {"111111010", 161},
	  {"1111110110", 8},
	  {"1111110111", 35},
	  {"1111111000", 66},
	  {"1111111001", 177},
	  {"1111111010", 193},
	  {"11111110110", 21},
	  {"11111110111", 82},
	  {"11111111000", 209},
	  {"11111111001", 240},
	  {"111111110100", 36},
	  {"111111110101", 51},
	  {"111111110110", 98},
	  {"111111110111", 114},
	  {"111111111000000", 130},
	  {"1111111110000010", 9},
	  {"1111111110000011", 10},
	  {"1111111110000100", 22},
	  {"1111111110000101", 23}, 
	  {"1111111110000110", 24},
	  {"1111111110000111", 25},
	  {"1111111110001000", 26},
	  {"1111111110001001", 37},
	  {"1111111110001010", 38},
	  {"1111111110001011", 39},
	  {"1111111110001100", 40},
	  {"1111111110001101", 41},
	  {"1111111110001110", 42},
	  {"1111111110001111", 52},
	  {"1111111110010000", 53},
	  {"1111111110010001", 54},
	  {"1111111110010010", 55},
	  {"1111111110010011", 56},
	  {"1111111110010100", 57},
	  {"1111111110010101", 58},
	  {"1111111110010110", 67},
	  {"1111111110010111", 68},
	  {"1111111110011000", 69},
	  {"1111111110011001", 70},
	  {"1111111110011010", 71},
	  {"1111111110011011", 72},
	  {"1111111110011100", 73},
	  {"1111111110011101", 74},
	  {"1111111110011110", 83},
	  {"1111111110011111", 84},
	  {"1111111110100000", 85},
	  {"1111111110100001", 86},
	  {"1111111110100010", 87},
	  {"1111111110100011", 88},
	  {"1111111110100100", 89},
	  {"1111111110100101", 90},
	  {"1111111110100110", 99},
	  {"1111111110100111", 100},
	  {"1111111110101000", 101},
	  {"1111111110101001", 102},
	  {"1111111110101010", 103},
	  {"1111111110101011", 104},
	  {"1111111110101100", 105},
	  {"1111111110101101", 106},
	  {"1111111110101110", 115},
	  {"1111111110101111", 116},
	  {"1111111110110000", 117},
	  {"1111111110110001", 118},
	  {"1111111110110010", 119},
	  {"1111111110110011", 120},
	  {"1111111110110100", 121},
	  {"1111111110110101", 122},
	  {"1111111110110110", 131},
	  {"1111111110110111", 132},
	  {"1111111110111000", 133},
	  {"1111111110111001", 134},
	  {"1111111110111010", 135},
	  {"1111111110111011", 136},
	  {"1111111110111100", 137},
	  {"1111111110111101", 138},
	  {"1111111110111110", 146},
	  {"1111111110111111", 147},
	  {"1111111111000000", 148},
	  {"1111111111000001", 149},
	  {"1111111111000010", 150},
	  {"1111111111000011", 151},
	  {"1111111111000100", 152},
	  {"1111111111000101", 153},
	  {"1111111111000110", 154},
	  {"1111111111000111", 162},
	  {"1111111111001000", 163},
	  {"1111111111001001", 164},
	  {"1111111111001010", 165},
	  {"1111111111001011", 166},
	  {"1111111111001100", 167},
	  {"1111111111001101", 168},
	  {"1111111111001110", 169},
	  {"1111111111001111", 170},
	  {"1111111111010000", 178},
	  {"1111111111010001", 179},
	  {"1111111111010010", 180},
	  {"1111111111010011", 181},
	  {"1111111111010100", 182},
	  {"1111111111010101", 183},
	  {"1111111111010110", 184},
	  {"1111111111010111", 185},
	  {"1111111111011000", 186},
	  {"1111111111011001", 194},
	  {"1111111111011010", 195},
	  {"1111111111011011", 196},
	  {"1111111111011100", 197},
	  {"1111111111011101", 198},
	  {"1111111111011110", 199},
	  {"1111111111011111", 200},
	  {"1111111111100000", 201},
	  {"1111111111100001", 202},
	  {"1111111111100010", 210},
	  {"1111111111100011", 211},
	  {"1111111111100100", 212},
	  {"1111111111100101", 213},
	  {"1111111111100110", 214},
	  {"1111111111100111", 215},
	  {"1111111111101000", 216},
	  {"1111111111101001", 217},
	  {"1111111111101010", 218},
	  {"1111111111101011", 225},
	  {"1111111111101100", 226},
	  {"1111111111101101", 227},
	  {"1111111111101110", 228},
	  {"1111111111101111", 229},
	  {"1111111111110000", 230},
	  {"1111111111110001", 231},
	  {"1111111111110010", 232},
	  {"1111111111110011", 233},
	  {"1111111111110100", 234},
	  {"1111111111110101", 241},
	  {"1111111111110110", 242},
	  {"1111111111110111", 243},
	  {"1111111111111000", 244},
	  {"1111111111111001", 245},
	  {"1111111111111010", 246},
	  {"1111111111111011", 247},
	  {"1111111111111100", 248},
	  {"1111111111111101", 249},
	  {"1111111111111110", 250},
  };

  

  return buildhuff(table, sizeof(table)/sizeof(HUFFENTRY));
}

/*
  get the standard chrominance AC Huffman table 
*/
static HUFFTABLE *chromac(void)
{
 static HUFFENTRY table[] =
 {
/* ac table 1 */
	{"00", 0},
	{"01", 1},
	{"100", 2},
	{"1010", 3},
	{"1011", 17},
	{"11000", 4},
	{"11001", 5},
	{"11010", 33},
	{"11011", 49},
	{"111000", 6},
	{"111001", 18},
	{"111010", 65},
	{"111011", 81},
	{"1111000", 7},
	{"1111001", 97},
	{"1111010", 113},
	{"11110110", 19},
	{"11110111", 34},
	{"11111000", 50},
	{"11111001", 129},
	{"111110100", 8},
	{"111110101", 20},
	{"111110110", 66},
	{"111110111", 145},
	{"111111000", 161},
	{"111111001", 177},
	{"111111010", 193},
	{"1111110110", 9},
	{"1111110111", 35},
	{"1111111000", 51},
	{"1111111001", 82},
	{"1111111010", 240},
	{"11111110110", 21},
	{"11111110111", 98},
	{"11111111000", 114},
	{"11111111001", 209},
	{"111111110100", 10},
	{"111111110101", 22},
	{"111111110110", 36},
	{"111111110111", 52},
	{"11111111100000", 225},
	{"111111111000010", 37},
	{"111111111000011", 241},
	{"1111111110001000", 23},
	{"1111111110001001", 24},
	{"1111111110001010", 25},
	{"1111111110001011", 26},
	{"1111111110001100", 38},
	{"1111111110001101", 39},
	{"1111111110001110", 40},
	{"1111111110001111", 41},
	{"1111111110010000", 42},
	{"1111111110010001", 53},
	{"1111111110010010", 54},
	{"1111111110010011", 55},
	{"1111111110010100", 56},
	{"1111111110010101", 57},
	{"1111111110010110", 58},
	{"1111111110010111", 67},
	{"1111111110011000", 68},
	{"1111111110011001", 69},
	{"1111111110011010", 70},
	{"1111111110011011", 71},
	{"1111111110011100", 72},
	{"1111111110011101", 73},
	{"1111111110011110", 74},
	{"1111111110011111", 83},
	{"1111111110100000", 84},
	{"1111111110100001", 85},
	{"1111111110100010", 86},
	{"1111111110100011", 87},
	{"1111111110100100", 88},
	{"1111111110100101", 89},
	{"1111111110100110", 90},
	{"1111111110100111", 99},
	{"1111111110101000", 100},
	{"1111111110101001", 101},
	{"1111111110101010", 102},
	{"1111111110101011", 103},
	{"1111111110101100", 104},
	{"1111111110101101", 105},
	{"1111111110101110", 106},
	{"1111111110101111", 115},
	{"1111111110110000", 116},
	{"1111111110110001", 117},
	{"1111111110110010", 118},
	{"1111111110110011", 119},
	{"1111111110110100", 120},
	{"1111111110110101", 121},
	{"1111111110110110", 122},
	{"1111111110110111", 130},
	{"1111111110111000", 131},
	{"1111111110111001", 132},
	{"1111111110111010", 133},
	{"1111111110111011", 134},
	{"1111111110111100", 135},
	{"1111111110111101", 136},
	{"1111111110111110", 137},
	{"1111111110111111", 138},
	{"1111111111000000", 146},
	{"1111111111000001", 147},
	{"1111111111000010", 148},
	{"1111111111000011", 149},
	{"1111111111000100", 150},
	{"1111111111000101", 151},
	{"1111111111000110", 152},
	{"1111111111000111", 153},
	{"1111111111001000", 154},
	{"1111111111001001", 162},
	{"1111111111001010", 163},
	{"1111111111001011", 164},
	{"1111111111001100", 165},
	{"1111111111001101", 166},
	{"1111111111001110", 167},
	{"1111111111001111", 168},
	{"1111111111010000", 169},
	{"1111111111010001", 170},
	{"1111111111010010", 178},
	{"1111111111010011", 179},
	{"1111111111010100", 180},
	{"1111111111010101", 181},
	{"1111111111010110", 182},
	{"1111111111010111", 183},
	{"1111111111011000", 184},
	{"1111111111011001", 185},
	{"1111111111011010", 186},
	{"1111111111011011", 194},
	{"1111111111011100", 195},
	{"1111111111011101", 196},
	{"1111111111011110", 197},
	{"1111111111011111", 198},
	{"1111111111100000", 199},
	{"1111111111100001", 200},
	{"1111111111100010", 201},
	{"1111111111100011", 202},
	{"1111111111100100", 210},
	{"1111111111100101", 211},
	{"1111111111100110", 212},
	{"1111111111100111", 213},
	{"1111111111101000", 214},
	{"1111111111101001", 215},
	{"1111111111101010", 216},
	{"1111111111101011", 217},
	{"1111111111101100", 218},
	{"1111111111101101", 226},
	{"1111111111101110", 227},
	{"1111111111101111", 228},
	{"1111111111110000", 229},
	{"1111111111110001", 230},
	{"1111111111110010", 231},
	{"1111111111110011", 232},
	{"1111111111110100", 233},
	{"1111111111110101", 234},
	{"1111111111110110", 242},
	{"1111111111110111", 243},
	{"1111111111111000", 244},
	{"1111111111111001", 245},
	{"1111111111111010", 246},
	{"1111111111111011", 247},
	{"1111111111111100", 248},
	{"1111111111111101", 249},
	{"1111111111111110", 250},
  };

  return buildhuff(table, sizeof(table)/sizeof(HUFFENTRY));
}

/*
  build Huffman table from entries
  Parmas: table - the table
          N - number of entries in table
*/
static HUFFTABLE *buildhuff(HUFFENTRY *table, int N)
{
  HUFFTABLE *answer;
  int i;

  answer = malloc(sizeof(HUFFTABLE));
  answer->N = N;
  answer->codes = malloc(256 * sizeof(char *));
  for(i=0;i<256;i++)
    answer->codes[i] = 0;
  for(i=0;i<N;i++)
  {
	if(answer->codes[ table[i].symbol ])
	  goto error_exit;
    answer->codes[ table[i].symbol ] = mystrdup(table[i].code);
    if(!answer->codes[ table[i].symbol ])
	  goto error_exit;
  }

  return answer;

error_exit:
  if(answer && answer->codes)
  {
    for(i=0;i<answer->N;i++)
	  free(answer->codes[i]);
    free(answer->codes);
  }
  free(answer);
  return 0;
}

/*
  destroy a Huffman table
  Params: ht - the table to destroy
*/
static void killhuff(HUFFTABLE *ht)
{
  int i;

  if(ht && ht->codes)
  {
    for(i=0;i<ht->N;i++)
	  free(ht->codes[i]);
    free(ht->codes);
  }
  free(ht);
}

/*
  get length parameters for a Huffman table
  Params: ht - the Huffman table
          len - return pointer for 16 length 
  Returns: total number of codes in table
  Notes: len[0] = codes with length 1, len[1] = codes with length 2, etc 
*/
static int getlength(HUFFTABLE *ht, int *len)
{
  int i;

  for(i=0;i<16;i++)
	len[i] = 0;
  for(i=0;i<256;i++)
	if(ht->codes[i])
      len[strlen(ht->codes[i]) -1]++;

  return ht->N;
}

/*
  compare two Huffman entries
  Parmas: e1 - pointer to first element
          e2 - pointer to second element
  Returns: sort order (for qsort())
*/
static int compentries(const void *e1, const void *e2)
{
  const HUFFENTRY *h1 = e1;
  const HUFFENTRY *h2 = e2;

  if(!h1->code && !h2->code)
	return 0;
  if(!h1->code)
	return 1;
  if(!h2->code)
	return -1;

 
  return strcmp(h1->code, h2->code);
}

/*
  get the symbols fromm a Huffman table.
  Params: ht - Huffman table
          sym - return pointer for symbols (256 max) in code order
*/
static void getsymbols(HUFFTABLE *ht, unsigned char *sym)
{
  HUFFENTRY ent[256];
  int i;

  for(i=0;i<256;i++)
  {
    ent[i].code = ht->codes[i];
	ent[i].symbol = i;
  }
  qsort(ent, 256, sizeof(HUFFENTRY), compentries);
  for(i=0;i<ht->N;i++)
	sym[i] = (unsigned char) ent[i].symbol;
}

/*
  write a 16-bit big-endian integer to file
  Params: x - value
          fp - pointer to open file
*/
static void fput16(int x, FILE *fp)
{
  fputc( (x >> 8) & 0xFF, fp);
  fputc(x & 0xFF, fp);
}

/*
  duplicate a string
  Params: str - the string
  Returns: malloced dpulicate
*/
static char *mystrdup(const char *str)
{
  char *answer;

  answer = malloc(strlen(str) + 1);
  if(answer)
	strcpy(answer, str);

  return answer;
}
