//
//  babyxfs_escapexml.c
//  BabyXFS project
//
//  Created by Malcolm McLean on 31/05/2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

/*
   Escape a string to write as XML
 */
static char *xml_escape(const char *data)
{
    int i;
    int size = 0;
    char *answer;
    char *ptr;
    
    for (i = 0; data[i]; i++)
    {
        switch(data[i]) {
            case '&':  size += 5; break;
            case '\"': size += 6; break;
            case '\'': size += 6; break;
            case '<':  size += 4; break;
            case '>':  size += 4; break;
            default:   size += 1; break;
        }
    }
    answer = malloc(size+1);
    if (!answer)
        goto out_of_memory;
    
    ptr = answer;
    for (i = 0; data[i]; i++) {
        switch(data[i]) {
            case '&':  strcpy(ptr, "&amp;"); ptr += 5;     break;
            case '\"': strcpy(ptr, "&quot;"); ptr += 6;    break;
            case '\'': strcpy(ptr, "&apos;"); ptr += 6;     break;
            case '<':  strcpy(ptr, "&lt;"); ptr += 4;      break;
            case '>':  strcpy(ptr, "&gt;"); ptr += 4;      break;
            default:   *ptr++ = data[i]; break;
        }
    }
    *ptr++ = 0;
    
    return answer;
out_of_memory:
    return 0;
}

/*
  is a file binary?
 */
static int is_binary(const char *path)
{
    int answer = 0;
    int ch;
    FILE *fp;
    
    fp = fopen(path, "rb");
    if (!fp)
        return 0;
    while ((ch = fgetc(fp)) != EOF)
    {
      if (ch < 32)
      {
          if (ch != '\t' && ch != '\n' && ch != '\r')
          {
              answer = 1;
              break;
          }
      }
       if (ch > 127)
       {
           answer = 1;
           break;
       }
    }
    fclose(fp);
    return answer;
}

void usage()
{
    fprintf(stderr, "babyxfs_escapexml - escapes ascii to xml\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: babyfs_escape_xml <ascii.txt>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "By Malcolm McLean\n");
    fprintf(stderr, "Part of the BabyX project.\n");
    fprintf(stderr, "Check us out on github and get involved.\n");
    fprintf(stderr, "Program and source free to anyone for any use.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    FILE *fp = 0;
    char *xmlstring = 0;
    char *plainstring = 0;
    
    if (argc != 2)
        usage();
    
    if (is_binary(argv[1]))
    {
        fprintf(stderr, "%s is binary\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    fp = fopen(argv[1], "r");
    if (!fp)
    {
        fprintf(stderr, "Can't source file\n");
        exit(EXIT_FAILURE);
    }
    plainstring = fslurp(fp);
    if (!plainstring)
    {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    fp = 0;
    
    xmlstring = xml_escape(plainstring);
    if (!xmlstring)
    {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", xmlstring);
    free(xmlstring);
    free(plainstring);
    
    return 0;
}

