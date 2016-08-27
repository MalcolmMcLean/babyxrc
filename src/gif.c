/*
  gif.c - code to load and save .gif format files.

  This is a file from the book Basic Algoithms by Malcolm McLean

  Notes: fixed bug for binary images. codesize in saveraster need to be 2 for
    bi-valued images.

  by Malcolm McLean
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rbtree.h"

typedef struct
{
  int screenwidth;
  int screenheight;
  int global_colourmap;
  int colour_resolution;
  int bits_per_pixel;
  int background;
} SCREEN;

typedef struct
{
  int left;
  int top;
  int width;
  int height;
  int use_local;
  int interlaced;
  int bits_per_pixel;
} LOCAL;

typedef struct
{
  unsigned char *data;
  int N;
  int pos;
  int bit;
} BSTREAM;

typedef struct
{
  int code;
  int prefix;
  int suffix;
  int len;
  unsigned char *ptr;
} ENTRY;

typedef struct
{
  ENTRY table[1 << 13];
  int Nsymbols;
  int codesize;
  int codelen;
  int nextcode;
  BSTREAM *bs;
  RBTREE *tree;
  unsigned char *data;
  int len;
  int pos;
} LZW;

static int loadheader(SCREEN *scr, FILE *fp);
static int loadimageheader(LOCAL *local, FILE *fp);
static int loadtransparency(FILE *fp);
static int loadpalette(FILE *fp, unsigned char *pal, int N);
static int loadraster(unsigned char *out, FILE *fp, int width, int height);
static int saveraster(unsigned char *data, FILE *fp, int width, int height, int codesize);
static int lzwcompress(void *data, int len, void *out, int outlen, int codesize);
static void compresssymbol(LZW *lzw);
static int addstring(LZW *lzw, ENTRY *prefix, int suffixpos, int code);
static ENTRY *findlongest(LZW *lzw, unsigned char *data, int len);
static int compfunc(const void *e1, const void *e2);

static void interlace(unsigned char *out, const unsigned char *in, int width, int height);
static int uninterlace(unsigned char *raster, int width, int height);

static BSTREAM *bstream(unsigned char *data, int N);
static void killbstream(BSTREAM *bs);
static int getbit(BSTREAM *bs);
static int getbits(BSTREAM *bs, int nbits);
static int putbit(BSTREAM *bs, int bit);
static int putbits(BSTREAM *bs, int x, int nbits);
static int flushbs(BSTREAM *bs);
static int fgetu16le(FILE *fp);
static int fputu16le(int x, FILE *fp);

/*
  load a gif file.
  Params: fname - name of file to load
          width - return pointer for image width
          height - return pointer for image height
          pal - return pointer for palette
          transparent - return pointer for transparent index (-1 if none)
  Returns: pointer to pixel datA (0 on failL )
 */
unsigned char *loadgif(char *fname, int *width, int *height, unsigned char *pal, int *transparent)
{
  FILE *fp;
  SCREEN screen;
  LOCAL header;
  unsigned char *answer;
  int trans;

  fp = fopen(fname, "rb");
  if(!fp)
    return 0;

  if(loadheader(&screen, fp) == -1)
  {
    fclose(fp);
    return 0;
  }

  if(screen.global_colourmap)
    loadpalette(fp, pal, 1 << screen.bits_per_pixel);
   
  trans = loadtransparency(fp);
  *transparent = trans;
  
  if( loadimageheader(&header, fp) == -1)
  {
    fclose(fp);
    return 0;
  }
  
  answer = malloc(header.width * header.height);
  
  if(header.use_local)
    loadpalette(fp, pal, 1 << header.bits_per_pixel);

  if(!answer)
  {
    fclose(fp);
    return 0;
  }

  if( loadraster(answer, fp, header.width, header.height) == -1 )
  {
    free(answer);
    fclose(fp);
    return 0;
  }
  if(header.interlaced)
  {
    if(uninterlace(answer, header.width, header.height) == -1)
    {
      free(answer);
      fclose(fp);
      return 0;
    }
  }

  fclose(fp);

  *width = header.width;
  *height = header.height;

  return answer;
}

