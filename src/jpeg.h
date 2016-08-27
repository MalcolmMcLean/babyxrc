#ifndef jpeg_h
#define jpeg_h

unsigned char *loadjpeg(const char *path, int *width, int *height);
int savejpeg(char *path, unsigned char *rgb, int width, int height);

#endif
