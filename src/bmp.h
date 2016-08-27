#ifndef bmp_h
#define bmp_h

int bmpgetinfo(char *fname, int *width, int *height);
unsigned char *loadbmp(char *fname, int *width, int *height);
unsigned char *loadbmp8bit(char *fname, int *width, int *height, unsigned char *pal);
unsigned char *loadbmp4bit(char *fname, int *width, int *height, unsigned char *pal);
int savebmp(char *fname, unsigned char *rgb, int width, int height);
int savebmp8bit(char *fname, unsigned char *data, int width, int height, unsigned char *pal);
int savebmp4bit(char *fname, unsigned char *data, int width, int height, unsigned char *pal);
int savebmp1bit(char *fname, unsigned char *data, int width, int height, unsigned char *pal);

#endif