/*
  save an image in gif format.
  Params: fname - name of file to save.
          data - pixel data (8 bit)
          width - image width
          height - image height
          pal - palette (one entry for each pixel value used)
          palsize - number of entries in palette
          transparent - index of transparent colour (-1 if none)
          important - true if palette in order of importnace
          interlace - true to save in interlaced mode.
  Returns: 0 on success, -1 on fail.
 */
int savegif(char *fname, unsigned char *data, int width, int height, unsigned char *pal, int palsize, int transparent, int important, int interlaced)
{
  FILE *fp;
  int format;
  int log2palsize = 1;
  unsigned char *buff;
  int i;
  
  fp = fopen(fname, "wb");
  if(!fp)
    return -1;

  while( (1 << log2palsize) < palsize)
    log2palsize++;
  
  fwrite("GIF89a", 1, 6, fp);
  fputu16le(width, fp);
  fputu16le(height, fp);
  format = 0x80 | ((log2palsize - 1) << 4) | (log2palsize - 1);
  fputc(format, fp);
  fputc(7, fp);
  fputc(0, fp);
  /* palette */
  for(i=0;i<palsize*3;i++)
    fputc( pal[i], fp);
  while(i < (1 << log2palsize) * 3)
  {
    fputc(0, fp);
    i++;
  }
  
  /* transparent block */
  fputc(33, fp);
  fputc(249, fp);
  fputc(4, fp);
  if(transparent != -1)
    fputc(1, fp);
  else
    fputc(0, fp);
  fputc(0, fp);
  fputc(0, fp);
  if(transparent != -1)
    fputc(transparent, fp);
  else
    fputc(0, fp);
  fputc(0, fp);
 

  /*image header */
  fputc(',', fp);
  fputu16le(0, fp);
  fputu16le(0, fp);
  fputu16le(width, fp);
  fputu16le(height, fp);
  format = interlaced ?  0x40 : 0;
  fputc(format, fp);

  if(interlaced)
  {
    buff = malloc(width * height);
    if(!buff)
    {
      fclose(fp);
      return -1;
    }
    interlace(buff, data, width, height);
    if( saveraster(buff, fp, width, height, log2palsize) == -1 )
    {
      fclose(fp);
      return -1;
    }
    free(buff);
  }
  else
    if( saveraster(data, fp, width, height, log2palsize) == -1)
    {
      fclose(fp);
      return -1;
    }

  fputc(';', fp);
  if(ferror(fp))
  {
    fclose(fp);
    return -1;
  }  
  
  if(fclose(fp) == EOF)
    return -1;

  return 0;
}

/*
  load the header data
  Params: scr - return pointer for header data
          fp - pointer to open file.
  Returns: 0 on success, -1 on fail.
 */
static int loadheader(SCREEN *scr, FILE *fp)
{
  char sig[6];
  int format;

  if(fread(sig, 1, 6, fp) != 6)
    return -1;

  if(sig[0] != 'G' || sig[1] != 'I' || sig[2] != 'F')
    return -1;

  scr->screenwidth = fgetu16le(fp);
  scr->screenheight = fgetu16le(fp);
  
  format = getc(fp);
  scr->global_colourmap = (format & 0x80) ? 1 : 0;
  scr->colour_resolution = ((format >> 4) & 0x07) + 1;
  scr->bits_per_pixel = (format & 0x07) + 1;
  scr->background = fgetc(fp);
  
  if(fgetc(fp) != 0)
    return -1;

  return 0;
}

/*
  load the transparency section.
  Params: fp - pointer to an open file.
  Returns: index of transparent colour, -1 if none
 */
static int loadtransparency(FILE *fp)
{
  int ch;
  int len;
  int pack;
  int transparentindex;
  int delay;

  ch = fgetc(fp);
  if(ch != 0x21)
  {
    ungetc(ch, fp);
    return -1;
  }
  ch = fgetc(fp);
  if(ch != 0xF9)
  {
    ungetc(ch, fp);
    return -1;
  }
  len = fgetc(fp);
  if(len != 4)
  {
    while(len--)
      fgetc(fp);
    return -1;
  }  
  pack = fgetc(fp);
  delay = fgetu16le(fp);
  transparentindex = fgetc(fp);
  if(fgetc(fp) != 0)
    return -1;

  if(pack & 0x01)
    return transparentindex;
  else
    return -1;
} 

