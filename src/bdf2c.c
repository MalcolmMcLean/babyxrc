///
///	@file bdf2c.c		@brief BDF Font to C source convertor
///
///	Copyright (c) 2009, 2010 by Lutz Sammer.  All Rights Reserved.
///
///	Contributor(s): 
///
///	License: AGPLv3
///
///	This program is free software: you can redistribute it and/or modify
///	it under the terms of the GNU Affero General Public License as
///	published by the Free Software Foundation, either version 3 of the
///	License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU Affero General Public License for more details.
///
///	$Id$
//////////////////////////////////////////////////////////////////////////////

///
///	@mainpage
///		bdf2c - converts bdf font files into C include files.
///
///		The Bitmap Distribution Format (BDF) is a file format for
///		storing bitmap fonts. The content is presented as a text file
///		that is intended to be human and computer readable.
///
///	BDF input:
///	@code
///	STARTCHAR A
///	ENCODING 65
///	SWIDTH 568 0
///	DWIDTH 8 0
///	BBX 8 13 0 -2
///	BITMAP
///	00
///	38
///	7C
///	C6
///	C6
///	C6
///	FE
///	C6
///	C6
///	C6
///	C6
///	00
///	00
///	ENDCHAR
///	@endcode
///
///	The result looks like this:
///        We have a dump of the font bitmaps, set pixels 0xFF, unset pixels
///           0x00. All maps maxwidth * height
///        We have a coding table, giving unicode values for each glyph
///        We have a width table.             
///	   We have the fot structure
///

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>

#define VERSION "5"			///< version of this application

//////////////////////////////////////////////////////////////////////////////

int Outline;				///< true generate outlined font

//////////////////////////////////////////////////////////////////////////////

static int mystrcasecmp(const char *s1, const char *s2)
{
	while (*s1 && *s2)
	{
		if (toupper(*s1) != toupper(*s2))
			return toupper(*s2) - toupper(*s1);
		s1++;
		s2++;
	}
	return *s1 - *s2;
}
///
///	Create our header file.
///
///	@param out	file stream for output
///
void CreateFontHeaderFile(FILE * out)
{
    fprintf(out,
	"// (c) 2009, 2010 Lutz Sammer, License: AGPLv3\n\n"
	"\t/// bitmap font structure\n" "struct bitmap_font {\n"
	"\tunsigned char Width;\t\t///< max. character width\n"
	"\tunsigned char Height;\t\t///< character height\n"
	"\tint Ascent;\t\t///< font ascent\n"
	"\tint Descent;\t\t///< font descent\n"
	"\tunsigned short Chars;\t\t///< number of characters in font\n"
	"\tconst unsigned char *Widths;\t///< width of each character\n"
	"\tconst unsigned short *Index;\t///< encoding to character index\n"
	"\tconst unsigned char *Bitmap;\t///< bitmap of all characters\n"
	"};\n\n");

}

//////////////////////////////////////////////////////////////////////////////

///
///	Print header for c file.
///
///	@param out	file stream for output
///	@param name	font variable name in C source file
///
static void Header(FILE * out, const char *name)
{
   
    fprintf(out,
	"\t/// character bitmap for each encoding\n"
	"static unsigned char %s_bitmap[] = {\n", name);
}

///
///	Print width table for c file
///
///	@param out		file stream for output
///	@param name		font variable name in C source file
///	@param width_table	width table read from BDF file
///	@param chars		number of characters in width table
///
static void WidthTable(FILE * out, const char *name, const unsigned *width_table,
    int chars)
{
    fprintf(out,
	"\t/// character width for each encoding\n"
	"static unsigned char %s_widths[] = {\n", name);
    while (chars--) {
	fprintf(out, "\t%u,\n", *width_table++);
    }
    fprintf(out, "};\n");
}

///
///	Print encoding table for c file
///
///	@param out		file stream for output
///	@param name		font variable name in C source file
///	@param encoding_table	encoding table read from BDF file
///	@param chars		number of characters in encoding table
///
static void EncodingTable(FILE * out, const char *name,
    const unsigned *encoding_table, int chars)
{
    fprintf(out,
	"static unsigned short %s_index[] = {\n", name);
    while (chars--) {
	fprintf(out, "\t%u,\n", *encoding_table++);
    }
    fprintf(out, "};\n");
}

