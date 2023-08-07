#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minimp3.h"
#include "loadmp3.h"

#define BUFFER_COUNT 2

static unsigned char *slurpb(const char *fname, int *len);

short *loadmp3(const char *fname, long *samplerate, int 
*Nchannels, long *Nsamples)
{
   unsigned char *bytes = 0;
   short *answer = 0;
   int N = 0;

   bytes = slurpb(fname, &N);
   if (!bytes)
     goto error_exit;

   answer = mp3streamdecompress(bytes, N, samplerate, Nchannels, 
Nsamples);
   if (!answer)
      goto error_exit;
   free(bytes);
   return answer;

error_exit:
   if (samplerate)
     *samplerate = 0;
   if (Nchannels)
     *Nchannels = 0;
   if (Nsamples)
     *Nsamples = 0;
   free(bytes);
   free(answer);

   return 0;
}

short *mp3streamdecompress(unsigned char *mp3_stream, int stream_size,
   long *samplerate, int *Nchannels, long *Nsamples)
{
   int i;
   unsigned char *inptr;
   unsigned char *stream_pos;
   signed short sample_buffer[MP3_MAX_SAMPLES_PER_FRAME * BUFFER_COUNT];
   int bytes_left;
   int byte_count;
   mp3_info_t info;
   mp3_decoder_t mp3 = 0;;
   size_t Nout = 0;
   short *answer = 0;

   stream_pos = mp3_stream;
   bytes_left = stream_size - 128;
  
   mp3 = mp3_create();
   if (!mp3)
     goto error_exit;
   do
   {
     byte_count = mp3_decode(mp3, stream_pos, bytes_left, sample_buffer,
       &info); 
     bytes_left -= byte_count;
     stream_pos += byte_count;
     Nout += info.audio_bytes;
   } while(bytes_left > 0 && byte_count);
     
   mp3_free(mp3);

   mp3 = mp3_create();
   if (!mp3)
     goto error_exit;
   answer = malloc(Nout);
   if (!answer)
     goto error_exit;

   Nout = 0;
   stream_pos = mp3_stream;
   bytes_left = stream_size - 128;

   do
   {
     byte_count = mp3_decode(mp3, stream_pos, bytes_left, sample_buffer,
       &info);
     bytes_left -= byte_count;
     stream_pos += byte_count;
     memcpy(answer + Nout/(sizeof(short)), sample_buffer, info.audio_bytes);
     Nout += info.audio_bytes;
   } while(bytes_left > 0 && byte_count);

   mp3_free(mp3);

   if (samplerate)
      *samplerate = info.sample_rate;
   if (Nchannels)
     *Nchannels = info.channels;
   if (Nsamples)
     *Nsamples = Nout / (info.channels * sizeof(short));;

   return answer;

error_exit:
   if (samplerate)
      *samplerate = 0;
   if (Nchannels)
     *Nchannels = 0;
   if (Nsamples)
     *Nsamples = 0;
   if (mp3)
   	mp3_free(mp3);
   free(answer);
   return 0;
}

static unsigned char *slurpb(const char *fname, int *len)

{
	FILE *fp;
	unsigned char *answer = 0;
	unsigned char *temp;
	int capacity = 1024;
	int N = 0;
	int ch;

	fp = fopen(fname, "rb");
	if (!fp)
		return 0;
	answer = malloc(capacity);
	if (!answer)
		goto out_of_memory;
	while ( (ch = fgetc(fp)) != EOF)
	{
		answer[N++] = ch;
		if (N >= capacity)
		{
			temp = realloc(answer, capacity + capacity / 2);
			if (!temp)
				goto out_of_memory;
			answer = temp;
			capacity = capacity + capacity / 2;
		}
	}
	*len = N;
	fclose(fp);
	return answer;
out_of_memory:
	fclose(fp);
	*len = -1;
	free(answer);
	return 0;
}

/*
#include "wavfile.h"

int main(int argc, char **argv)
{
   short *pcm;
   long samplerate;
   int Nchannels;
   long Nsamples;

   if (argc == 2)
   {
      pcm = loadmp3(argv[1], &samplerate, &Nchannels, &Nsamples);
      printf("samplerate %ld channels %d Nsamples %ld\n",
        samplerate, Nchannels, Nsamples);
     if (pcm)
       savewav("out.wav", pcm, samplerate, Nchannels, Nsamples);
     free(pcm);
   }

   return 0;
}

*/
