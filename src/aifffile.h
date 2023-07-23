#ifndef aifffile_h
#define aifffile_h

#include <stdio.h>

short *loadaiff(const char *fname, long *samplerate, int *Nchannels, long 
*Nsamples);
short *floadaiff(FILE *fp, long *samplerate, int *Nchannels, long 
*Nsamples);

#endif