///
///	Print footer for c file.
///
///	@param out		file stream for output
///	@param name		font variable name in C source file
///	@param width		character width of font
///	@param height		character height of font
///	@param chars		number of characters in font
///
static void Footer(FILE * out, const char *name, int ascent, int descent, int width, int height, int chars)
{
  fprintf(out, "struct bitmap_font %s_font =\n", name);
  fprintf(out, "{\n");
  fprintf(out, "%d, /* max width */\n", width);
  fprintf(out, "%d, /* height */\n", height);
  fprintf(out, "%d, /* ascent */\n", ascent);
  fprintf(out, "%d, /* descent */\n", descent);
  fprintf(out, "%d, /* N chars */\n", chars);
  fprintf(out, "%s_widths, /* width */\n", name);
  fprintf(out, "%s_index, /* index */\n", name);
  fprintf(out, "%s_bitmap, /* bitmap */\n", name);
  fprintf(out, "};\n\n");
}

///
///	Dump character.
///
///	@param out	file stream for output
///	@param bitmap	input bitmap
///	@param width	character width
///	@param height	character height
///
static void DumpCharacter(FILE * out, unsigned char *bitmap, int width, int height)
{
    int x;
    int y;
    int c;
    int mask;

    for (y = 0; y < height; ++y) 
    {
	fputc('\t', out);
	for (x = 0; x < width; x ++) 
        {
	    c = bitmap[y * ((width + 7) / 8) + x / 8];
            mask = 0x0080 >> (x % 8); 
	    if (c & mask) {
	      fprintf(out, "0xFF, ");
	    } else {
	      fprintf(out, "0x00, ");
	    }
	}
	fputc('\n', out);
    }
}

///
///	Hex ascii to integer
///
///	@param p	hex input character (0-9a-fA-F)
///
///	@returns converted integer
///
static int Hex2Int(const char *p)
{
    if (*p <= '9') {
	return *p - '0';
    } else if (*p <= 'F') {
	return *p - 'A' + 10;
    } else {
	return *p - 'a' + 10;
    }
}

///
///	Rotate bitmap.
///
///	@param bitmap	input bitmap
///	@param shift	rotate counter (0-7)
///	@param width	character width
///	@param height	character height
///
static void RotateBitmap(unsigned char *bitmap, int shift, int width, int height)
{
    int x;
    int y;
    int c;
    int o;

    if (shift < 0 || shift > 7) {
	fprintf(stderr, "This shift isn't supported\n");
	exit(-1);
    }

    for (y = 0; y < height; ++y) {
	o = 0;
	for (x = 0; x < width; x += 8) {
	    c = bitmap[y * ((width + 7) / 8) + x / 8];
	    bitmap[y * ((width + 7) / 8) + x / 8] = c >> shift | o;
	    o = c << (8 - shift);
	}
    }
}

static void MoveDownBitmap(unsigned char *bitmap, int shift, int width, int height)
{
  int x, y;
 
  width = (width + 7)/8;
  for(y=height-1;y>=shift;y--)
    for(x=0;x<width;x++)
      bitmap[y*width+x] = bitmap[(y-shift)*width+x];
  for(y=0;y<shift;y++)
    for(x=0;x<width;x++)
      bitmap[y*width+x] = 0;
}

///
///	Outline character.  Create an outline font from normal fonts.
///
///	@param bitmap	input bitmap
///	@param width	character width
///	@param height	character height
///
static void OutlineCharacter(unsigned char *bitmap, int width, int height)
{
    int x;
    int y;
    unsigned char *outline;

    outline = malloc(((width + 7) / 8) * height);
    memset(outline, 0, ((width + 7) / 8) * height);
    for (y = 0; y < height; ++y) {
	for (x = 0; x < width; ++x) {
	    // Bit not set check surroundings
	    if (~bitmap[y * ((width + 7) / 8) + x / 8] & (0x80 >> x % 8)) {
		// Upper row bit was set
		if (y
		    && bitmap[(y - 1) * ((width + 7) / 8) +
			x / 8] & (0x80 >> x % 8)) {
		    outline[y * ((width + 7) / 8) + x / 8] |= (0x80 >> x % 8);
		    // Previous bit was set
		} else if (x
		    && bitmap[y * ((width + 7) / 8) + (x -
			    1) / 8] & (0x80 >> (x - 1) % 8)) {
		    outline[y * ((width + 7) / 8) + x / 8] |= (0x80 >> x % 8);
		    // Next bit was set
		} else if (x < width - 1
		    && bitmap[y * ((width + 7) / 8) + (x +
			    1) / 8] & (0x80 >> (x + 1) % 8)) {
		    outline[y * ((width + 7) / 8) + x / 8] |= (0x80 >> x % 8);
		    // below row was set
		} else if (y < height - 1
		    && bitmap[(y + 1) * ((width + 7) / 8) +
			x / 8] & (0x80 >> x % 8)) {
		    outline[y * ((width + 7) / 8) + x / 8] |= (0x80 >> x % 8);
		}
	    }
	}
    }
    memcpy(bitmap, outline, ((width + 7) / 8) * height);
    free(outline);
}