/*
  load the image header data
  Params: local - return pointer to local image structure
          fp - pointer to an open file
  Returns: 0 on success, -1 on fail.
 */
static int loadimageheader(LOCAL *local, FILE *fp)
{
  int comma;
  int format;

  while( (comma = fgetc(fp)) != ',')
  {
    printf("extra %d %x\n", comma, comma ); 
    if(comma == EOF)
      return -1;
  }
  local->left = fgetu16le(fp);
  local->top = fgetu16le(fp);
  local->width = fgetu16le(fp);
  local->height = fgetu16le(fp);
  format = fgetc(fp);

  local->use_local = (format & 0x80) ? 1 : 0;
  local->interlaced = (format & 0x40) ? 1 : 0;
  local->bits_per_pixel = (format & 0x07) + 1;

  return 0;
}

/*
  load a palette.
  Params: fp - pointer to an open file
          pal - return pointer for palette
          N - number of palette entries to read
  Returns: 0 on success, -1 on fail.
 */
static int loadpalette(FILE *fp, unsigned char *pal, int N)
{
  int i;

  for(i=0;i<N*3;i++)
    pal[i] = (unsigned char) fgetc(fp);

  return 0;
}

/*
  load the raster data
  Params: out - return pointer for raster data
          fp - pointer to an open file
          width - image width
          higher - image height
  Returns: 0 on success, -1 on fail.         
 */
int loadraster(unsigned char *out, FILE *fp, int width, int height)
{
  unsigned char buff[256];
  int codesize;
  int block;
  int clear;
  int end;
  int nextcode;
  int codelen;
  unsigned char *stream = 0;
  int blen = 0;
  BSTREAM *bs;
  ENTRY *table;
  int pos = 0;
  int ii;
  int len;
  int second;
  int first;
  int tempcode;
  int ch;

  codesize = fgetc(fp);
  block = fgetc(fp);

  clear = 1 << codesize;
  end = clear +1;
  nextcode = end + 1;
  codelen = codesize + 1;

  while(block)
  {
    if( fread(buff, 1, block, fp) != block )
      return -1;
    stream = realloc(stream, blen + block);
    if(!stream)
      return -1;

    memcpy(stream + blen, buff, block);
    blen += block;
    block = fgetc(fp);   
  }
  
  table = malloc(sizeof(ENTRY) * (1 << 12));
  
  for(ii=0;ii<nextcode;ii++)
  {
    table[ii].prefix = 0;
    table[ii].len = 1;
    table[ii].suffix = ii;
  }
  bs = bstream(stream, blen);

  first = clear;
  while(first == clear)
    first = getbits(bs, codelen);
  ch = first;
  out[0] = ch;
  pos = 1;

  while(1)
  {
    second = getbits(bs, codelen);
   
    if(second == clear)
    {
      nextcode = end + 1;
      codelen = codesize + 1;
      first = getbits(bs, codelen);
      
      while(first == clear)
        first = getbits(bs, codelen);
      if(first == end)
        break;
      ch = first;
      out[pos++] = first;
      continue;
    }
    if(second == end)
    {
      break;
    }

    if(second >= nextcode)
    {
      len = table[first].len;
      if(len + pos >= width * height)
        break;

      tempcode = first;
      for(ii=0;ii<len;ii++)
      {
        out[pos + len - ii - 1] = (unsigned char) table[tempcode].suffix;
        tempcode = table[tempcode].prefix;
      }  
      out[pos + len] = (unsigned char) ch;
      pos += len + 1;
    }
    else
    {
      len = table[second].len;
      if(pos + len > width * height)
        break;
      tempcode = second;
   
      for(ii=0;ii<len;ii++)
      {
        ch = table[tempcode].suffix;
        out[pos + len - ii - 1] = (unsigned char) table[tempcode].suffix;
	    tempcode = table[tempcode].prefix;
      }
      pos += len;
    }
    
    if(nextcode < 4096)
    {
      table[nextcode].prefix = first;
      table[nextcode].len = table[first].len + 1;
      table[nextcode].suffix = ch;
  
      nextcode++;  
      if( nextcode == (1 << codelen) )
      {
        codelen++;
    
        if(codelen == 13)
          codelen = 12;
      }
    }

    first = second;
  }

  if(pos != width * height)
    return -1;  
  
  free(table);
  free(stream);

  return 0;
}

