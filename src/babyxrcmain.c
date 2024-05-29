#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "options.h"
#include "xmlparser2.h"
#include "asciitostring.h"
#include "loadasutf8.h"
#include "loadcursor.h"
#include "loadimage.h"
#include "wavfile.h"
#include "aifffile.h"
#include "loadmp3.h"
#include "csv.h"
#include "dumpcsv.h"
#include "dataframes.h"
#include "resize.h"
#include "bdf2c.h"
#include "ttf2c.h"
#include "bbx_utf8.h"
#include "basic.h"
#include "samplerate/samplerate.h"

typedef struct
{
    char **names;
    char **scripts;
    int Nscripts;
} MINIBASIC;

char *getextension(char *fname);
char *xformdata(void *minibasic, const char *scriptname, const char *str);

int dumpimage(FILE *fp, int header, char *name, unsigned char *rgba, int width, int height)
{
  int i;
    
  if (header)
  {
      fprintf(fp, "extern int %s_width;\n", name);
      fprintf(fp, "extern int %s_height;\n", name);
      fprintf(fp, "extern unsigned char %s_rgba[%d];\n", name, width * height *4);
      return 0;
  }

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
int processimage(FILE *fp, int header, char *fname, char *name, int wwidth, int wheight)
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
  dumpimage(fp, header, name, resizedrgba, wwidth, wheight);
  free(rgba);
  free(resizedrgba);
  free(ext);
  return 0;
}

int dumpcursor(FILE *fp, int header, BBX_CURSOR *cursor, const char *name)
{
	int i;
    
    if (header)
    {
        fprintf(fp, "extern struct bbx_cursor %s\n", name);
        return 0;
    }

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

int dumpaudio(FILE *fp, int header, const short *pcm, long samplerate, int Nchannels, long Nsamples, char *name)
{
    size_t count;
    long i;
    
    count = Nsamples * Nchannels;
    
    if (header)
    {
        fprintf(fp, "exern long %s_samplerate;\n", name);
        fprintf(fp, "extern int %s_Nchannels;\n", name);
        fprintf(fp, "extern long %s_Nsamples;\n", name);
        fprintf(fp, "extern short %s[%ld];\n", name, (long) count);
        return 0;
    }
    
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

int dumpbinary(FILE *fp, int header, const char *fname, const char *name)
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
  if (header)
  {
      fprintf(fp, "extern unsigned char %s[%ld];\n", name, (long) flen);
  }
  else
  {
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
  }

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

void removetrailingnewline(char *str)
{
  int i;

  if(!str)
    return;
  for(i=0;str[i];i++)
      ;
   if (i)
   {
       i--;
       if (str[i] == '\n')
            str[i] = 0;
   }
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

static char *makecomment(const char *str)
{
    char *trimmed = 0;
    char *ptr = 0;
    char *answer = 0;
    
    trimmed = trim(str);
    if (!trimmed)
        goto error_exit;
    if (strlen(trimmed) >= 4)
    {
        if (trimmed[0] == '/' && trimmed[1] == '*')
            memmove(trimmed, trimmed + 2, strlen(trimmed) - 2 + 1);
        if (trimmed[strlen(trimmed) -2] == '*' && trimmed[strlen(trimmed)-1] == '/')
            trimmed[strlen(trimmed) -2] = 0;
    }
    ptr = trimmed;
    while ((ptr = strstr(ptr, "*/")))
        *ptr = '^';
    answer = malloc(strlen(trimmed) + 6 + 1);
    if (!answer)
        goto error_exit;
    
    strcpy(answer, "/*");
    if (strchr(trimmed, '\n'))
        strcat(answer, "\n");
    strcat(answer, trimmed);
    if (strchr(trimmed, '\n'))
        strcat(answer, "\n");
    strcat(answer, "*/");
    
    free(trimmed);
    
    return answer;
error_exit:
    free(trimmed);
    free(answer);
    return 0;
    
}

int parseboolean(const char *boolattribute, int *error)
{
    int answer = 0;
    
    if (error)
        *error = 0;
    if (!strcmp(boolattribute, "true"))
        return 1;
    if (!strcmp(boolattribute, "false"))
        return 0;
    if (!strcmp(boolattribute, "1"))
        return 1;
    if (!strcmp(boolattribute, "0"))
        return 0;
    
    if (error)
        *error = -1;
    return 0;
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



int processimagetag(FILE *fp, int header, const char *fname, const char *name, const char *widthstr, const char *heightstr)
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
    width = (int) strtol(widthstr, &end, 10);
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
    height = (int) strtol(heightstr, &end, 10);
    if(*end || height <= 0)
    {
      fprintf(stderr, "Bad height ***%s*** Using default\n", heightstr);
      height = -1;
    }
  }
  else
    height = -1;

  processimage(fp, header, path, imagename, width, height);
  free(path);
  free(imagename);
  return 0;
}

int processfonttag(FILE *fp, int header,  const char *fname, const char *name, const char *pointsstr)
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
    points = (int) strtol(pointsstr, &end, 10);
    if(*end || points <= 0)
    {
      fprintf(stderr, "Bad points ***%s*** Using default\n", pointsstr);
      points = 12;
    } 
  }
  else
  {
      points = 12;
  }
  ext = getextension(path);
  makelower(ext);
  if(!strcmp(ext, ".ttf"))
  {
    dumpttf(path, header, fontname, points, fp);
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
      ReadBdf(fpbdf, fp, header, fontname);
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
  free(ext);

  return answer;
}


int processstringtag(FILE *fp, int header, const char *fname, const char *name, const char *xform, const char *str, void *xformcontext)
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
    
  if (header)
  {
      fprintf(fp, "extern char %s[];\n", stringname);
      free(path);
      free(stringname);
      return 0;
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
       {
           if (xform)
           {
               string = xformdata(xformcontext, xform, buff);
               if (string)
               {
                   free(buff);
                   buff = string;
               }
               else
               {
                   fprintf(stderr, "Out of memory with string\n");
                   answer = -1;
               }
           }
           string = texttostring(buff);
       }
       free(buff);
       fclose(fpstr);
    }
  }
  else if(str)
  {
    if (xform)
        str = xformdata(xformcontext, xform, str);
    if (str)
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
    fprintf(fp, "char %s[] = ", stringname);
    fputs(string, fp);
    fprintf(fp, ";\n"); 
  }
  
  free(path);
  free(string);
  free(stringname);

  return answer;
}

