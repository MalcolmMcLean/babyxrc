#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freetype/freetype.h"

static TT_Error initrastermap(TT_Raster_Map *map, int width, int height, int gray_render)
{
  map->rows = height;
  map->width = width;

  if (gray_render) 
  {
    map->cols = map->width;
    map->size = map->rows * map->width;
  } 
  else 
  {
    map->cols = (map->width + 7) / 8;	/* convert to # of bytes */
    map->size = map->rows * map->cols;	/* number of bytes in buffer */
  }

  map->flow = TT_Flow_Down;
  map->bitmap = (void *) malloc(map->size);
  if (map->bitmap == NULL)
     return 1;

  return 0;
}

static void killrastermap(TT_Raster_Map *map)
{
  if(map)
    free(map->bitmap);
}

static int getascenderanddescender(TT_Instance instance, TT_Glyph glyph, int Nglyphs, int *ascent, int *descent, int *width)
{
  int i;
  TT_Pos ascender = 0;
  TT_Pos descender = 0;
  TT_Pos maxwidth = 0;
  TT_Glyph_Metrics glyphMetrics;
  TT_UShort glyphIndex;
  TT_Error err;

  for(i=0;i<Nglyphs;i++)
  {
    glyphIndex = (TT_UShort) i;
    err = TT_Load_Glyph(instance, glyph, glyphIndex, TTLOAD_SCALE_GLYPH | TTLOAD_HINT_GLYPH);
    if(err == 0)
    {
      TT_Get_Glyph_Metrics(glyph, &glyphMetrics);
      if(ascender < glyphMetrics.bearingY)
        ascender = glyphMetrics.bearingY;
      if(descender > glyphMetrics.bbox.yMin)
        descender = glyphMetrics.bbox.yMin;
      if(maxwidth < glyphMetrics.bbox.xMax - glyphMetrics.bbox.xMin + glyphMetrics.bearingX)
         maxwidth =  glyphMetrics.bbox.xMax - glyphMetrics.bbox.xMin + glyphMetrics.bearingX;
    }
  }
  *ascent = (ascender + 31)/64;
  *descent = (-descender + 31)/64;
  *width = (maxwidth + 63)/64;

  return 0;
}

static void getcodes(TT_CharMap charmap, int Nglyphs, int *ret)
{
  int i, ii;

  for(i=0;i<Nglyphs;i++)
  {
    for(ii=0;ii<0xFFFF;ii++)
      if(TT_Char_Index(charmap, ii) == i)
        break;
    if(ii < 0xFFFF)
      ret[i] = ii;
    else
      ret[i] = -1;
  }
}

static int getbestcharmap(TT_Face face)
{
  int N;
  TT_UShort platformID, encodingID;
  int i;
  
  N = TT_Get_CharMap_Count(face);
  /* try to get a Unicode encoding 0 , 0 = Apple Unicode. 3 1 = Windows unicode */
  for(i=0;i<N;i++)
  {
    TT_Get_CharMap_ID(face, i, &platformID, &encodingID);
    if( (platformID == 3 && encodingID == 1) || 
	(platformID == 0 && encodingID == 0) )
      return i;
  }
  /* Ok, maybe its  Windows symbol font */ 
  for(i=0;i<N;i++)
  {
    TT_Get_CharMap_ID(face, i, &platformID, &encodingID);
    if(platformID == 3 && encodingID == 0)
      return i;
  } 

  /* just return the default Apple Roman coding */
  return 0;

}

/*
  sort codes is ascending order (-1 = no code, at start ) 
 */
static int compints(const void *e1, const void *e2)
{
  return *(const int *)e1 - *(const int *) e2;
}

