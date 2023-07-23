//
//  loadmp3.h
//  babyxrc
//
//  Created by Malcolm McLean on 23/07/2023.
//

#ifndef loadmp3_h
#define loadmp3_h

short *loadmp3(const char *fname, long *samplerate, int
               *Nchannels, long *Nsamples);
short *mp3streamdecompress(unsigned char *mp3_stream, int stream_size,
   long *samplerate, int *Nchannels, long *Nsamples);


#endif /* loadmp3_h */