///
///	Read BDF font file.
///
///	@param bdf	file stream for input (bdf file)
///	@param out	file stream for output (C source file)
///	@param name	font variable name in C source file
///
///	@todo bbx isn't used to correct character position in bitmap
///
void ReadBdf(FILE * bdf, FILE * out, const char *name)
{
    char linebuf[1024];
    char *s;
    char *p;
    int fontboundingbox_width;
    int fontboundingbox_height;
    int fontboundingbox_x;
    int fontboundingbox_y;
    int font_ascent = -1;
    int font_descent = -1;
    int chars;
    int i;
    int j;
    int n;
    int scanline;
    char charname[1024];
    int encoding;
    int bbx;
    int bby;
    int bbw;
    int bbh;
    int width;
    int yshift;
    unsigned *width_table;
    unsigned *encoding_table;
    unsigned char *bitmap;

    fontboundingbox_width = 0;
    fontboundingbox_height = 0;
    chars = 0;
    for (;;) {
	if (!fgets(linebuf, sizeof(linebuf), bdf)) {	// EOF
	    break;
	}
	if (!(s = strtok(linebuf, " \t\n\r"))) {	// empty line
	    break;
	}
	if (!mystrcasecmp(s, "FONTBOUNDINGBOX")) {
	    p = strtok(NULL, " \t\n\r");
	    fontboundingbox_width = atoi(p);
	    p = strtok(NULL, " \t\n\r");
	    fontboundingbox_height = atoi(p);
            p = strtok(NULL, " \t\n\r");
	    fontboundingbox_x = atoi(p);
	    p = strtok(NULL, " \t\n\r");
	    fontboundingbox_y  = atoi(p);
	} else if (!mystrcasecmp(s, "CHARS")) {
	    p = strtok(NULL, " \t\n\r");
	    chars = atoi(p);
	    break;
	} else if (!mystrcasecmp(s, "FONT_ASCENT")) {
            p = strtok(NULL, " \t\n\r");
	    font_ascent = atoi(p);
        } else if (!mystrcasecmp(s, "FONT_DESCENT")) {
            p = strtok(NULL, " \t\n\r");
	    font_descent = atoi(p);
        }
       
    }
    //
    //	Some checks.
    //
    if (fontboundingbox_width <= 0 || fontboundingbox_height <= 0) {
	fprintf(stderr, "Need to know the character size\n");
	exit(-1);
    }
    if (chars <= 0) {
	fprintf(stderr, "Need to know the number of characters\n");
	exit(-1);
    }
    if (Outline) {			// Reserve space for outline border
	fontboundingbox_width++;
	fontboundingbox_height++;
    }
    //
    //	Allocate tables
    //
    width_table = malloc(chars * sizeof(*width_table));
    if (!width_table) {
	fprintf(stderr, "Out of memory\n");
	exit(-1);
    }
    encoding_table = malloc(chars * sizeof(*encoding_table));
    if (!encoding_table) {
	fprintf(stderr, "Out of memory\n");
	exit(-1);
    }
    /*	FIXME: needed for proportional fonts.
       offset_table = malloc(chars * sizeof(*offset_table));
       if (!offset_table) {
       fprintf(stderr, "Out of memory\n");
       exit(-1);
       }
     */
    bitmap =
	malloc(((fontboundingbox_width + 7) / 8) * fontboundingbox_height);
    if (!bitmap) {
	fprintf(stderr, "Out of memory\n");
	exit(-1);
    }

    Header(out, name);

    scanline = -1;
    n = 0;
    encoding = -1;
    bbx = 0;
    bby = 0;
    bbw = 0;
    bbh = 0;
    width = INT_MIN;
    strcpy(charname, "unknown character");
    for (;;) {
	if (!fgets(linebuf, sizeof(linebuf), bdf)) {	// EOF
	    break;
	}
	if (!(s = strtok(linebuf, " \t\n\r"))) {	// empty line
	    break;
	}
	// printf("token:%s\n", s);
	if (!mystrcasecmp(s, "STARTCHAR")) {
	    p = strtok(NULL, " \t\n\r");
	    strcpy(charname, p);
	} else if (!mystrcasecmp(s, "ENCODING")) {
	    p = strtok(NULL, " \t\n\r");
	    encoding = atoi(p);
	} else if (!mystrcasecmp(s, "DWIDTH")) {
	    p = strtok(NULL, " \t\n\r");
	    width = atoi(p);
	} else if (!mystrcasecmp(s, "BBX")) {
	    p = strtok(NULL, " \t\n\r");
	    bbw = atoi(p);
	    p = strtok(NULL, " \t\n\r");
	    bbh = atoi(p);
	    p = strtok(NULL, " \t\n\r");
	    bbx = atoi(p);
	    p = strtok(NULL, " \t\n\r");
	    bby = atoi(p);
	} else if (!mystrcasecmp(s, "BITMAP")) {
	    fprintf(out, "// %3d $%02x '%s'\n", encoding, encoding, charname);
	    fprintf(out, "//\twidth %d, bbx %d, bby %d, bbw %d, bbh %d\n",
		width, bbx, bby, bbw, bbh);

	    if (n == chars) {
		fprintf(stderr, "Too many bitmaps for characters\n");
		exit(-1);
	    }
	    if (width == INT_MIN) {
		fprintf(stderr, "character width not specified\n");
		exit(-1);
	    }
	    //
	    //	Adjust width based on bounding box
	    //
	    if (bbx < 0) {
		width -= bbx;
		bbx = 0;
	    }
	    if (bbx + bbw > width) {
		width = bbx + bbw;
	    }
	    if (Outline) {		// Reserve space for outline border
		++width;
	    }
	    width_table[n] = width;
	    encoding_table[n] = encoding;
	    ++n;
	    if (Outline) {		// Leave first row empty
		scanline = 1;
	    } else {
		scanline = 0;
	    }
	    memset(bitmap, 0,
		((fontboundingbox_width + 7) / 8) * fontboundingbox_height);
	} else if (!mystrcasecmp(s, "ENDCHAR")) {
	    if (bbx) {
		RotateBitmap(bitmap, bbx, fontboundingbox_width,
		    fontboundingbox_height);
	    }
            yshift = fontboundingbox_height - bbh + fontboundingbox_y - bby;
            if(yshift > 0) {
	      MoveDownBitmap(bitmap, yshift, fontboundingbox_width,
			     fontboundingbox_height);
	    }
	    if (Outline) {
		RotateBitmap(bitmap, 1, fontboundingbox_width,
		    fontboundingbox_height);
		OutlineCharacter(bitmap, fontboundingbox_width,
		    fontboundingbox_height);
	    }
	    DumpCharacter(out, bitmap, fontboundingbox_width,
		fontboundingbox_height);
	    scanline = -1;
	    width = INT_MIN;
	} else {
	    if (scanline >= 0) {
		p = s;
		j = 0;
		while (*p) {
		    i = Hex2Int(p);
		    ++p;
		    if (*p) {
			i = Hex2Int(p) | i * 16;
		    } else {
			bitmap[j + scanline * ((fontboundingbox_width +
				    7) / 8)] = i;
			break;
		    }
		 
		    bitmap[j + scanline * ((fontboundingbox_width + 7) / 8)] =
			i;
		    ++j;
		    ++p;
		}
		++scanline;
	    }
	}
    }

    fprintf(out, "};\n\n\n");

    // Output width table for proportional font.
    WidthTable(out, name, width_table, chars);
    // FIXME: Output offset table for proportional font.
    // OffsetTable(out, name, offset_table, chars);
    // Output encoding table for utf-8 support
    EncodingTable(out, name, encoding_table, chars);

    Footer(out, name, font_ascent, font_descent, fontboundingbox_width, fontboundingbox_height, chars);
}

//////////////////////////////////////////////////////////////////////////////

///
///	Print version
///
static void PrintVersion(void)
{
    printf("bdf2c Version %s, (c) 2009, 2010 by Lutz Sammer\n"
	"\tLicense AGPLv3: GNU Affero General Public License version 3\n"
	"\tModified by Malcolm McLean, 2013\n",
	VERSION);
}

static void Usage(void)
{
  PrintVersion();
  printf("\nConvert a bdf file to a C-parseable dump\n");
  printf("Usage : <font.bdf> <name>\n");
  exit(EXIT_FAILURE);
}

		
int bdf2cmain(int argc, char **argv)
{
  FILE *fpin;
  if(argc != 3)
    Usage();
  fpin = fopen(argv[1], "r");
  if(!fpin)
  {
    fprintf(stderr, "Can't open %s\n", argv[1]);
    exit(EXIT_FAILURE);
  }
  printf("#include \"font.h\"\n\n");
  ReadBdf(fpin, stdout, argv[2]);
  fclose(fpin);
  return 0;
}
