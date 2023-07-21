#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "xmlparser.h"
#include "asciitostring.h"
#include "loadcursor.h"
#include "loadimage.h"
#include "wavfile.h"
#include "resize.h"
#include "bdf2c.h"
#include "ttf2c.h"

char *getextension(char *fname);

int dumpimage(FILE *fp, char *name, unsigned char *rgba, int width, int height)
{
  int i;

  fprintf(fp, "int %s_width = %d;\n", name, width);
  fprintf(fp, "int %s_height = %d;\n", name, height);
  fprintf(fp, "unsigned char %s_rgba[%d] = \n", name, width * height *4);
  fprintf(fp, "{\n");
  for(i=0;i<width * height * 4;i++)
  {
    fprintf(fp, "0x%02x, ", rgba[i]);
    if( (i % 10) == 9)
      fprintf(fp, "\n");
  }
  if(i % 10)
    fprintf(fp, "\n");
  fprintf(fp, "};\n");
  fprintf(fp, "\n\n");

 
  return 0;
}

/*
  process the image after parsing completed
    wwidth, wwheight - wanted width and height, -1 if use the file
 */
int processimage(FILE *fp, char *fname, char *name, int wwidth, int wheight)
{
  unsigned char *rgba;
  unsigned char *resizedrgba;
  int width, height;
  int err;
  char *ext = 0;
  int i;

  ext = getextension(fname);
  if (ext)
  {
	  for (i = 0; ext[i]; i++)
		  ext[i] = tolower(ext[i]);
  }
  if (ext && !strcmp(ext, ".svg") && wwidth > 0 && wheight > 0)
  {
	  rgba = loadassvgwithsize(fname, wwidth, wheight);
	  width = wwidth;
	  height = wheight;
  }
  else
      rgba = loadrgba(fname, &width, &height, &err);
  if(!rgba)
  {
    fprintf(stderr, "Can't load image %s\n", fname);
    exit(EXIT_FAILURE);
  }
  if(wwidth == -1)
    wwidth = width;
  if(wheight == -1)
    wheight = height;
  resizedrgba = malloc(wwidth*wheight*4);
  if(!resizedrgba)
  {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
  resizeimage(resizedrgba, wwidth, wheight, rgba, width, height);
  dumpimage(fp, name, resizedrgba, wwidth, wheight);
  free(rgba);
  free(resizedrgba);
  free(ext);
  return 0;
}

int dumpcursor(FILE *fp, BBX_CURSOR *cursor, const char *name)
{
	int i;

	fprintf(fp, "unsigned char %s_rgba[%d] = \n", name, cursor->width * cursor->height * 4);
	fprintf(fp, "{\n");
	for (i = 0; i<cursor->width * cursor->height * 4; i++)
	{
		fprintf(fp, "0x%02x, ", cursor->rgba[i]);
		if ((i % 10) == 9)
			fprintf(fp, "\n");
	}
	if (i % 10)
		fprintf(fp, "\n");
	fprintf(fp, "};\n");
	fprintf(fp, "\n");
	fprintf(fp, "struct bbx_cursor %s = \n", name);
	fprintf(fp, "{\n");
	fprintf(fp, "  %d,\n", cursor->width);
	fprintf(fp, "  %d,\n", cursor->height);
	fprintf(fp, "  %s_rgba,\n", name);
	fprintf(fp, "  %d,\n", cursor->hotx);
	fprintf(fp, "  %d\n", cursor->hoty);
	fprintf(fp, "};\n");
	fprintf(fp, "\n\n");

	return 0;
}

int putcursordefinition(FILE *fp)
{
	fprintf(fp, "/* cursor structure */\n");
	fprintf(fp, "struct bbx_cursor {\n");
	fprintf(fp, "  int width;         /* cursor width in pixels */\n");
	fprintf(fp, "  int height;        /* cursor height in pixels */\n");
	fprintf(fp, "  unsigned char *rgba;     /* cursor rgba data */\n");
	fprintf(fp, "  int hotx;       /* hotspot x co-ordinate */\n");
	fprintf(fp, "  int hoty;       /* hotspot y co-ordinate */\n");
	fprintf(fp, "};\n\n");

	return 0;
}

int dumpaudio(FILE *fp, const short *pcm, long samplerate, int Nchannels, long Nsamples, char *name)
{
    size_t count;
    long i;
    
    count = Nsamples * Nchannels;
    
    fprintf(fp, "long %s_samplerate = %ld;\n", name, samplerate);
    fprintf(fp, "int %s_Nchannels = %d;\n", name, Nchannels);
    fprintf(fp, "long %s_Nsamples = %ld;\n", name, (long) Nsamples);
    
    fprintf(fp, "short %s[%ld] = {\n", name, (long) count);
    for (i=0; i <count; i++)
    {
      fprintf(fp, "%d, ", pcm[i]);
      if( (i % 10) == 9)
        fprintf(fp, "\n");
    }
    if (i % 10)
      fprintf(fp, "\n");

    fprintf(fp, "};\n\n");
    
    return 0;
}

int dumpbinary(FILE *fp, const char *fname, const char *name)
{
  FILE *fpb;
  size_t flen = 0;
  size_t i = 0;
  int ch;

  fpb = fopen(fname, "rb");
  if(!fpb)
    return -1;
  while(fgetc(fpb) != EOF)
  {
    flen++;
    if(flen > INT_MAX)
    {
      fprintf(stderr,"Warning, excessively long binary %s\n", fname);
      fprintf(stderr, "Unlikely to compile\n");
    }
  }
  if(flen == 0)
  {
    fprintf(stderr, "Can't create binary image of empty file %s\n", fname);
    fclose(fpb);
    return -1;
  }
  fseek(fpb, 0, SEEK_SET);
  fprintf(fp, "unsigned char %s[%ld] = {\n", name, (long) flen);
  while( (ch = fgetc(fpb)) != -1)
  {
    fprintf(fp, "0x%02x, ", ch);
    if( (i % 10) == 9)
      fprintf(fp, "\n");
    i++;
  }
  if(i % 10)
    fprintf(fp, "\n");

  fprintf(fp, "};\n\n");

  fclose(fpb);

  return 0;
}

void makelower(char *str)
{
  int i;

  if(!str)
    return;
  for(i=0;str[i];i++)
    str[i] = tolower(str[i]);
}

char *getbasename(char *fname)
{
  char *base;
  char *ext;
  char *answer;

  base = strrchr(fname, '/');
  if(!base)
    base = strrchr(fname, '\\');
  if(!base)
    base = fname;
  else
    base = base + 1;
  ext = strrchr(base, '.');
  if(!ext || ext == base)
      ext = base + strlen(base);
  answer = malloc(ext-base + 1);
  memcpy(answer, base, ext - base);
  answer[ext-base] = 0;
  
  return answer;
}

char *getextension(char *fname)
{
  char *ext;
  char *answer;

  ext = strrchr(fname, '.');
  if(!ext || ext == fname || strchr(ext, '/') || strchr(ext, '\\'))
  {
    answer = malloc(1);
    if(answer)
      answer[0] = 0;
  }
  else
  {
    answer = malloc(strlen(ext) + 1);
    if(answer)
      strcpy(answer, ext);
  }
  return answer;
}

static char *mystrdup(const char *str)
{
  char *answer;

  answer = malloc(strlen(str) + 1);
  if(answer)
    strcpy(answer, str);

  return answer;
}


static char *trim(const char *str)
{
  char *answer;
  int i;

  answer = malloc(strlen(str) + 1);
  if(!answer)
    return 0;
  for(i=0;isspace((unsigned char) str[i]);i++)
    ;
  strcpy(answer, str + i);
  for(i=strlen(answer)-1; i >= 0;i--)
    if(isspace((unsigned char) answer[i]))
       answer[i] = 0;
    else
      break;
  return answer;
}

static int quotedstring(const char *str)
{
  char *trimmed;
  int answer = 0;

  trimmed = trim(str);
  if(!trimmed)
  {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
  if(strlen(trimmed) >= 2 && 
     trimmed[0] == '\"' && 
     trimmed[strlen(trimmed)-1] == '\"')
    answer= 1;
  else if(strlen(trimmed) >= 3 &&
            trimmed[0] == 'L' &&
            trimmed[1] == '\"' &&
            trimmed[strlen(trimmed)-1] == '\"')
    answer = 1;
    
     
  free(trimmed);
  return answer;
}

static char *addquotes(const char *str)
{
  if(quotedstring(str))
  {
    if(!validcstring(str))
      fprintf(stderr, "Warning, quoted string appears to be invalid C syntax\n");
    return trim(str);
  }
  else
    return texttostring(str);

}

int putfontdefinition(FILE *fp)
{
	fprintf(fp, "/* bitmap font structure */\n");
	fprintf(fp, "struct bitmap_font {\n");
	fprintf(fp, "  unsigned char width;         /* max. character width */\n");
	fprintf(fp, "  unsigned char height;        /* character height */\n");
	fprintf(fp, "  int ascent;                  /* font ascent */\n");
	fprintf(fp, "  int descent;                 /* font descent */\n");
	fprintf(fp, "  unsigned short Nchars;       /* number of characters in font */\n");
	fprintf(fp, "  unsigned char *widths;       /* width of each character */\n");
	fprintf(fp, "  unsigned short *index;       /* encoding to character index */\n");
	fprintf(fp, "  unsigned char *bitmap;       /* bitmap of all characters */\n");
	fprintf(fp, "};\n\n");

	return 0;
}



int processimagetag(FILE *fp, const char *fname, const char *name, const char *widthstr, const char *heightstr)
{
  char *path;
  char *imagename;
  int width, height;
  char *end;

  if(!fname)
  {
    fprintf(stderr, "No image source file\n");
    return -1;
  }
  path = mystrdup(fname);
  if(name)
    imagename = mystrdup(name);
  else
    imagename = getbasename(path);
  if(widthstr)
  {
    width = strtol(widthstr, &end, 10);
    if(*end || width <= 0)
    {
      fprintf(stderr, "Bad width ***%s*** Using default\n", widthstr);
      width = -1;
    }
  }
  else
    width = -1;
  if(heightstr)
  {
    height = strtol(heightstr, &end, 10);
    if(*end || height <= 0)
    {
      fprintf(stderr, "Bad height ***%s*** Using default\n", heightstr);
      height = -1;
    }
  }
  else
    height = -1;

  processimage(fp, path, imagename, width, height);
  free(path);
  free(imagename);
  return 0;
}

int processfonttag(FILE *fp, const char *fname, const char *name, const char *pointsstr)
{
  int points;
  char *end;
  char *path;
  char *fontname;
  char *ext;
  int answer = 0;

  if(!fname)
  {
    fprintf(stderr, "No font source file\n");
    return -1;
  }
  path = mystrdup(fname);
  if(name)
    fontname = mystrdup(name);
  else
    fontname = getbasename(path);
  
  if(pointsstr)
  {
    points = strtol(pointsstr, &end, 10);
    if(*end || points <= 0)
    {
      fprintf(stderr, "Bad points ***%s*** Using default\n", pointsstr);
      points = 12;
    } 
  }
  ext = getextension(path);
  makelower(ext);
  if(!strcmp(ext, ".ttf"))
  {
    dumpttf(path, fontname, points, fp); 
  }
  else if(!strcmp(ext, ".bdf"))
  {
    FILE *fpbdf;
    fpbdf = fopen(path, "r");
    if(!fpbdf)
    {
      fprintf(stderr, "Can't open %s\n", path);
      answer = -1;
    }
    else   
    {
      ReadBdf(fpbdf, fp, fontname);
      fclose(fpbdf);
    }
  }
  else
  {
    fprintf(stderr, "Unsupported font file type %s\n", ext);
    return -1;
  }
  free(path);
  free(fontname);

  return answer;
}

int processstringtag(FILE *fp, const char *fname, const char *name, const char *str)
{
  char *path = 0;
  char *stringname = 0;
  char *string = 0;
  FILE *fpstr;
  int answer = 0;
  char *buff;

  if(fname)
    path = mystrdup(fname);
  if(name)
    stringname = mystrdup(name);
  else if(path)
    stringname = getbasename(path);
  else
  {
    fprintf(stderr, "No string name specified\n");
    answer = -1;
   }
    
  if(path)
  {
    fpstr = fopen(path, "r");
    if(!fpstr)
    {
      fprintf(stderr, "Can't open %s\n", fname);
      answer = -1;
    }
    else
    {
       buff = fslurp(fpstr);
       if(buff)
         string = texttostring(buff);
       free(buff);
       fclose(fpstr);
    }
  }
  else if(str)
  {
    string = addquotes(str);
  }
  if(!string)
  {
    fprintf(stderr, "Out of memory with string\n");
    answer = -1;
  }
  else if(!stringname)
  {
    fprintf(stderr, "Problem with string name\n");
    answer = -1; 
  }
  else if(stringname && string)
  {
    fprintf(fp, "char *%s = ", stringname);
    fputs(string, fp);
    fprintf(fp, ";\n"); 
  }
  
  free(path);
  free(string);
  free(stringname);

  return answer;
}

int processbinarytag(FILE *fp, const char *fname, const char *name)
{
  char *binaryname;
  int answer = 0;

  if(!fname)
  {
    fprintf(stderr, "Error, binary without src attribute\n");
    return -1;
  }
  
  if(name)
    binaryname = mystrdup(name);
  else
    binaryname = getbasename( (char*)fname );
  if(dumpbinary(fp, fname, binaryname) < 0)
  {
    fprintf(stderr, "Error processing %s\n", fname);
    answer = -1;
  } 
  free(binaryname);

  return answer;
}

int processcursortag(FILE *fp, const char *fname, const char *name)
{
	char *cursorname;
	int answer = 0;
	int err;
	BBX_CURSOR *cursor;

	if (!fname)
	{
		fprintf(stderr, "Error, cursor without src attribute\n");
		return -1;
	}

	if (name)
		cursorname = mystrdup(name);
	else
		cursorname = getbasename((char*)fname);
	cursor = loadcursor(fname, &err);
	if (!cursor)
	{
		fprintf(stderr, "can't load cursor %s\n", fname);
		return -1;
	}
	if (dumpcursor(fp, cursor, cursorname) < 0)
	{
		fprintf(stderr, "Error processing %s\n", fname);
		answer = -1;
	}
	free(cursorname);
	free(cursor->rgba);
	free(cursor);

	return answer;
}

int processaudiotag(FILE *fp, const char *fname, const char *name)
{
    char *audioname;
    int answer = 0;
    int err;
    long samplerate;
    int Nchannels;
    long Nsamples;
    short *pcm;
    
    if (!fname)
    {
        fprintf(stderr, "Error, audio without src attribute\n");
        return -1;
    }

    if (name)
        audioname = mystrdup(name);
    else
        audioname = getbasename((char*)fname);
    pcm = loadwav(fname, &samplerate, &Nchannels, &Nsamples);
    if (!pcm)
    {
        fprintf(stderr, "can't load audio %s\n", fname);
        return -1;
    }
    if (dumpaudio(fp, pcm, samplerate, Nchannels, Nsamples, audioname) < 0)
    {
        fprintf(stderr, "Error processing %s\n", fname);
        answer = -1;
    }
    free(audioname);
    free(pcm);

    return answer;
}

void usage(void)
{
  printf("The Baby X resource compiler v1.0\n");
  printf("by Malcolm Mclean\n");
  printf("\n");
  printf("Usage: babyxrc <script.xml>\n");
  printf("\n");
  printf("Example script file:\n");
  printf("<BabyXRC>\n");
  printf("<image src = \"smiley.png\", name = \"fred\", width = \"10\", height = \"10\"> </image>\n");
  printf("<image src = \"lena.jpg\"> </image>\n");
  printf("<font src = \"arial.ttf\", name = \"arial12\", points=\"12\"> </font>\n");
  printf("<string src = \"external.txt\"> </string>\n");
  printf("<string name = \"internal\"> \"A C string\\n\"> </string>\n");
  printf("<string name = \"embedded\"> Embedded string </string>\n");
  printf("<binary src = \"dump.bin\", name = \"dump_bin\"> </binary>\n"); 
  printf("</BabyXRC>\n");
  printf("\n");
  printf("<image>\n");
  printf("loads gif, png, jpeg, tiff or bmp format images. Will resize if necessary\n");
    printf("using averaging for shrinking and bilinear interpolation for expanding.\n");
  printf("width and height defaults to image size.\n");
  printf("Output is always as a C-parseable 32 bit rgba array.\n");
  printf("<font>\n");
  printf("Handles ttf or bdf font. Truetype must always have points set.\n");
  printf("Output glyphs in 8-bit grayscale.\n");
  printf("<string>\n");
  printf("Ascii strings, external or embedded. Quoted strings assumed to be C \n");
  printf("string literals.\n");
  printf("<binary>\n");
  printf("Dump raw binary data.\n");   
  exit(EXIT_FAILURE);
}


int main(int argc, char **argv)
{
  XMLDOC *doc;
  int err;
  XMLNODE **scripts;
  XMLNODE *node;
  int Nscripts;
  int Nchildren;
  int i, ii;
  const char *path;
  const char *name;
  const char *str;
  const char *widthstr;
  const char *heightstr;
  const char *pointsstr;
  

  if(argc != 2)
    usage();

  doc = loadxmldoc(argv[1], &err); 
  if(!doc)
  {
    fprintf(stderr, "Can't read resource script file\n");
    exit(EXIT_FAILURE);
  } 
  scripts = xml_getdescendants(xml_getroot(doc), "BabyXRC", &Nscripts);
  for(i=0;i<Nscripts;i++)
  {
    if(i == 0 && xml_Nchildrenwithtag(scripts[i], "font") > 0)
      putfontdefinition(stdout);
	if (i == 0 && xml_Nchildrenwithtag(scripts[i], "cursor") > 0)
		putcursordefinition(stdout);
    Nchildren = xml_Nchildrenwithtag(scripts[i], "image");
    for(ii=0;ii<Nchildren;ii++)
    {
      node = xml_getchild(scripts[i], "image", ii);
      path = xml_getattribute(node, "src");
      name = xml_getattribute(node, "name");
      widthstr = xml_getattribute(node, "width");
      heightstr = xml_getattribute(node, "height");
      processimagetag(stdout, path, name, widthstr, heightstr); 
    }
    Nchildren = xml_Nchildrenwithtag(scripts[i], "font");
    for(ii=0;ii<Nchildren;ii++)
    {
      node = xml_getchild(scripts[i], "font", ii);
      path = xml_getattribute(node, "src");
      name = xml_getattribute(node, "name");
      pointsstr = xml_getattribute(node, "points");
      processfonttag(stdout, path, name, pointsstr);
    }
    Nchildren = xml_Nchildrenwithtag(scripts[i], "string");
    for(ii=0;ii<Nchildren;ii++)
    {
        node = xml_getchild(scripts[i], "string", ii);
        path = xml_getattribute(node, "src");
        name = xml_getattribute(node, "name");
        str = xml_getdata(node);
        processstringtag(stdout, path, name, str);
    }
    Nchildren = xml_Nchildrenwithtag(scripts[i], "binary");
    for(ii=0;ii<Nchildren;ii++)
    {
      node = xml_getchild(scripts[i], "binary", ii);
      path = xml_getattribute(node, "src");
      name = xml_getattribute(node, "name");
      processbinarytag(stdout, path, name);
    }
	Nchildren = xml_Nchildrenwithtag(scripts[i], "cursor");
	for (ii = 0; ii<Nchildren; ii++)
	{
		node = xml_getchild(scripts[i], "cursor", ii);
		path = xml_getattribute(node, "src");
		name = xml_getattribute(node, "name");
		processcursortag(stdout, path, name);
	}
    Nchildren = xml_Nchildrenwithtag(scripts[i], "audio");
    for (ii = 0; ii<Nchildren; ii++)
    {
        node = xml_getchild(scripts[i], "audio", ii);
        path = xml_getattribute(node, "src");
        name = xml_getattribute(node, "name");
        processaudiotag(stdout, path, name);
    }
  }  
  killxmldoc(doc);


  return 0;
}
