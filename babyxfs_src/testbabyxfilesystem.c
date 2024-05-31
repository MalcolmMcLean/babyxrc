//
//  testbabyxfilesystem.c
//  testbabyxfilesystem
//
//  Created by Malcolm McLean on 31/05/2024.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "bbx_filesystem.h"

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

static int streamsidentical(FILE *fpa, FILE *fpb)
{
    int cha, chb;
    
    while ( (cha = fgetc(fpa)) != EOF)
    {
        chb = fgetc(fpb);
        if (cha != chb)
            return 0;
    }
    chb = fgetc(fpb);
    if (cha != chb)
        return 0;
    
    return 1;
}

void usage()
{
    fprintf(stderr, "testbabyxfilesystem Tests the Baby X file system BBX_FileSystem\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: testbabyxfilesystem <directory> <directory.xml> <path>\n");
    fprintf(stderr, "\t<directory> - path to the local directory to mount.\n");
    fprintf(stderr, "\t<directory.xml> - FileSystem XML generated from the directory.\n");
    fprintf(stderr, "\t<path> - path to file in the directory to extract.\n");
    fprintf(stderr, "\t\tpath in the form \"/directory/subfolder/readme.txt\".\n");
    fprintf(stderr, "\t\tpass \"-getname\" for the path to get the filesystem names.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "By Malcolm McLean\n");
    fprintf(stderr, "Part of the BabyX project.\n");
    fprintf(stderr, "Check us out on github and get involved.\n");
    fprintf(stderr, "Program and source free to anyone for any use.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    BBX_FileSystem *bbx_fs_xml = 0;
    BBX_FileSystem *bbx_fs_stdio = 0;
    FILE *fp;
    FILE *fp_stdio = 0;
    FILE *fp_xml = 0;
    int ch;
    char *xmlstring;
    int err;
    if (argc != 4)
    {
        usage();
    }
    
    fp = fopen(argv[2], "r");
    if (!fp)
    {
        fprintf(stderr, "Can't open xml file\n");
        exit(EXIT_FAILURE);
    }
    xmlstring = fslurp(fp);
    if (!xmlstring)
    {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    fp = 0;
    
    bbx_fs_stdio = bbx_filesystem();
    /* no need to check in Baby X, Baby X functions never return out of memory */
    /* Comment one of these in and out to toggle between filesystems */
    err = bbx_filesystem_set(bbx_fs_stdio, argv[1], BBX_FS_STDIO);
    if (err)
    {
        fprintf(stderr, "Can't set up stdio filessystem\n");
        exit(EXIT_FAILURE);
    }
    bbx_fs_xml = bbx_filesystem();
    err = bbx_filesystem_set(bbx_fs_xml, xmlstring, BBX_FS_STRING);
    if (err)
    {
        fprintf(stderr, "Can't set up XML filessystem\n");
        exit(EXIT_FAILURE);
    }
   
    if (!strcmp(argv[3], "-getname"))
    {
        printf("stdio filesystem name %s\n", bbx_filesystem_getname(bbx_fs_stdio));
        printf("xml filesystem name %s\n", bbx_filesystem_getname(bbx_fs_xml));
    }
    else
    {
        fp_stdio = bbx_filesystem_fopen(bbx_fs_stdio, argv[3], "r");
        if (!fp_stdio)
        {
            fprintf(stderr, "Can't open target file on stdio system\n");
            exit(EXIT_FAILURE);
        }
        fp_xml = bbx_filesystem_fopen(bbx_fs_xml, argv[3], "r");
        if (!fp_xml)
        {
            fprintf(stderr, "Can't open target file on xml system\n");
            exit(EXIT_FAILURE);
        }
        if (!streamsidentical(fp_stdio, fp_xml))
        {
            fprintf(stderr, "Files differ\n");
        }
        fseek(fp_xml, 0, SEEK_SET);
        while ( (ch = fgetc(fp_xml)) != EOF)
            putchar(ch);
        err = bbx_filesystem_fclose(bbx_fs_stdio, fp_stdio);
        if (err)
            fprintf(stderr, "Error closing stdio file\n");
        err = bbx_filesystem_fclose(bbx_fs_xml, fp_xml);
        if (err)
            fprintf(stderr, "error closing xml file\n");
    }
    
    bbx_filesystem_kill(bbx_fs_stdio);
    bbx_filesystem_kill(bbx_fs_xml);
    free(xmlstring);
    
    return 0;
}