int dumpttf(char *fname, int header, char *name, int points, FILE *fp)
{
  int major, minor;
  TT_Engine engine;
  TT_Byte palette[5] = {0x00, 0x40, 0x80, 0xC0, 0xFF};
  TT_Face face;
  TT_Face_Properties faceProperties;
  TT_Instance instance;
  TT_Instance_Metrics instanceMetrics;
  TT_Glyph glyph;
  TT_Glyph_Metrics glyphMetrics;
  TT_CharMap charmap;
  TT_Raster_Map map;
  TT_UShort glyphIndex;
  TT_Error err;
  int i;
  int x, y;
  int ascent, descent, maxwidth;
  int *codes;
  int *widths;
  int Nglyphs;
    
  if (header)
  {
      fprintf(fp, "exrern struct bitmap_font %s_font;\n", name);
      return 0;
  }

  TT_FreeType_Version(&major, &minor);
  err = TT_Init_FreeType(&engine);
  if(err)
  {
    fprintf(stderr, "Can't initialise free type system\n");
    return -1;
  }
 
  TT_Set_Raster_Gray_Palette(engine, palette);
  err = TT_Open_Face(engine, fname, &face);
  if(err)
  {
    fprintf(stderr, "Can't open ttf file %s\n", fname);
    return -1;
  }
  TT_Get_Face_Properties(face, &faceProperties);
  TT_New_Instance(face, &instance);
  TT_Set_Instance_CharSize(instance, points*64);
  TT_Get_Instance_Metrics(instance, &instanceMetrics);
  TT_Get_CharMap(face, getbestcharmap(face), &charmap);
  codes = malloc(faceProperties.num_Glyphs * sizeof(int));
  getcodes(charmap, faceProperties.num_Glyphs, codes);
  qsort(codes, faceProperties.num_Glyphs, sizeof(int), compints);
  for(i=0;i<faceProperties.num_Glyphs;i++)
    if(codes[i] != -1)
      break;
  Nglyphs = faceProperties.num_Glyphs - i;
  memmove(codes, codes+i, Nglyphs * sizeof(int));
  widths = malloc(Nglyphs * sizeof(int));
  TT_New_Glyph(face, &glyph);

  getascenderanddescender(instance, glyph, faceProperties.num_Glyphs, &ascent, &descent, &maxwidth);
  initrastermap(&map, (maxwidth+3)/4*4, ascent + descent, 1);

  fprintf(fp, "\nstatic unsigned char %s_bitmap[%d] =\n", name,  Nglyphs* maxwidth * (ascent + descent) );
  fprintf(fp, "{\n");
  for(i=0;i<Nglyphs;i++)
  {
    memset(map.bitmap, 0, map.size);
    glyphIndex = TT_Char_Index(charmap, codes[i]);

    TT_Load_Glyph(instance, glyph, glyphIndex, TTLOAD_SCALE_GLYPH | TTLOAD_HINT_GLYPH);
    TT_Get_Glyph_Metrics(glyph, &glyphMetrics);

    widths[i] = (glyphMetrics.advance + 31)/64;
    if(widths[i] < 0)
      widths[i] = 0;
    if(widths[i] > maxwidth)
      widths[i] = maxwidth;

    // TT_Get_Glyph_Pixmap(glyph, &map, glyphMetrics.bearingX, descent * 64);
    TT_Get_Glyph_Pixmap(glyph, &map, 0, descent * 64);
    fprintf(fp, "/* glyph %d */\n", codes[i]);
    for(y=0;y<ascent+descent;y++)
    {
      for(x=0;x<maxwidth;x++)
        fprintf(fp, "0x%02x, ", ((unsigned char *)map.bitmap)[y * map.width + x]);
      fprintf(fp, "\n");
    }
    fprintf(fp, "\n"); 
  }
  fprintf(fp, "};\n");

  fprintf(fp, "static unsigned char %s_widths[%d] =\n", name,  Nglyphs);
  fprintf(fp, "{\n");
  for(i=0;i<Nglyphs;i++)
    fprintf(fp, "%d,\n", widths[i]);
  fprintf(fp, "};\n\n");

  fprintf(fp, "static unsigned short %s_index[%d] =\n", name,  Nglyphs);
  fprintf(fp, "{\n");
  for(i=0;i<Nglyphs;i++)
    fprintf(fp, "%d,\n", codes[i]);
  fprintf(fp, "};\n\n");

  fprintf(fp, "struct bitmap_font %s_font = \n", name);
  fprintf(fp, "{\n");
  fprintf(fp, "%d, /* max width */\n", maxwidth);
  fprintf(fp, "%d, /* height */\n", ascent + descent);
  fprintf(fp, "%d, /* ascent */\n", ascent);
  fprintf(fp, "%d, /* descent */\n", descent);
  fprintf(fp, "%d, /* Nchars */\n", Nglyphs);
  fprintf(fp, "%s_widths, /* widths */\n", name);
  fprintf(fp, "%s_index, /* index */\n", name);
  fprintf(fp, "%s_bitmap /* bitmap */\n", name);
  fprintf(fp, "};\n");
  fprintf(fp, "\n"); 

  free(codes);
  free(widths);
  killrastermap(&map); 
  TT_Done_Glyph(glyph);
  TT_Close_Face(face);
  TT_Done_FreeType(engine);

  return 0;
}


static void usage(void)
{
  printf("ttf2c - converts a ttf file to an ANSI C bitmapped font\n");
  printf("Usage: <font.ttf> <name> <points>\n");
  printf("Creates C file on stdout\n");
  exit(EXIT_FAILURE);
}

int ttf2cmain(int argc, char **argv)
{
  if(argc != 4)
    usage();
  printf("#include \"font.h\"\n");
  dumpttf(argv[1], 0, argv[2], atoi(argv[3]), stdout);

  return 0;
}