/*
  save raster data
  Params: data - the data to save
          fp - pointer to an open file
          width - image width
          height - image height
          codesize - size of code to use.
 */
static int saveraster(unsigned char *data, FILE *fp, int width, int height, int codesize)
{
  unsigned char *buff;
  int size;
  int i;
  int block;

  for(i=0;i<width * height;i++)
    assert(data[i] < (1 << codesize) );

  buff = malloc(width * height * 2);
  if(!buff)
    return -1;

  if(codesize == 1)
    codesize = 2;

  fputc(codesize, fp);

  size = lzwcompress(data, width * height, buff, width * height * 2, codesize);

  if(size == 0)
  {
    free(buff);
    return -1;
  }
  for(i=0;i<size;i+=255)
  {
    block = ((size - i) > 255) ? 255 : (size - i);
    fputc(block, fp);
    fwrite(buff+i, block, 1, fp);
  }
  fputc(0, fp);

  free(buff);

  return 0;
}

/*
  lzw compress some data
  Params: data - the data to compress
          len - data length
          out - output buffer
          outlen - leght of output buffer.
          codesize - length of intial codes
  Returns: length of compressed data, 0 on fail.
*/
static int lzwcompress(void *data, int len, void *out, int outlen, int codesize)
{
  LZW *lzw;
  int i;
  int answer;

  lzw  = malloc(sizeof(LZW));
  if(!lzw)
    return 0;
  lzw->bs = bstream(out, outlen);
  if(!lzw->bs)
    {
      free(lzw);
      return 0;
    }
  lzw->tree = rbtree(compfunc);
  if(!lzw->tree)
    {
      killbstream(lzw->bs);
      free(lzw);
      return 0;
    }
  lzw->Nsymbols = (1 << codesize);
  lzw->codesize = codesize;
  lzw->codelen = codesize + 1;
  lzw->nextcode = lzw->Nsymbols + 2;
  lzw->data = data;
  lzw->len = len;
  lzw->pos = 0;

  for(i=0;i<lzw->Nsymbols + 2;i++)
  {
    lzw->table[i].len = 1;
    lzw->table[i].suffix = i;
    lzw->table[i].code = i;
  }

  putbits(lzw->bs, lzw->Nsymbols, lzw->codelen);

  while(lzw->pos < len)
  {
    if(lzw->bs->pos > outlen - 2)
    {
      killbstream(lzw->bs);
      killrbtree(lzw->tree);
      return 0;
    }
    compresssymbol(lzw);
  }

  putbits(lzw->bs, lzw->Nsymbols+1, lzw->codelen);

  answer = flushbs(lzw->bs);

  killbstream(lzw->bs);
  killrbtree(lzw->tree);
  free(lzw);

  return answer;
}

/*
  compress a symbol.
  Params: lzw - the lzw compressor state.
  Notes: outputs one prefix to the output, then adds prefix and suffix
         to string table.
*/
static void compresssymbol(LZW *lzw)
{
  static int hack =0;
  ENTRY *prefix;

  prefix = findlongest(lzw, &lzw->data[lzw->pos], lzw->len - lzw->pos);

  putbits(lzw->bs, prefix->code, lzw->codelen);

  addstring(lzw, prefix, lzw->pos + prefix->len, lzw->nextcode );
 
  lzw->pos += prefix->len;
  lzw->nextcode++;

  if(lzw->nextcode == (1 << lzw->codelen) + 1)
  {
    if(lzw->codelen == 12)
    {
      putbits(lzw->bs, lzw->Nsymbols, lzw->codelen);
      killrbtree(lzw->tree);
      lzw->tree = rbtree(compfunc);
      lzw->codelen = lzw->codesize + 1; 
      lzw->nextcode = lzw->Nsymbols + 2;
    }
    else
      lzw->codelen++;
  }
}