int processutf8tag(FILE *fp, int header, const char *fname, const char *name, const char *str)
{
  char *path = 0;
  char *stringname = 0;
  char *string = 0;
  FILE *fpstr;
  int answer = 0;
  int i;
  int error;

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
    
  if (header)
  {
      fprintf(fp, "extern char %s[];\n", stringname);
      free(path);
      free(stringname);
      return 0;
  }
    
  if (path)
  {
      string = loadasutf8(path, &error);
      if (!string)
      {
          fprintf(stderr, "Can't load %s\n", path);
          return -1;
      }
  }
  else if(str)
  {
    string = mystrdup(str);
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
    fprintf(fp, "char %s[] = {\n", stringname);
    for (i =0; string[i]; i++)
    {
        fprintf(fp, "0x%02x, ", (unsigned char) string[i]);
        if ((i % 10) == 9)
            fprintf(fp, "\n");
    }
    fprintf(fp, "0x00\n");
    fprintf(fp, "};\n");
  }
  
  free(path);
  free(string);
  free(stringname);

  return answer;
}

unsigned short *utf8toutf16(const char *utf8, int allowsurrogatepairs, int *error)
{
    unsigned short *answer;
    int N = 0;
    int Nout = 0;
    int i, j;
    const char *ptr;
    int ch;
    
    if (error)
        *error = 0;
    N = bbx_utf8_Nchars(utf8);
    ptr = utf8;
    for (i =0; i < N; i++)
    {
        ch = bbx_utf8_getch(ptr);
        if (ch > 0xFFFF)
            Nout += 2;
        else
            Nout += 1;
        ptr += bbx_utf8_skip(ptr);
    }
    answer = malloc( (Nout +1) * sizeof(short));
    if (!answer)
    {
        if (error)
            *error = 1;
        return 0;
    }
    
    ptr = utf8;
    j = 0;
    for (i = 0; i < N; i++)
    {
        ch = bbx_utf8_getch(ptr);
        if (ch < 0 || ch > 0xFFFF)
        {
            if (allowsurrogatepairs && ch > 0)
            {
                unsigned short highsurrogate = 0xD800 | ((ch >> 10) & 0x03FF);
                unsigned short lowsurrogate = 0xDC00 | (ch & 0x03FF);
                answer[j++] = highsurrogate;
                answer[j++] = lowsurrogate;
            }
            else
            {
                if (error)
                    *error = -2;
                ch = 0xFFFE;
                answer[j++] = (unsigned short) ch;
            }
        }
        else
        {
            answer[j++] = (unsigned short) ch;
        }
        
        ptr += bbx_utf8_skip(ptr);
    }
    answer[j] = 0;
    
    return answer;
}

