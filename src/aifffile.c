#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "aifffile.h"

typedef struct
{ 
  short numChannels;
  unsigned long numSampleFrames;
  short sampleSize;
  long sampleRate;
  char compression[4];
} HEADER;

static int loadcommon(FILE *fp, HEADER *header, int extended);
static short *loadsounddata(FILE *fp, HEADER *header);
static int loadchunkheader(FILE *fp, char *chunkid, long *size);

static int skipchunk(FILE *fp, long size);
static int readpstring(FILE *fp, char *out);
static long double freadieee754_80bit(FILE *fp, int bigendian);
static int fget16be(FILE *fp);
static long fget32be(FILE *fp);
static int fget16le(FILE *fp);

short *loadaiff(const char *fname, long *samplerate, int *Nchannels, long 
*Nsamples)
{
  FILE *fp;
  short *answer = 0;

  fp = fopen(fname, "rb");
  if (!fp)
   goto error_exit;
  answer = floadaiff(fp, samplerate, Nchannels, Nsamples);
  if (!answer)
    goto error_exit;
  fclose(fp);

  return answer;
error_exit:
  if (fp)
    fclose(fp);  
  if (samplerate)
     *samplerate = 0;
  if (Nchannels)
    *Nchannels = 0;
  if (Nsamples)
     *Nsamples = 0;
  free(answer);

  return 0;
}

short *floadaiff(FILE *fp, long *samplerate, int *Nchannels, long 
*Nsamples)
{
   short *answer = 0;
   HEADER header;
   char id[5];
   long size;
   int err;
   int extended = 0;
   int hascommon = 0;
   int hassounddata = 0;
   
   err = loadchunkheader(fp, id, &size);
   if (err)
      goto error_exit;
   if (strcmp(id, "FORM") != 0)
     goto error_exit;

   if (fread(id, 1, 4, fp) != 4)
     goto error_exit;
   if (strncmp(id, "AIFF", 4) == 0)
     extended = 0;
   else if(strncmp(id, "AIFC", 4) == 0)
     extended = 1;
   else
     goto error_exit;
   
   while (!hascommon)
   {
      err = loadchunkheader(fp, id, &size);
      if (err)
         goto error_exit;

      if (strcmp(id, "COMM") == 0)
      {
         loadcommon(fp, &header, extended);
         hascommon = 1;
      }
      else
      {
        err = skipchunk(fp, size);
        if (err)
           goto error_exit;
      }
   }

   while (!hassounddata)
   {
      err = loadchunkheader(fp, id, &size);
      if (err)
        goto error_exit;

      if (!strcmp(id, "SSND"))
      {
         answer = loadsounddata(fp, &header);
         if (!answer)
           goto error_exit;
         hassounddata = 1;
      }
      else
      {
        err = skipchunk(fp, size);
        if (err)
           goto error_exit;
      }
   }

   if (samplerate)
     *samplerate = header.sampleRate;
   if (Nchannels)
     *Nchannels = header.numChannels;
   if (Nsamples)
     *Nsamples = (long) header.numSampleFrames;
   return answer;

error_exit:
    if (samplerate)
       *samplerate = 0;
   if (Nchannels)
     *Nchannels = 0;
   if (Nsamples)
     *Nsamples = 0;
   free(answer);
   return 0;

}

static int loadcommon(FILE *fp, HEADER *header, int extended)
{
   long double x;
   header->numChannels = fget16be(fp);
   header->numSampleFrames = fget32be(fp);
   header->sampleSize = fget16be(fp);
   x = freadieee754_80bit(fp, 1);
   header->sampleRate = (long) x;

   if (extended)
   {
      char id[4];
      char temp[256];
      fread(id, 1, 4, fp);
      readpstring(fp, temp);
      strncpy(header->compression, id, 4);  
   }
   else
   {
      strncpy(header->compression, "NONE", 4);
   }

   if (feof(fp))
     return -1;
   return 0;
}

static short *loadsounddata(FILE *fp, HEADER *header)
{
   short *answer = 0;
   long offset;
   long blocksize;
   long i;
   int bigendian = 1;

   answer = malloc(header->numSampleFrames * header->numChannels * 
sizeof(short));
   if (!answer)
     goto error_exit;

   offset = fget32be(fp);
   blocksize = fget32be(fp);

   for (i =0; i <offset; i++)
   {
     if (fgetc(fp) == EOF)
        goto error_exit;
   }

   if (strncmp(header->compression, "NONE", 4) == 0)
   {
      bigendian = 1;
   }
   else if(strncmp(header->compression, "sowt", 4) == 0)
   {
     bigendian = 0;
   }
   else
   {
     goto error_exit;
   }

   if (header->sampleSize != 16)
      goto error_exit;
 

   for (i = 0; i < header->numSampleFrames * header->numChannels; i++)
   {
      if (bigendian)
      	answer[i] = fget16be(fp);
      else
        answer[i] = fget16le(fp);
   }
   if (feof(fp))
     goto error_exit;

   return answer;

   error_exit:
     free(answer);
   return 0;
}