/*
  add a string to the string table.
  Params: lzw - the compressor
          prefix - the prefix
          suffixpos - inde xof suffix in data
          code - the code for the new entry.
*/
static int addstring(LZW *lzw, ENTRY *prefix, int suffixpos, int code)
{
  ENTRY *entry;
  int res;
  

  assert(lzw->nextcode > 0 && lzw->nextcode < (1 << 13));
  if(suffixpos >= lzw->len)
    return 0;

  entry = &lzw->table[lzw->nextcode];

  entry->code = code;
  entry->prefix = prefix->code;
  entry->suffix = lzw->data[suffixpos];
  entry->len = prefix->len + 1;
  entry->ptr = &lzw->data[suffixpos - prefix->len];
  res = rbt_add(lzw->tree, entry, entry);

  return 0;
}

/*
  find longest matching entry in string table.
  Params: lzw - the compressor
          data - data to search
          len - maximum length
  Returns: the best match.
*/
static ENTRY *findlongest(LZW *lzw, unsigned char *data, int len)
{
  ENTRY key;
  ENTRY *best;
  ENTRY *match;
  int i;

  /*
  int answer = *data;
  for(i=lzw->Nsymbols+2;i<lzw->nextcode;i++)
  {
    int ln = lzw->table[i].len;
    if(len < ln)
    {
      continue;
    }
    if(ln < lzw->table[answer].len)
      continue;
    if(!memcmp(lzw->table[i].ptr, data, ln))
      answer = i;
  }
  return  &lzw->table[answer];  
  */
  
  key.ptr = data;
  best = &lzw->table[*data];
  for(i=2;i<=len;i++)
  {
    key.len = i;
    match = rbt_find(lzw->tree, &key);
      
    if(!match)
      break; 
    else
      best = match; 
  }

  return best;
}

/*
  comparison function.
  Params: e1 - first string.
          e2 - second string.
  Returns: integer specifying ordering.       
*/
static int compfunc(const void *e1, const void *e2)
{
  const ENTRY *ptr1 = e1;
  const ENTRY *ptr2 = e2;
  int len;
  int answer;

  len = ptr1->len;
  if(len > ptr2->len)
    len = ptr2->len;
  answer = memcmp(ptr1->ptr, ptr2->ptr, len);
  if(!answer)
    return ptr1->len - ptr2->len;
  return answer;
}

/*
  interlace an image.
  Params: out - return pointer for interlaced data
          in - input data
          width - image width
          height - image height
 */
static void interlace(unsigned char *out, const unsigned char *in, int width, int height)
{
  int i;

  for(i=0;i<height;i+=8)
  {
    memcpy(out, in + i * width, width);
    out += width;
  }
  for(i=4;i<height;i+=8)
  {
    memcpy(out, in + i *width, width);
    out += width;
  } 
  for(i=2;i<height;i+=4)
  {
    memcpy(out, in + i * width, width);
    out += width;
  }
  for(i=1;i<height;i+=2)
  {
    memcpy(out, in + i * width, width);
    out += width;
  }
}
 
/*
  deinterlace image data
  Params: raster - data to deinterlace
          width - image width
          height - image height
  Returns: 0 on success, -1 on fail. 
 */
static int uninterlace(unsigned char *raster, int width, int height)
{
  unsigned char *buff;
  int i;
  int line = 0;

  buff = malloc(width * height);
  if(!buff)
    return -1;

  for(i=0;i<height; i+=8)
    memcpy(buff + width * i, raster + width * line++, width);
  for(i=4;i<height;i+=8)
    memcpy(buff + width * i, raster + width * line++, width);
  for(i=2;i<height;i+=4)
    memcpy(buff + width * i, raster + width * line++, width);
  for(i=1;i<height;i+=2)
    memcpy(buff + width * i, raster + width * line++, width);

  memcpy(raster, buff, width * height);
  
  free(buff);

  return 0;
} 

