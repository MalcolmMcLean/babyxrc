#ifndef gif_h
#define gif_h

unsigned char *loadgif(char *fname, int *width, int *height, unsigned char *pal, int *transparent);
int savegif(char *fname, unsigned char *data, int width, int height, unsigned char *pal, int palsize, int transparent, int important, int interlaced);

#endif