int processutf16tag(FILE *fp, int header, const char *fname, const char *name, const char *allowsurrogatepairs, const char *str)
{
  char *path = 0;
  char *stringname = 0;
  char *string = 0;
  FILE *fpstr;
  int answer = 0;
  int i;
  int error;
  unsigned short *utf16 = 0;
  int allowsurrogatesflag = 0;

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
  if (header)
  {
      fprintf(fp, "extern unsigned char %s[];\n", stringname);
      free(path);
      free(stringname);
      return 0;
  }
  if (path)
  {
      string = loadasutf8(path, &error);
      if (!string)
      {
          fprintf(stderr, "Can't load %s\n", path);
          return -1;
      }
  }
  else if(str)
  {
    string = mystrdup(str);
  }
    
  if (allowsurrogatepairs)
  {
      allowsurrogatesflag = parseboolean(allowsurrogatepairs, &error);
      if (error)
      {
          fprintf(stderr, "Bad boolean value to allowsurrogatepairs: \"%s\"\n", allowsurrogatepairs);
          answer = -1;
      }
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
      utf16 = utf8toutf16(string, allowsurrogatesflag, &error);
    if (error == -2)
      fprintf(stderr, "Not all values in %s can be represented in UTF-16\n", stringname);
    if (!utf16)
        answer = -1;
    else
    {
        fprintf(fp, "unsigned short %s[] = {\n", stringname);
        for (i = 0; utf16[i]; i++)
        {
            fprintf(fp, "0x%04x, ", (unsigned short) utf16[i]);
            if ((i % 10) == 9)
                fprintf(fp, "\n");
        }
        fprintf(fp, "0x0000\n");
        fprintf(fp, "};\n");
    }
  }
  
  free(path);
  free(string);
  free(stringname);
  free(utf16);
    
  return answer;
}

int processcommenttag(FILE *fp, const char *fname, const char *xform, const char *str, void *xformcontext)
{
    char *comment  = 0;
    char *string = 0;
    int error = 0;
    
    if (fname)
    {
        string = loadasutf8(fname, &error);
        if (!string)
        {
            fprintf(stderr, "Can't load %s\n", fname);
            goto error_exit;
        }
        if (xform)
        {
            comment = xformdata(xformcontext, xform, string);
            if (!comment)
            {
                fprintf(stderr, "Out of memory\n");
                goto error_exit;
            }
            free (string);
            string = comment;
            comment = 0;
        }
        comment = makecomment(string);
        if (!comment)
        {
            fprintf(stderr, "Out of memory\n");
            goto error_exit;
        }
    }
    else if (str)
    {
        if (xform)
        {
            string = xformdata(xformcontext, xform, str);
            if (!string)
            {
                fprintf(stderr, "Out of memory\n");
                goto error_exit;
            }
            str = string;
        }
        comment = makecomment(str);
        if (!comment)
        {
            fprintf(stderr, "Out of memory\n");
            goto error_exit;
        }
    }
    else
    {
        fprintf(stderr, "comment tag has no text\n");
        goto error_exit;
    }
    
    fputs(comment, fp);
    fprintf(fp, "\n");
    free(string);
    free(comment);
    return 0;
    
error_exit:
    free(string);
    free(comment);
    return -1;
        
}

int processincludetag(FILE *fp, int header,  const char *fname, const char *system, const char *alsoheader)
{
    int alsoheaderflag = 1;
    int systemflag = 0;
    int error;
    
    if (system)
    {
        systemflag = parseboolean(system, &error);
        if (error)
        {
            fprintf(stderr, "system attribute of include element must be boolean\n");
            return -1;
        }
    }
    if (alsoheader)
    {
        systemflag = parseboolean(alsoheader, &error);
        if (error)
        {
            fprintf(stderr, "header attribute of include element must be boolean\n");
            return -1;
        }
    }
    if (!fname)
    {
        fprintf(stderr, "include element must have src attribute set\n");
        return -1;
    }
    if (header && !alsoheaderflag)
        return 0;
    if (systemflag)
        fprintf(fp, "#include <%s>\n", fname);
    else
        fprintf(fp, "#include \"%s\"\n", fname);
    
    return 0;
}

