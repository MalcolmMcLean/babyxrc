//
//  babyxxmltodir.c
//  
//
//  Created by Malcolm McLean on 01/06/2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "bbx_write_source.h"

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

void usage(void)
{
    fprintf(stderr, "babyxfs_xmltodir - convert a FileSystem xml file to a directory\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: babyxfs_xmltodir - <filesystem.xml> <targetpath>\n");
    fprintf(stderr, "\tfilesystem.xml - a <FileSystem> xml file\n");
    fprintf(stderr, "\ttargetpath - path to the target directory\n");
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
    char *xml = 0;
    int err;
    
    if (argc != 3)
        usage();
    
    fp = fopen(argv[1], "r");
    if (!fp)
    {
        fprintf(stderr, "Can't ooen xml file %s\n", argv[1]);
        goto error_exit;
    }
    xml = fslurp(fp);
    if (!xml)
    {
        fprintf(stderr, "FIle too big, out of memory\n");
        goto error_exit;
    }
    fclose(fp);
    fp = 0;
    err = bbx_write_source(xml, argv[2], "..", "castarophe");
    if (err)
    {
        fprintf(stderr, "Error writing directory %s\n", argv[2]);
        goto error_exit;
    }
    
    free(xml);
    return 0;
error_exit:
    fclose(fp);
    free(xml);
    exit(EXIT_FAILURE);
}
