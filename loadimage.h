#ifndef loadimage_h
#define loadimage_h

unsigned char *loadrgba(char *fname, int *width, int *height, int *err);
unsigned char *loadassvgwithsize(char *fname, int width, int height);

#endif
