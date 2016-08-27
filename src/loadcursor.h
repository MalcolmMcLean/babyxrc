#ifndef loadcursor_h
#define loadcursor_h

typedef struct
{
	int width;
	int height;
	unsigned char *rgba;
	int hotx;
	int hoty;
} BBX_CURSOR;

BBX_CURSOR *loadcursor(const char *filename, int *err);

#endif
