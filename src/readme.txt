This is the Baby X resource compiler.

It is designed to take resources - images, fonts, strings and so
on - and package them up as compileable C source. So you include
the source in your project.

It takes a simple definition file which lists the resources. The 
path will be relative to where you evoke the resource compiler.

<BabyXRC>
<font src = "Skater Girls Rock.ttf", name = "skatergirls", points = "20"> </font> 
<image src = "valST.GIF", name = "valineST", width = "52", height = "44"> </image>
<cursor src ="yellow_smiley.cur", name ="yellowsmiley_cur"> </cursor>
</BabyXRC> 

That's essentially it. It can load bmp, jpeg, gif, png, tiff and svg images. 
It automatically rescales them if required. The output is a 32 bit rgba
buffer.
Cursors are .cur file. They're essentially the same as images, but with
a hotpoint defined.
You can also package up text and binaries. Code to resample audio files
is in, but not tested yet.

Fonts are converted to a raster format that is simple to write to a
display. 

#ifndef font_h
#define font_h

/* bitmap font structure */
struct bitmap_font {
  unsigned char width;		/* max. character width */
  unsigned char height;		/* character height */
  int ascent;		        /* font ascent */
  int descent;		        /* font descent */
  unsigned short Nchars;        /* number of characters in font */
  unsigned char *widths;	/* width of each character */
  unsigned short *index;	/* encoding to character index */
  unsigned char *bitmap;	/* bitmap of all characters */
};

struct bitmap_font vera12_font = 
{
20, /* max width */
19, /* height */
15, /* ascent */
4, /* descent */
257, /* Nchars */
vera12_widths, /* widths */
vera12_index, /* index */
vera12_bitmap /* bitmap */
};

The bitmap is an alpha map / greyscale, depending how you wish to think of it.
The characters are in cells width * height, and right padded. To get from
unicode to the internal index, do a binary search on the index member. 
You then read off the width, and index into the bitmap 

unsigned char *cell  = ptr->bitmamp + ptr->width * ptr->height * index;
 
The program is free for any use, but please respect the copyright licences
of the components.