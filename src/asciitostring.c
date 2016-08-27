#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "asciitostring.h"

static size_t linesbiggerthan(const char *str, size_t maxlen);
static int escaped(int ch);
static char escapechar(int ch);

/*
  load a text file into memory

*/
char *fslurp(FILE *fp)
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

/*
  test if a string is a valid C string - can it go in the place
   of a C string literal?
 */
int validcstring(const char *str)
{
  size_t len;
  size_t i, j;
  size_t start;
  size_t end;

  len = strlen(str);
  if(len < 2)
    return 0;
  for(start=0; start < len; start++)
    if(!isspace( (unsigned char) str[start]))
      break;
  end = len;
  while(end--)
    if(!isspace((unsigned char) str[end]))
      break;
  if(start == len || end == (size_t) -1)
    return 0;
  if(str[start] == 'L')
    start++;
  if(start == end || str[start] != '\"' || str[end] != '\"')
    return 0;
  start++;
  end--;

  for(i=start;i<=end;i++)
  {
    if(str[i] == '\\')
    {
      if(i == end)
        return 0;
      if(strchr("aftbrnvxou01234567\?\\\'\"", str[i+1]))
      {
        i++;
        continue;
      }
      else
        return 0;
    }
      
    if(str[i] == '\"')
    {
      for(j=i+1;j<end;j++)
        if(!isspace((unsigned char) str[j]))
          break;
      if(str[j] == '\"')
        i = j;
      else
        return 0;
     }
     if(!isgraph( (unsigned char) str[i]) && str[i] != ' ')
       return 0;
  }
  
  return 1;
}


/*
  convert a string to a C language string;
  Params:
    str - the string to convert
  Returns: C version of string, 0 on out of memory
  Notes: newlines are represented by breaks in the string.
*/
char *texttostring(const char *str)
{
  size_t len = 0;
  size_t i;
  size_t j = 0;
  size_t linelen = 0;
  char *answer;

  for(i=0;str[i];i++)
  {
    if(str[i] == '\n')
      len += 5;
    else if(escaped(str[i]))
      len+=2;
   else
     len += 1;
  }
  len += linesbiggerthan(str, 100) * 3;
  len++;
  len += 2;
  answer = malloc(len);
  if(!answer)
    return 0;
  answer[j++] = '"';
  for(i=0;str[i];i++)
  {
    if(str[i] == '\n' && str[i+1] != 0)
    {
      answer[j++] = '\\';
      answer[j++] = 'n';
      answer[j++] = '\"';
      answer[j++] = '\n';
      answer[j++] = '\"';
      linelen = 0;
    }
    else if(escaped(str[i]))
    {
      answer[j++] = '\\';
      answer[j++] = escapechar(str[i]);
      linelen++;
    }
    else
    {
      answer[j++] = str[i];
      linelen++;
    }
    if(linelen == 100 && str[i+1] != '\n')
    {
      answer[j++] = '\"';
      answer[j++] = '\n';
      answer[j++] = '\"';
      linelen = 0;
    }
  }
  answer[j++] = '\"';
  answer[j++] = 0;

  return answer;

}

/*
  test if a character is escaped in C
  Params: ch - the character to test
  Returns: 1 if escaped in C strings, else 0
*/
static int escaped(int ch)
{
  char *escapes = "\a\b\f\n\r\t\v\?\'\"\\";

  if(ch == 0)
    return 1;
  return strchr(escapes, ch) ? 1 : 0;

}

/*
  get the escape character to represent ch
  Params: ch - an escaped character
  Returns: character that stands in for it in esacpe sequence,
    0 if ch is not an escaped character
*/
static char escapechar(int ch)
{
  char *escapes = "\a\b\f\n\r\t\v\?\'\"\\";
  char *characters = "abfnrtv?\'\"\\";
  char *ptr;

  if(ch == 0)
    return '0';
  ptr = strchr(escapes, ch);
  if(ptr)
    return characters[ptr - escapes];
  else
    return 0;

}

/*
  get the number of lines bigger than a certain value
*/
static size_t linesbiggerthan(const char *str, size_t maxlen)
{
  size_t len = 0;
  size_t answer = 0;

  while(*str)
  {
    if(*str == '\n')
     len = 0;
    else
    {
      len++;
      if(len > maxlen)
      {
       len = 0;
       answer++;
      }
    }
     str++;
   }

  return answer;
}
