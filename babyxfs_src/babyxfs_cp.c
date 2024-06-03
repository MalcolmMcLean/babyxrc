//
//  babyxfs_cp.c
//  babyxfs
//
//  Created by Malcolm McLean on 28/05/2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "bbx_filesystem.h"


/*
    This program is an impementation of the cp command for FileSystem.xml files.
 
     We generate an XML file which represents a filesystem. Then we query it for files.
     Unfortunately there is no way to traverse a physical computer's filesystem with
     complete portablity, and so the encoder, babyxfs_dirtoxml, will not run everywhere.
     But the decoder is completetely portable. And you can incorporate it into your own
     programs to have embedded files. Use the Baby X resource compiler to convert the XML
     to a string, then load it using xmldocfromstring.
 
     The XML is very simple
     
     <FileSystem>
           <directory name="poems">
               <directory name="Shakespeare">
                    <file name="Sonnet 12" type="text">
                        When do I count the clock that tells the time?
                    </file>
                </directory>
                <directory name="Blake">
                    <file name="Tyger" type="text">
                            Tyger, tyger, burning bright,
                            Through the forests of the night,
                    </file>
                    <file name="BlakePicture.png" type="binary">
 <![CDATA[M)"E3'U@":H````0#)A$12!``#H`````6(8````056"`0``@"HFV0#!52#-$(
 M0)W;FE&;E!``(E8E7>`43EN%'_[>3/D`$2("(E0-4$D.!0*A>H((=04)D$(A
 M2,$(HBHB(+N"L60$1PR*ZJBH@K*UU*6P"+*6PN;06$1==Q"V0EW-P08W]-OW
 ]]>
                    </file>
                </directory>
           </directory>
     </FilesSystem>
 
     All the code was written by Malcolm  McLean.
      It is free for any use.
 */


void *bbx_malloc(size_t size)
{
    void *answer = 0;
    int N;
    
    N = (int) size;
    if (N < 0 || N != size)
    {
        fprintf(stderr, "Illegal memory request %d bytes\n", N);
        exit(EXIT_FAILURE);
    }
    if (N == 0)
        N = 1;
    answer = malloc( (size_t) N);
    if (!answer)
    {
        fprintf(stderr, "Baby X system out of memory\n");
        exit(EXIT_FAILURE);
    }
    
    return answer;
}

static char *bbx_strdup(const char *str)
{
    char *answer = bbx_malloc((int) strlen(str) +1);
    strcpy(answer, str);
    
    return answer;
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


static unsigned char *fslurpb(FILE *fp, int *len)

{
    unsigned char *answer = 0;
    unsigned char *temp;
    int capacity = 1024;
    int N = 0;
    int ch;

    answer = malloc(capacity);
    if (!answer)
        goto out_of_memory;
    while ( (ch = fgetc(fp)) != EOF)
    {
        answer[N++] = ch;
        if (N >= capacity - 4)
        {
            temp = realloc(answer, capacity + capacity / 2);
            if (!temp)
                goto out_of_memory;
            answer = temp;
            capacity = capacity + capacity / 2;
        }
    }
    *len = N;
    
    return answer;
    
out_of_memory:
    *len = -1;
    free(answer);
    return 0;
}

int isbinary(const unsigned char *data, int N)
{
    int i;
    
    for (i = 0; i < N; i++)
    {
        if (data[i] > 127)
            return  1;
        else if (data[i] < 32)
        {
            if (data[i] != '\n' && data[i] != '\r' &&data[i] != '\t')
                return 1;
        }
    }
    
    return  0;;
}


void usage()
{
    fprintf(stderr, "babyxfs_cp: copy a file to a FileSystem XML archive\n");
    fprintf(stderr, "Usage: - babyxfs_cp <filesystem.xml> <targetpath> <sourcepath>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Note the source path is currently on the host mchine.\n");
    fprintf(stder,, "\n");
    fprintf(stderr, "Generate the FileSystem files with the program babyxfs_dirtoxml\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "By Malcolm McLean\n");
    fprintf(stderr, "Part of the BabyX project.\n");
    fprintf(stderr, "Check us out on github and get involved.\n");
    fprintf(stderr, "Program and source free to anyone for any use.\n");
    exit(EXIT_FAILURE);
}



int docommand(BBX_FileSystem *fs, int argc, char **argv)
{
    FILE *fp;
    unsigned char *data;
    int N;
    
    fp = fopen(argv[2], "r");
    data = fslurpb(fp, &N);
    if (!data)
    {
        fprintf(stderr, "Can't read file\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    fp = 0;
    
    fp = bbx_filesystem_fopen(fs, argv[1], "w");
    if (fp)
    {
        fwrite(data, 1, N, fp);
        bbx_filesystem_fclose(fs, fp);
    }
    
    fp = bbx_filesystem_fopen(fs, argv[1], "w");
    if (fp)
    {
        char buff[256];
        while(fgets(buff, 256, fp))
         fprintf(stderr, "%s\n", buff);
    }
    bbx_filesystem_fclose(fs, fp);
    
}


int main(int argc, char **argv)
{
    char error[1024];
    char **list;
    int i;
    
    FILE *fp = 0;
    char *xmlstring = 0;
    BBX_FileSystem *bbx_fs_xml = 0;
    int err;
    
    if (argc < 2)
        usage();
    
    fp = fopen(argv[1], "r");
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
    
    bbx_fs_xml = bbx_filesystem();
    err = bbx_filesystem_set(bbx_fs_xml, xmlstring, BBX_FS_STRING);
    if (err)
    {
        fprintf(stderr, "Can't set up XML filessystem\n");
        exit(EXIT_FAILURE);
    }
   
    docommand(bbx_fs_xml, argc -1, argv + 1);
    
    bbx_filesystem_kill(bbx_fs_xml);
    free(xmlstring);
    
    return 0;
}
