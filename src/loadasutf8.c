#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "text_encoding_detect.h"
#include "loadasutf8.h"

static char *loadascii(const char *fname);
static char *loadutf8(const char *fname, int bom);
static char *loadUTF16(const char *fname, int bigendian, int bom);
static int bbx_utf8_putch(char *out, int ch);
static char *fslurp(FILE *fp);

char *loadasutf8(const char *filename, int *err)
{
   TextEncoding encoding = TEXTENC_None;
   char *answer = 0;

   if (*err)
     *err = 0;

   encoding = DetectTextFileEncoding(filename, err);
   switch (encoding)
   {
      case TEXTENC_None:
        goto error_exit;
        break;
      case TEXTENC_ANSI:
        goto error_exit;
        break;
      case TEXTENC_ASCII:
        answer = loadascii(filename);
        break;
      case TEXTENC_UTF8_NOBOM:
        answer = loadutf8(filename, 0);
        break;
      case TEXTENC_UTF8_BOM:
        answer = loadutf8(filename, 1);
        break;
      case TEXTENC_UTF16_LE_BOM:
        answer = loadUTF16(filename, 0, 1);
        break;
      case TEXTENC_UTF16_LE_NOBOM:
        answer = loadUTF16(filename, 0, 0);
        break;
      case TEXTENC_UTF16_BE_BOM:
        answer = loadUTF16(filename, 1, 1);
        break;
      case TEXTENC_UTF16_BE_NOBOM:
        answer = loadUTF16(filename, 1, 0);
        break;
   }
   if (!answer)
     goto error_exit;

   return answer;

error_exit:
   if (err && *err == 0)
      *err = -1;
   free(answer);
   return 0;
}

static char *loadascii(const char *fname)
{
   FILE *fp;
   char *answer = 0;

   fp = fopen(fname, "r");
   if (!fp)
      goto error_exit;
   answer = fslurp(fp);
   if (!answer)
      goto error_exit;
   fclose(fp);
   return answer;
error_exit:
   free(answer);
   return 0;
}

static char *loadutf8(const char *fname, int bom)
{
   FILE *fp;
   char * answer = 0;
   size_t len;

   fp = fopen(fname, "rb");
   if (!fp)
      goto error_exit;
   answer = fslurp(fp);
   if (!answer)
      goto error_exit;
   len = strlen(answer);
   if (bom && len >= 3)
   {
      memmove(answer, answer + 3, len - 3 + 1);
   }
   else if (bom && len < 3)
   {
      goto error_exit;
   }
   fclose(fp);
   return answer;
error_exit:
    free(answer);
    if (fp)
      fclose(fp);
    return 0;
}

static char *loadUTF16(const char *fname, int bigendian, int bom)
{
   FILE *fp;
   char *answer;
   int capacity = 1024;
   int pos = 0;
   int ch1, ch2;
   int codepoint;
   char *temp;

   fp = fopen(fname, "rb");
   if (!fp)
     return 0;
   answer = malloc(capacity);
   if (bom)
   {
      fgetc(fp);
      fgetc(fp);
   }
   do
   {
      ch1 = fgetc(fp);
      if (ch1 == EOF)
        break;
      ch2 = fgetc(fp);
      if (ch2 == EOF)
        goto error_exit;
      if (bigendian)
      {
         int tempch = ch1;
         ch1 = ch2;
         ch2 = tempch;
      }
      if ((ch2 & 0xFC) == 0xD8)
      {
          int ch3 = getc(fp);
          int ch4 = getc(fp);
          int highsurrogate, lowsurrogate;
          
          if (ch3 == EOF || ch4 == EOF)
              goto error_exit;
          if (bigendian)
          {
              int tempch = ch3;
              ch3 = ch4;
              ch4 = tempch;
          }
          if ((ch4 & 0xFC) == 0xDC)
          {
              highsurrogate = ((ch2 * 256) + ch1) & 0x03FF;
              lowsurrogate = ((ch4 * 256) + ch3) & 0x03FF;
              codepoint = 0x10000 + highsurrogate * 1024 + lowsurrogate;
          }
          else
          {
              goto error_exit;
          }
      }
      else  if ((ch2 & 0xFC) == 0xDC)
      {
          goto error_exit;
      }
      else
      {
          codepoint = ch2 * 256 + ch1;
      }
      if (pos > capacity - 8)
      {
         temp = realloc(answer, capacity + capacity / 2);
         if (!temp)
            goto error_exit;
         answer = temp;
         capacity = capacity + capacity / 2;
      }
      pos += bbx_utf8_putch(answer + pos, codepoint);
   } while(1);
   
   pos += bbx_utf8_putch(answer + pos, 0);

   temp = realloc(answer, pos);
   if (!temp)
      goto error_exit;
   answer = temp;
   fclose(fp);
   return answer;

error_exit:
    free(answer);
    return 0;      
}



static int bbx_utf8_putch(char *out, int ch)
{
  char *dest = out;
  if (ch < 0x80) 
  {
     *dest++ = (char)ch;
  }
  else if (ch < 0x800) 
  {
    *dest++ = (ch>>6) | 0xC0;
    *dest++ = (ch & 0x3F) | 0x80;
  }
  else if (ch < 0x10000) 
  {
     *dest++ = (ch>>12) | 0xE0;
     *dest++ = ((ch>>6) & 0x3F) | 0x80;
     *dest++ = (ch & 0x3F) | 0x80;
  }
  else if (ch < 0x110000) 
  {
     *dest++ = (ch>>18) | 0xF0;
     *dest++ = ((ch>>12) & 0x3F) | 0x80;
     *dest++ = ((ch>>6) & 0x3F) | 0x80;
     *dest++ = (ch & 0x3F) | 0x80;
  }
  else
    return 0;
  return dest - out;
}

/*
  load a text file into memory

*/
static char *fslurp(FILE *fp)
{
  char *answer;
  char *temp;
  int buffsize = 1024;
  int i = 0;
  int ch;

  answer = malloc(1024);
  if(!answer)
    return 0;
  while( (ch = fgetc(fp)) != EOF )
  {
    if(i == buffsize-2)
    {
      if(buffsize > INT_MAX - 100 - buffsize/10)
      {
	free(answer);
        return 0;
      }
      buffsize = buffsize + 100 * buffsize/10;
      temp = realloc(answer, buffsize);
      if(temp == 0)
      {
        free(answer);
        return 0;
      }
      answer = temp;
    }
    answer[i++] = (char) ch;
  }
  answer[i++] = 0;

  temp = realloc(answer, i);
  if(temp)
    return temp;
  else
    return answer;
}

int loadasutf8main(int argc, char **argv)
{
   char *utf8;
   int error = 0;

   if (argc == 2)
   {
      utf8 = loadasutf8(argv[1], &error);
      printf("%s\n", utf8);
      free(utf8);  
   }

   return 0;
}