int processbinarytag(FILE *fp, int header, const char *fname, const char *name)
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
  if(dumpbinary(fp, header, fname, binaryname) < 0)
  {
    fprintf(stderr, "Error processing %s\n", fname);
    answer = -1;
  } 
  free(binaryname);

  return answer;
}

int processcursortag(FILE *fp, int header, const char *fname, const char *name)
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
	if (dumpcursor(fp, header, cursor, cursorname) < 0)
	{
		fprintf(stderr, "Error processing %s\n", fname);
		answer = -1;
	}
	free(cursorname);
	free(cursor->rgba);
	free(cursor);

	return answer;
}

short *resampleaudio(const short *pcm, long samplerate, int Nchannels, long Nsamples, long resamplerate, long *Nsamplesout)
{
    float *fpcmin = 0;
    float *fpcmout = 0;
    short *answer = 0;
    long Nout;
    SRC_DATA src_data;
    
    fpcmin = malloc(Nsamples * Nchannels * sizeof(float));
    if (!fpcmin)
        goto error_exit;
    Nout = (long) ((Nchannels * Nsamples * (double) resamplerate + 1204.0)/samplerate);
    fpcmout = malloc(Nout * sizeof(float));
    if (!fpcmout)
        goto error_exit;
    
    src_short_to_float_array(pcm, fpcmin, (int) (Nsamples * Nchannels) );
    
    src_data.data_in = fpcmin;
    src_data.data_out = fpcmout;
    src_data.input_frames = Nsamples;
    src_data.output_frames = Nout / Nchannels;
    src_data.input_frames_used = 0;
    src_data.output_frames_gen = 0;
    src_data.end_of_input = 0;
    src_data.src_ratio = ((double)resamplerate)/(double)(samplerate);
    
    if (!src_is_valid_ratio(src_data.src_ratio))
        goto error_exit;
    
    src_simple(&src_data, SRC_SINC_BEST_QUALITY, Nchannels);
    answer = malloc(src_data.output_frames_gen * Nchannels * sizeof(short));
    if (!answer)
        goto error_exit;

    src_float_to_short_array(src_data.data_out, answer, src_data.output_frames_gen * Nchannels);
    
    if (Nsamplesout)
        *Nsamplesout = src_data.output_frames_gen;
    free(fpcmin);
    free(fpcmout);
    
    return answer;
    
error_exit:
    free(fpcmin);
    free(fpcmout);
    free(answer);
    return 0;
}

short *loadaudiofile(const char *fname, long *samplerate, int *Nchannels, long *Nsamples)
{
    char *ext;
    short *answer = 0;

    ext = getextension((char *)fname);
    if (ext)
        makelower(ext);
    
    if (!strcmp(ext, ".wav"))
    {
        answer = loadwav(fname, samplerate, Nchannels, Nsamples);
    }
    else if (!strcmp(ext, ".aif") || !strcmp(ext, ".aiff"))
    {
        answer = loadaiff(fname, samplerate, Nchannels, Nsamples);
    }
    else if(!strcmp(ext, ".mp3"))
    {
        answer = loadmp3(fname, samplerate, Nchannels, Nsamples);
    }

    free(ext);
    
    return answer;
}

int processaudiotag(FILE *fp, int header, const char *fname, const char *name, const char *sampleratestr)
{
    char *audioname;
    int answer = 0;
    int err;
    long samplerate;
    int Nchannels;
    long Nsamples;
    short *pcm;
    char *end;
    
    if (!fname)
    {
        fprintf(stderr, "Error, audio without src attribute\n");
        return -1;
    }

    if (name)
        audioname = mystrdup(name);
    else
        audioname = getbasename((char*)fname);
    pcm = loadaudiofile(fname, &samplerate, &Nchannels, &Nsamples);
    if (!pcm)
    {
        fprintf(stderr, "can't load audio %s\n", fname);
        return -1;
    }
    if (sampleratestr)
    {
        long resamplerate = strtol(sampleratestr, &end, 10);
        short *resampledpcm;
        long Nresamples;
        if (resamplerate <= 0)
        {
            fprintf(stderr, "audio samplerate must be positive\n");
            return -1;
        }
        if (samplerate != resamplerate)
        {
            resampledpcm = resampleaudio(pcm, samplerate, Nchannels, Nsamples,
                                         resamplerate, &Nresamples);
            if (resampledpcm)
            {
                free(pcm);
                pcm = resampledpcm;
                resampledpcm = 0;
                Nsamples = Nresamples;
                samplerate = resamplerate;
            }
            else
            {
                fprintf(stderr, "failed to resample %s\n", audioname);
                return -1;
            }
        }
    }
    if (dumpaudio(fp, header, pcm, samplerate, Nchannels, Nsamples, audioname) < 0)
    {
        fprintf(stderr, "Error processing %s\n", fname);
        answer = -1;
    }
    free(audioname);
    free(pcm);

    return answer;
}