/*
  create a bitstream.
  Params: data - the data buffer
          N - size of data buffer
  Returns: constructed object
 */
static BSTREAM *bstream(unsigned char *data, int N)
{
  BSTREAM *answer = malloc(sizeof(BSTREAM));

  if(!answer)
    return 0;
  answer->data = data;
  answer->pos = 0;
  answer->N = N;
  answer->bit = 1;

  return answer;
}

/*
  destroy a bitstream:
  Params: bs - the bitstream to destroy.
 */
static void killbstream(BSTREAM *bs)
{
  free(bs);
}

/*
  read a bit from the bitstream:
  Params: bs - the bitstream;
  Returns: the bit read 
 */
static int getbit(BSTREAM *bs)
{
  int answer = (bs->data[bs->pos] & bs->bit) ? 1 : 0;
  bs->bit <<= 1;
  if(bs->bit == 0x100)
  {
    bs->bit = 1;
    bs->pos++;
  } 
  return answer;
}

/*
  read several bits from a bitstream:
  Params: bs - the bitstream:
          nbits - number of bits to read
  Returns: the bits read (little-endian)
 */
static int getbits(BSTREAM *bs, int nbits)
{
  int i;
  int answer = 0;

  for(i=0;i<nbits;i++)
  {
    answer |= (getbit(bs) << i);
  }

  return answer;
}

/*
  put a bit to the bitstream;
  Params: bs - the bitstream
          bit - set to put a 1, else put a 0
  Returns: 0
 */
static int putbit(BSTREAM *bs, int bit)
{
  if(bit)
    bs->data[bs->pos] |= (unsigned char) bs->bit;
  else
    bs->data[bs->pos] &= (unsigned char ) ~bs->bit;
  bs->bit <<= 1;
  if(bs->bit == 0x100)
  {
    bs->bit = 1;
    bs->pos++;
  }
  return 0;
}

/*
  write several bits to the bitstream
  Params: bs - the stream
          x - the data to write
          nobits - no bits to write
  Returns: 0.
  Notes: writes little-endian 
 */
static int putbits(BSTREAM *bs, int x, int nbits)
{
  int i;

  for(i=0;i<nbits;i++)
    putbit(bs, x & (1 << i));

  return 0;
}

/*
  flush the bitstream:
  Params: bs - the bitstream;
  Returns: number of b=ytes written
 */
static int flushbs(BSTREAM *bs)
{
  if(bs->bit == 1)
    return bs->pos;
  return bs->pos + 1;
}

/*
  read a 16-bit unsigned integer from a file.
  Params: fp - the file.
  Returns: 16 bit little0endian integer
 */
static int fgetu16le(FILE *fp)
{
  int answer;

  answer = fgetc(fp);
  answer |= fgetc(fp) << 8;

  return answer;
}

/*
  write a 16-bit integer to a file (little-endian)
  Params: x - the integer
          fp - pointer to open file
 */
static int fputu16le(int x, FILE *fp)
{
  fputc(x & 0xFF, fp);
  return fputc( ( x >> 8) & 0xFF, fp);
}

int gifmain(int argc, char **argv)
{
  unsigned char pal[256 * 3];
  unsigned char *data;
  int width;
  int height;
  int trans;
  unsigned char *raster;
  int res;
  int i;

  if(argc == 2)
  {
    data = loadgif(argv[1], &width, &height, pal, &trans);
    if(!data)
    {
      printf("loadgif failed\n");
      exit(EXIT_FAILURE);
    }
    printf("W %d h %d\n", width, height);

    if(trans != -1)
    {
      pal[trans*3] = 255;
      pal[trans*3+1] = 150;
      pal[trans*3+2] = 150;
      printf("transparent\n");
    }

    res = savegif("test.gif", data, width, height, pal, 256, -1, 0, 0);
    
    free(data);

    printf("saved test res %d\n", res);
  }

  
  raster = malloc(100 * 150);
  for(i=0;i<100*150;i++)
    raster[i] = ((i%150) < 100) ? 1 : 2;
  savegif("test.gif", raster, 150, 100, pal, 256, -1, 0, 0); 
 
 
  return 0;
}
