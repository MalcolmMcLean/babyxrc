#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wavfile.h"

typedef struct
{
   int formattype;
   int numchannels;
   long samplerate;
   int bitspersample;
   long datasize;
} WAVHEADER;

static int loadheader(FILE *fp, WAVHEADER *header);
static int saveheader(FILE *fp, long samplerate, int Nchannels, long 
Nsamples);
static short *load8bitpcm(FILE *fp, long N);
static short *load16bitpcm(FILE *fp, long N);
static void fput32le(long x, FILE *fp);
static void fput16le(int x, FILE *fp);
static long fget32le(FILE *fp);
static int fget16le(FILE *fp);

short *loadwav(const char *fname, long *samplerate, int *Nchannels, long 
*Nsamples)
{
   FILE *fp = 0;
   short *answer = 0;

   fp = fopen(fname, "rb");
   if (!fp)
      goto error_exit;
   answer = floadwav(fp, samplerate, Nchannels, Nsamples);
   if (!answer)
     goto error_exit;
   fclose(fp);

   return answer;

error_exit:
   if (fp)
     fclose(fp);
   if (samplerate)
     *samplerate = 0;
   if(Nchannels)
     *Nchannels = 0;
   if (Nsamples)
     *Nsamples = 0;
   
  free(answer);

  return 0;

}

short *floadwav(FILE *fp, long *samplerate, int *Nchannels, long 
*Nsamples)
{
   WAVHEADER header;
   short *answer = 0;
   long N;
   int err;

   err = loadheader(fp, &header);
   if (err == -1)
     goto error_exit;

   if (header.bitspersample == 8)
   {
      N = header.datasize;
      answer =load8bitpcm(fp, N);
      if (!answer)
        goto error_exit;
   }
   else if (header.bitspersample == 16)
   {
     N = header.datasize / 2;
     answer = load16bitpcm(fp, N);
     if (!answer)
       goto error_exit;
   }
   else
     goto error_exit;
   
   if (header.numchannels <= 0)
     goto error_exit;

   if (samplerate)
     *samplerate = header.samplerate;
   if (Nchannels)
     *Nchannels = header.numchannels;
   if (Nsamples)
     *Nsamples = N / header.numchannels;

   return answer;

 error_exit:
   if (samplerate)
     *samplerate = 0;
   if(Nchannels)
     *Nchannels = 0;
   if (Nsamples)
     *Nsamples = 0;
  
  free(answer);
 
  return 0;
   
}

int savewav(const char *fname, const short *pcm, long 
samplerate, int Nchannels, 
long Nsamples)
{
   int answer = 0;
   FILE *fp;

   fp = fopen(fname, "wb");
   if (!fp)
      return -1;

   answer = fsavewav(fp, pcm, samplerate, Nchannels, Nsamples);
   fclose(fp);

   return answer;
}

int fsavewav(FILE *fp, const short *pcm, long samplerate, int 
Nchannels, long Nsamples)
{
   int err = 0;
   long i;

   err = saveheader(fp, samplerate, Nchannels, Nsamples);
   if (err)
      return err;
   
   for (i = 0; i < Nsamples * Nchannels; i++)
   {
      fput16le(pcm[i], fp);
   }
   
   if (ferror(fp))
     return -1;
   return 0;
}

static int loadheader(FILE *fp, WAVHEADER *header)
{
   char chunk[4];
   long size;
   long fmtsize;

   fread(chunk, 4, 1, fp);
   if (strncmp(chunk, "RIFF", 4) != 0)
     return -1;
   size = fget32le(fp);
   fread(chunk, 4, 1, fp);
   if (strncmp(chunk, "WAVE", 4) != 0)
     return -1;
    fread(chunk, 4, 1, fp);
    if (strncmp(chunk, "fmt", 3) != 0)
      return -1;
    fmtsize = fget32le(fp);;
    header->formattype = fget16le(fp);
    header->numchannels = fget16le(fp);
    header->samplerate = fget32le(fp);
    fget32le(fp);
    fget16le(fp);
    header->bitspersample = fget16le(fp);
    fread(chunk, 4, 1, fp);
    if (strncmp(chunk, "data", 4) != 0)
       return -1;
    header->datasize = fget32le(fp);
   
    return 0;
}

static int saveheader(FILE *fp, long samplerate, int Nchannels, long 
Nsamples)
{
   long filesize;

   fwrite("RIFF", 1, 4, fp);
   filesize = Nsamples * Nchannels * 2 + 44;
   fput32le(filesize -8, fp);
   fwrite("WAVE", 1, 4, fp);
   fwrite("fmt ", 1, 4, fp);
   fput32le(16, fp);
   fput16le(1, fp);
   fput16le(Nchannels, fp);
   fput32le(samplerate, fp);
   fput32le(samplerate * Nchannels * 2, fp);
   fput16le(Nchannels * 2, fp);
   fput16le(16, fp);
   fwrite("data", 1, 4, fp);
   fput32le(Nchannels * Nsamples * 2, fp);
   
   if (ferror(fp))
     return -1;

   return 0;   
}

static short *load8bitpcm(FILE *fp, long N)
{
  long i;
  short *answer;

  answer = malloc(N * sizeof(short));
  if (!answer)
    return 0;
  for (i = 0; i < N; i++)
    answer[i] = (fgetc(fp) - 128) * 256;
  if (feof(fp))
  {
     free(answer);
     return 0;
  } 

  return answer;
}

static short *load16bitpcm(FILE *fp, long N)
{
   long i;
   short *answer;

   answer = malloc(N * sizeof(short));
   if (!answer)
      return 0;
   for (i = 0;i < N; i++)
     answer[i] = fget16le(fp);
   if (feof(fp))
   {
      free(answer);
      return 0;
   }

   return answer;
}

static void fput32le(long x, FILE *fp)
{
  fputc(x & 0xFF, fp);
  fputc((x >> 8) & 0xFF, fp);
  fputc((x >> 16) & 0xFF, fp);
  fputc((x >> 24) & 0xFF, fp);
}

static void fput16le(int x, FILE *fp)
{
   fputc(x & 0xFF, fp);
   fputc((x >> 8) & 0xFF, fp);
}

static long fget32le(FILE *fp)
{
   long c1, c2, c3, c4;

	c1 = fgetc(fp);
	c2 = fgetc(fp);
	c3 = fgetc(fp);
	c4 = fgetc(fp);
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

int wavfilemain(int argc, char **argv)
{
   short *pcm;
   
   long Nsamples;
   long samplerate;
   int Nchannels;
   int err;

   if (argc == 2)
   {
      pcm = loadwav(argv[1], &samplerate, &Nchannels, &Nsamples);
      printf("%p: rate %ld channels %d N %ld\n", (void *) pcm, samplerate, 
Nchannels, Nsamples); 
      err = savewav("out.wav", pcm, samplerate, Nchannels, Nsamples);
      printf("Error %d\n", err);
   }

   return 0;
}
