#ifndef wavfile_h
#define wavfile_h

#include <stdio.h>

short *loadwav(const char *fname, long *samplerate, int *Nchannels, long 
*Nsamples);
int savewav(const char *fname, const short *pcm, long samplerate, int 
Nchannels, long Nsamples);
short *floadwav(FILE *fp, long *samplerate, int *Nchannels, long 
*Nsamples);
int fsavewav(FILE *fp, const short *pcm, long samplerate, int Nchnanels, 
long Nsamples);

#endif