int processinternationalnode(FILE *fp, XMLNODE *node, int header)
{
    XMLNODE *child;
    const char *name;
    const char *language;
    const char *str;
    const char *path;
    char *string;
    int Nchildren;
    FILE *fpstr;
    char *buff;
    int i, ii;
    int answer = 0;
    int error;
    char stringname[256];
    
    name = xml_getattribute(node, "name");
    Nchildren = xml_Nchildrenwithtag(node, "string");
   
    if (header)
    {
        fprintf(fp, "char *get_%s(const char *language);\n", name);
        return 0;
    }
    for(i=0;i<Nchildren;i++)
    {
        child = xml_getchild(node, "string", i);
        path = xml_getattribute(child, "src");
        language = xml_getattribute(child, "language");
        if (!language)
        {
            fprintf(stderr, "Children of international nodes must have\"language\" attribute set.\n");
            return -1;
        }
        str = xml_getdata(child);
        string = 0;
        snprintf(stringname, 256, "%s_%s", name, language);
        if(path)
        {
          fpstr = fopen(path, "r");
          if(!fpstr)
          {
            fprintf(stderr, "Can't open %s\n", path);
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
        else
        {
          fprintf(fp, "char *%s = ", stringname);
          fputs(string, fp);
          fprintf(fp, ";\n");
        }
        free(string);
    }
    Nchildren = xml_Nchildrenwithtag(node, "utf8");
    for(i=0;i<Nchildren;i++)
    {
        child = xml_getchild(node, "utf8", i);
        path = xml_getattribute(child, "src");
        language = xml_getattribute(child, "language");
        if (!language)
        {
            fprintf(stderr, "Children of international nodes must have\"language\" attribute set.\n");
            return -1;
        }
        str = xml_getdata(child);
        snprintf(stringname, 256, "%s_%s", name, language);
        if(path)
        {
            string = loadasutf8(path, &error);
        }
        else if(str)
        {
          string = mystrdup(str);
        }
        if (string)
        {
            fprintf(fp, "char %s[] = {\n", stringname);
            for (ii = 0; string[ii]; ii++)
            {
                fprintf(fp, "0x%02x, ", (unsigned char) string[ii]);
                if ((ii % 10) == 9)
                    fprintf(fp, "\n");
            }
            if ((ii % 10) == 9)
                fprintf(fp, "\n");
            fprintf(fp, "0x00\n");
            fprintf(fp, "};\n");
        }
        free(string);
    }
    
    fprintf(fp, "char *get_%s(const char *language)\n", name);
    fprintf(fp, "{\n");
    Nchildren = xml_Nchildrenwithtag(node, "string");
    for(i=0;i<Nchildren;i++)
    {
        child = xml_getchild(node, "string", i);
        language = xml_getattribute(child, "language");
        fprintf(fp, "    if (!strcmp(\"%s\", language))\n", language);
        fprintf(fp, "        return %s_%s;\n", name, language);
    }
    Nchildren = xml_Nchildrenwithtag(node, "utf8");
    for(i=0;i<Nchildren;i++)
    {
        child = xml_getchild(node, "utf8", i);
        language = xml_getattribute(child, "language");
        fprintf(fp, "    if (!strcmp(\"%s\", language))\n", language);
        fprintf(fp, "        return %s_%s;\n", name, language);
    }
    fprintf(fp, "    return 0;\n");
    fprintf(fp, "}\n");
    
    return 0;
}

MINIBASIC *creatmininbasic(void)
{
    MINIBASIC *mb;
    
    mb = malloc(sizeof(MINIBASIC));
    if (!mb)
        goto out_of_memory;
    mb->names = 0;
    mb->scripts = 0;
    mb->Nscripts = 0;
    
    return mb;
out_of_memory:
    return 0;
}

void killminibasic(MINIBASIC *mb)
{
    int i;
    
    if (mb)
    {
        for (i =0; i < mb->Nscripts; i++)
        {
            free(mb->names[i]);
            free(mb->scripts[i]);
        }
        free(mb);
    }
}

int loadminibasic_r(XMLNODE *node, MINIBASIC *mb)
{
    FILE *fp;
    const char *name;
    char *script;
    void *temp;
    int answer = 0;
    
    while (node)
    {
        if (!strcmp(xml_gettag(node), "minibasic"))
        {
            temp = realloc(mb->names, (mb->Nscripts + 1) * sizeof(char *));
            if (!temp)
                goto out_of_memory;
            mb->names = temp;
            mb->names[mb->Nscripts] = 0;
            
            temp = realloc(mb->scripts, (mb->Nscripts + 1) * sizeof(char *));
            if (!temp)
                goto out_of_memory;
            mb->scripts = temp;
            mb->scripts[mb->Nscripts] = 0;
            
            name = xml_getattribute(node, "name");
            if (!name)
            {
                fprintf(stderr, "minibasic element line %d lacks name attribute\n",
                        xml_getlineno(node));
                break;
            }
            
            if (xml_getattribute(node, "src"))
            {
                fp = fopen(xml_getattribute(node, "src"), "r");
                if (!fp)
                {
                    fprintf(stderr, "minibasic element line %d can't open file %s\n",
                            xml_getlineno(node), xml_getattribute(node, "src"));
                    break;
                }
                script = fslurp(fp);
                if (!script)
                    goto out_of_memory;
                fclose(fp);
            }
            else if (xml_getdata(node))
            {
                script = mystrdup(xml_getdata(node));
                if (!script)
                    goto out_of_memory;
            }
            else
            {
                fprintf(stderr, "don't have data for minibasic element line %d\n",
                        xml_getlineno(node));
            }
                
            mb->names[mb->Nscripts] = mystrdup(name);
            mb->scripts[mb->Nscripts] = script;
            mb->Nscripts++;
        }
        
        if (node->child)
            answer |= loadminibasic_r(node->child, mb);
        
        node = node->next;
    }
    
    return answer;
    out_of_memory:
    return -1;
    
}

char *xformdata(void *minibasic, const char *scriptname, const char *str)
{
    MINIBASIC *mb;
    char *answer = 0;
    FILE *fpin = 0;
    FILE *fpout = 0;
    int len;
    int i;
    int err;
     
    mb = minibasic;
    
    if (!scriptname)
        return 0;
    if (!str)
        str = "";

    len = strlen(str);
    for (i = 0; i < mb->Nscripts; i++)
    {
        if (!strcmp(mb->names[i], scriptname))
        {
            fpin = tmpfile();
            fpout = tmpfile();
            
            if (fwrite(str, 1, len, fpin) != len )
                goto out_of_memory;
            fprintf(fpin, "\n");
            fseek(fpin, 0, SEEK_SET);
            
            err = basic(mb->scripts[i], fpin, fpout, stderr);
            if (err)
                goto error_exit;
            fseek(fpout, 0, SEEK_SET);
            answer = fslurp(fpout);
            if (!answer)
                goto out_of_memory;
            removetrailingnewline(answer);
            
            fclose(fpout);
            fclose(fpin);
            fpout = 0;
            fpin = 0;
            
            break;
        }
    }
    if (i == mb->Nscripts)
    {
        fprintf(stderr, "transform %s does not exist.\n", scriptname);
        goto error_exit;
    }
    
    return answer;
out_of_memory:
    free(answer);
    fclose(fpin);
    fclose(fpout);
    return 0;
    
error_exit:
    free(answer);
    fclose(fpin);
    fclose(fpout);
    return 0;
}

void reportunknownattributes(XMLNODE *node, XMLATTRIBUTE *bad)
{
    XMLATTRIBUTE *attr;
    fprintf(stderr, "Element <%s> line %d has unrecognised attributes:\n", xml_gettag(node),xml_getlineno(node));
    for (attr = bad; attr != NULL; attr = attr->next)
        fprintf(stderr, "\t%s\n", attr->name);
}

int checkunknownattributes_r(XMLNODE *node)
{
    const char *tag = 0;
    XMLATTRIBUTE *badattributes = 0;
    int answer = 0;
    char *end = 0;
    
    while (node)
    {
        tag = xml_gettag(node);
        badattributes = 0;
        
        if (!strcmp(tag, "BabyXRC"))
        {
            badattributes = xml_unknownattributes(node, "src", end);
            // root node, don't do sibs.
            if (node->child)
                answer = checkunknownattributes_r(node->child);
            break;
        }
        else if (!strcmp(tag, "comment"))
        {
            badattributes = xml_unknownattributes(node, "src", end);
        }
        else if (!strcmp(tag, "include"))
        {
            badattributes = xml_unknownattributes(node, "src", "system", "header", end);
        }
        else if (!strcmp(tag, "image"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", "width", "height", end);
         }
        else if (!strcmp(tag, "font"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", "points", end);
        }
        else if (!strcmp(tag, "string"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", "language", end);
        }
        else if (!strcmp(tag, "utf8"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", "language", end);
        }
        else if (!strcmp(tag, "utf16"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", "allowsurrogatepairs", end);
        }
        else if(!strcmp(tag, "binary"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", end);
        }
        else if (!strcmp(tag, "cursor"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", end);
        }
        else if (!strcmp(tag, "audio"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", "samplerate", end);
        }
        else if (!strcmp(tag, "international"))
        {
            badattributes = xml_unknownattributes(node, "name", end);
        }
        else if (!strcmp(tag, "dataframe"))
        {
            badattributes = xml_unknownattributes(node, "src", "name", "xpath", "ctype", "external", "usechildren", "useattributes", end);
        }
        else if (!strcmp(tag, "field"))
        {
            badattributes = xml_unknownattributes(node, "name", "xpath", "ctype", "format", "external", end);
        }
        else
        {
            fprintf(stderr, "unrecognised element tag %s line %d\n", tag, xml_getlineno(node));
        }
        
        if (badattributes)
        {
            reportunknownattributes(node, badattributes);
            answer = 1;
        }
        
        if (node->child)
            answer |= checkunknownattributes_r(node->child);
        
        node = node->next;
    }
    
    return answer;
}

void usage(void)
{
  printf("The Baby X resource compiler v1.1\n");
  printf("by Malcolm Mclean\n");
  printf("\n");
  printf("Usage: babyxrc [-header] <script.xml>\n");
  printf("\n");
  printf("-header write a .h header file instead of a .c source file.\n");
  printf("Example script file:\n");
  printf("<BabyXRC>\n");
  printf("<image src = \"smiley.png\", name = \"fred\", width = \"10\", height = \"10\"> </image>\n");
  printf("<image src = \"lena.jpg\"> </image>\n");
  printf("<font src = \"arial.ttf\", name = \"arial12\", points=\"12\"> </font>\n");
  printf("<string src = \"external.txt\"> </string>\n");
  printf("<string name = \"internal\"> \"A C string\\n\"> </string>\n");
  printf("<string name = \"embedded\"> Embedded string </string>\n");
  printf("<audio src = \"beep.wav\", samplerate = \"44100\"></audio>\n");
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
  printf("<audio>\n");
  printf("Audio in .wav, .aiff or .mp3 format. Resamples to the rate passed.\n");
  printf("<binary>\n");
  printf("Dump raw binary data.\n");   
  exit(EXIT_FAILURE);
}




int main(int argc, char **argv)
{
  OPTIONS *opt = 0;
  XMLDOC *doc;
  char errormessage[256];
  char *scriptfile;
  XMLNODE **scripts;
  XMLNODE *node;
  int Nscripts;
  int header = 0;
  MINIBASIC *mb =0;
  int i;
  const char *path;
  const char *name;
  const char *str;
  const char *xform;
  const char *widthstr;
  const char *heightstr;
  const char *pointsstr;
  const char *sampleratestr;
  const char *allowsurrogatepairsstr;
  const char *system;
  const char *headerstr;
  
  opt = options(argc, argv, 0);
  header = opt_get(opt, "-header", 0);
  if(opt_Nargs(opt) != 1)
    usage();
  scriptfile = opt_arg(opt, 0);
  if (opt_error(opt, stderr))
        exit(EXIT_FAILURE);
  killoptions(opt);
  opt = 0;

  doc = loadxmldoc(scriptfile, errormessage, 256);
  if(!doc)
  {
    fprintf(stderr, "Can't read resource script file (%s)\n", errormessage);
    exit(EXIT_FAILURE);
  } 
  scripts = xml_getdescendants(xml_getroot(doc), "BabyXRC", &Nscripts);
  if (header)
  {
      char *basename = getbasename(scriptfile);
      for (i = 0; basename[i]; i++)
      {
          if (!isalnum((unsigned char) basename[i]))
              basename[i] = '_';
      }
      if (!isalpha((unsigned char) basename[0]) && strlen(basename))
          basename[0] = 'X';
      fprintf(stdout, "#ifndef %s_h\n", basename);
      fprintf(stdout, "#define %s_h\n", basename);
      fprintf(stdout, "\n");
      free(basename);
  }
  for(i=0;i<Nscripts;i++)
  {
    checkunknownattributes_r(scripts[i]);
    mb = creatmininbasic();
    loadminibasic_r(scripts[i]->child, mb);
      
    if (i == 0 && xml_Nchildrenwithtag(scripts[i], "international") > 0)
          fprintf(stdout, "#include <string.h>\n");
    if(i == 0 && xml_Nchildrenwithtag(scripts[i], "font") > 0)
    {
        fprintf(stdout, "#ifndef BBX_BITMAPFONTDEFINED\n");
        fprintf(stdout, "#define BBX_BITMAPFONTDEFINED\n");
        putfontdefinition(stdout);
        fprintf(stdout, "#endif\n");
    }
	if (i == 0 && xml_Nchildrenwithtag(scripts[i], "cursor") > 0)
    {
        fprintf(stdout, "#ifndef BBX_CURSORDEFINED\n");
        fprintf(stdout, "#define BBX_CURSORDEFINED\n");
        putcursordefinition(stdout);
        fprintf(stdout, "#endif\n");
    }
    for (node = scripts[i]->child; node != NULL; node = node->next)
    { 
        const char* tag = xml_gettag(node);
        if (!strcmp(tag, "comment"))
        { 
            path = xml_getattribute(node, "src");
            xform = xml_getattribute(node, "xform");
            str = xml_getdata(node);
            processcommenttag(stdout, path, xform, str, mb);
        }
        if (!strcmp(tag, "include"))
        {
            path = xml_getattribute(node, "src");
            system = xml_getattribute(node, "system");
            headerstr = xml_getattribute(node, "header");
            processincludetag(stdout, header, path, system, headerstr);
        }
        else if (!strcmp(tag, "image"))
        {
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            widthstr = xml_getattribute(node, "width");
            heightstr = xml_getattribute(node, "height"); 
            processimagetag(stdout, header, path, name, widthstr, heightstr);
         }
        else if (!strcmp(tag, "font"))
        {
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            pointsstr = xml_getattribute(node, "points");
            processfonttag(stdout, header, path, name, pointsstr);
        }
        else if (!strcmp(tag, "string"))
        {
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            xform = xml_getattribute(node, "xform");
            str = xml_getdata(node);
            processstringtag(stdout, header, path, name, xform, str, mb);
        }
        else if (!strcmp(tag, "utf8"))
        {
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            str = xml_getdata(node);
            processutf8tag(stdout, header, path, name, str);
        }
        else if (!strcmp(tag, "utf16"))
        {
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            allowsurrogatepairsstr = xml_getattribute(node, "allowsurrogatepairs");
            str = xml_getdata(node);
            processutf16tag(stdout, header, path, name, allowsurrogatepairsstr, str);
        }
        else if(!strcmp(tag, "binary"))
        { 
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            processbinarytag(stdout, header, path, name);
        }
        else if (!strcmp(tag, "cursor"))
        {
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            processcursortag(stdout, header, path, name);
        }
        else if (!strcmp(tag, "audio"))
        {
            path = xml_getattribute(node, "src");
            name = xml_getattribute(node, "name");
            sampleratestr = xml_getattribute(node, "samplerate");
            processaudiotag(stdout, header, path, name, sampleratestr);
        }
        else if (!strcmp(tag, "international"))
        {
            processinternationalnode(stdout, node, header);
        }
        else if (!strcmp(tag, "dataframe"))
        {
            processdataframenode(stdout, node, header, mb);
        }
    }
      killminibasic(mb);
  }
  if (header)
  {
      fprintf(stdout, "\n#endif\n");
  }
  killxmldoc(doc);
    free(scriptfile);
    free(scripts);


  return 0;
}