static int loadchunkheader(FILE *fp, char *chunkid, long *size)
{
  size_t Nread;
  Nread  = fread(chunkid, 1, 4, fp);
  if (Nread != 4)
    return -1;
  chunkid[4] = 0;
  *size = fget32be(fp);

  if (feof(fp))
    return -1;
  return 0;
}

static int skipchunk(FILE *fp, long size)
{
   long i;
   for (i = 0; i < size; i++)
   {
     if (fgetc(fp) == EOF)
       return -1;
   };
   if (size % 2)
      fgetc(fp);
   return 0;
}

static int readpstring(FILE *fp, char *out)
{
   int len;
   size_t Nread;
   char buff[256];
   
   if (out == 0)
      out = buff;

   len = fgetc(fp);
   if (len == EOF)
     return -1;
   Nread = fread(out, 1, len, fp);
   if (Nread != len)
   {
     out[Nread] = 0;
     return -1;
   }
   out[len] = 0;
   if ((len +1) % 2)
     if (fgetc(fp) == EOF)
       return -1;
    
   return 0;   
} 

static long double freadieee754_80bit(FILE *fp, int bigendian)
{
	unsigned char buff[10];
	int i;
	double fnorm = 0.0;
	unsigned char temp;
	int sign;
	int exponent;
	double bitval;
	int maski, mask;
	int expbits = 15;
	int significandbits = 63;
	int shift;
	long double answer;

	/* read the data */
	for (i = 0; i < 10; i++)
		buff[i] = fgetc(fp);
	/* just reverse if not big-endian*/
	if (!bigendian)
	{
		for (i = 0; i < 5; i++)
		{
			temp = buff[i];
			buff[i] = buff[8 - i - 1];
			buff[10 - i - 1] = temp;
		}
	}
	sign = buff[0] & 0x80 ? -1 : 1;
	/* expoent in raw format*/
	exponent = ((buff[0] & 0x7F) << 8) | ((buff[1] & 0xFF));

	/* read inthe mantissa. Top bit is 0.5, the successive bits half*/
	bitval = 0.5;
	maski = 2;
	mask = 0x40;
	for (i = 0; i < significandbits; i++)
	{
		if (buff[maski] & mask)
			fnorm += bitval;

		bitval /= 2.0;
		mask >>= 1;
		if (mask == 0)
		{
			mask = 0x80;
			maski++;
		}
	}
	/* handle zero specially */
	if (exponent == 0 && fnorm == 0)
		return 0.0;

	shift = exponent - ((1 << (expbits - 1)) - 1); /* exponent = shift 
+ bias */
	/* nans have exp 4096 and non-zero mantissa */
	if (shift == 4096 && fnorm != 0)
		return sqrt(-1.0);
	/*infinity*/
	if (shift == 4096 && fnorm == 0)
	{

#ifdef INFINITY
		return sign == 1 ? INFINITY : -INFINITY;
#endif
		return	(sign * 1.0) / 0.0;
	}
	if (shift > -4095)
	{
		answer = ldexpl(fnorm + 1.0, shift);
		return answer * sign;
	}
	else
	{
		/* denormalised numbers */
		if (fnorm == 0.0)
			return 0.0;
		shift = -4094;
		while (fnorm < 1.0)
		{
			fnorm *= 2;
			shift--;
		}
		answer = ldexpl(fnorm, shift);
		return answer * sign;
	}
}


static int fget16be(FILE *fp)
{
	int c1, c2;

	c2 = fgetc(fp);
	c1 = fgetc(fp);

	return ((c2 ^ 128) - 128) * 256 + c1;
}

static long fget32be(FILE *fp)
{
	int c1, c2, c3, c4;

	c4 = fgetc(fp);
	c3 = fgetc(fp);
	c2 = fgetc(fp);
	c1 = fgetc(fp);
	return ((c4 ^ 128) - 128) * 256 * 256 * 256 + c3 * 256 * 256 + c2 
* 256 + c1;
}

static int fget16le(FILE *fp)
{
        int c1, c2;
        
        c1 = fgetc(fp);
        c2 = fgetc(fp); 
        
        return ((c2 ^ 128) - 128) * 256 + c1;
}

