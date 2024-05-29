#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 strdup dtop in replacement
 */
static char *mystrdup(const char *str)
{
  char *answer;

  answer = malloc(strlen(str) + 1);
  if(answer)
    strcpy(answer, str);

  return answer;
}

/*
    Concatenate two strings, returuning an allocated result.
 */
static char *mystrconcat(const char *prefix, const char *suffix)
{
    int lena, lenb;
    char *answer;
    
    lena = (int) strlen(prefix);
    lenb = (int) strlen(suffix);
    answer = malloc(lena + lenb + 1);
    if (answer)
    {
        strcpy(answer, prefix);
        strcpy(answer + lena, suffix);
    }
    return  answer;
}


/*
   get the filename at the end if a path
 */
static char *getfilename(const char *path)
{
    const char *answer;
    
    answer = strrchr(path, '/');
    if (answer)
        answer = answer + 1;
    else
        answer = path;
    
    return mystrdup(answer);
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

/*
  Load a binary file into memory
 */
static unsigned char *slurpb(const char *fname, int *len)
{
    FILE *fp;
    unsigned char *answer = 0;
    unsigned char *temp;
    int capacity = 1024;
    int N = 0;
    int ch;

    fp = fopen(fname, "rb");
    if (!fp)
    {
        *len = -2;
        return 0;
    }
    answer = malloc(capacity);
    if (!answer)
        goto out_of_memory;
    while ( (ch = fgetc(fp)) != EOF)
    {
        answer[N++] = ch;
        if (N >= capacity)
        {
            temp = realloc(answer, capacity + capacity / 2);
            if (!temp)
                goto out_of_memory;
            answer = temp;
            capacity = capacity + capacity / 2;
        }
    }
    *len = N;
    fclose(fp);
    return answer;
out_of_memory:
    fclose(fp);
    *len = -1;
    free(answer);
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
       if (ch > 127)
       {
           answer = 1;
           break;
       }
    }
    fclose(fp);
    return answer;
}

/*
    is a file a regular file ?
 */
static int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/*
  is a file a directory ?
 */
static int is_directory(const char *path)
{
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

/*
   Escape a string to write as XML
 */
static char *xml_escape(const char *data)
{
    int i;
    int size = 0;
    char * answer;
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
 The functions
 
 unsigned char *uudecodestr(const char *uucode, int *N)
 char *uuencodestr(const unsigned char *binary, int N)
 
 Have been modifed by Malcolm McLean from the functions
 
 void UUEncode( char * pSrcFilename, FILE * pSrcFile, FILE * pDstFile )
 bool UUDecode( FILE * pSrcFile )
 
 by Tom Weatherhead.
 
 They appear in Tom's github project "helix".
 
The MIT License (MIT)

Copyright (c) 2002-2021 Tom Weatherhead

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 
 */
/*
    decode a uuencoded string to binary
 */
unsigned char *uudecodestr(const char *uucode, int *N)
{
    int Nmaxout;
    unsigned char *out;
    int ix = 0;
    int jx = 0;
    int k;
    int i;
    
    Nmaxout = ((int)strlen(uucode)/60 +1) * 45;
    out = malloc(Nmaxout);
    
    while( uucode[ix] )
    {
        // Get a line of the src text file.
        char acSrcData[128];
        
        k = 0;
        while (uucode[ix] != '\n' && uucode[ix])
        {
            acSrcData[k++] = uucode[ix++];
            if (k > 126)
                goto error_exit;
        }
        acSrcData[k++] = 0;
        if (uucode[ix])
            ix++;
        if( strlen( acSrcData ) == 0 )
        {
            // continue;
        }
        else if( acSrcData[0] > 32  &&  acSrcData[0] <= 77 )
        {
            int knBytesToWrite = (int)( acSrcData[0] - 32 );
            unsigned char acDstData[45];
            int nDstIndex = 0;

            for( int i = 1; i < strlen( acSrcData ); i += 4 )
            {
                int j;
                int anSrc1[4];

                for( j = 0; j < 4; ++j )
                {
                    const char c = acSrcData[i + j];

                    if( c < 32  ||  c > 96 )
                    {
                        break;
                    }

                    anSrc1[j] = (int)( ( c - 32 ) & 63);
                }

                for( ; j < 4; ++j )
                {
                    anSrc1[j] = 0;
                }

                int k = 0;

                for( j = 0; j < 4; ++j )
                {
                    k |= ( anSrc1[j] << ( j * 6 ) );
                }

                for( j = 0; j < 3; ++j )
                {
                    acDstData[nDstIndex++] = (unsigned char)( k >> ( j * 8 ) );
                }
            }

            for (i = 0; i < knBytesToWrite; i++)
                out[jx++] = acDstData[i];
        }
    }
    
    if (N)
        *N = jx;

    return out;
    
error_exit:
    free(out);
    if (N)
        *N = -1;
    return 0;
}

/*
    encode binary to text using uuencoding.
 */
char *uuencodestr(const unsigned char *binary, int N)
{
    char acTable[64];
    int i;
    int j = 0;
    char *out = 0;
    int Nleft;
    int Nout;
    
    acTable[0] = '`';    // We could use a space instead.

    for( i = 1; i < 64; ++i )
    {
        acTable[i] = (char)( i + 32 );
    }
    
    Nout = (N/45 + 1) * (15 * 4 + 1 + 1);
    out = malloc(Nout + 1);
    if (!out)
        goto out_of_memory;
    
    Nleft = N;
    while( Nleft > 0 )
    {
        unsigned char aucSrcData[45];
        int knBytesRead = Nleft > 45 ? 45 : Nleft;

        memcpy(aucSrcData, binary + N - Nleft, knBytesRead);
        
        out[j++] = acTable[knBytesRead];

        for( i = 0; i < knBytesRead; i += 3 )
        {
            unsigned long k = (unsigned int)aucSrcData[i]
+ ( (unsigned long)aucSrcData[i + 1] << 8 ) + ( (unsigned long)aucSrcData[i
+ 2] << 16 );

            out[j++] = acTable[k & 63];
            out[j++] = acTable[(k >> 6) & 63];
            out[j++] = acTable[(k >> 12) & 63];
            out[j++] = acTable[k >> 18];
        }
        out[j++] = '\n';
        Nleft -= knBytesRead;
    }
    out[j++] = 0;
    
    return out;
    
out_of_memory:
    return 0;
}


int writetextfile(FILE *fpout, const char *fname)
{
    FILE *fp = 0;
    char *text = 0;
    char *xmltext = 0;
    int i;

    fp = fopen(fname, "r");
    text = fslurp(fp);
    if (!text)
        goto out_of_memory;
    fclose(fp);
    fp = 0;
    
    xmltext = xml_escape(text);
    if (!xmltext)
        goto out_of_memory;
    for (i=0; xmltext[i];i++)
        fputc(xmltext[i], fpout);
    
    free(text);
    free(xmltext);
    
    return 0;
    
out_of_memory:
    fclose(fp);
    free(text);
    free(xmltext);
    
    return 0;
}

int writebinaryfile(FILE *fpout, const char *fname)
{
    int N;
    unsigned char *binary = 0;
    char *uucode = 0;
    int i;
    
    binary = slurpb(fname, &N);
    if (!binary)
        goto out_of_memory;
    uucode = uuencodestr(binary,N);
    if (!uucode)
        goto out_of_memory;
    fprintf(fpout, "<![CDATA[");
    for (i = 0; uucode[i];i++)
        fputc(uucode[i], fpout);
    fprintf(fpout, "]]>");
    free(binary);
    free(uucode);
    
    return 0;
out_of_memory:
    free(binary);
    free(uucode);
    return -1;
}

void processregularfile(const char *path, int depth)
{
    int error;
    char *filename = 0;
    char *xmlfilename = 0;
    int i;
    
    filename = getfilename(path);
    if (!filename)
        goto out_of_memory;
    xmlfilename = xml_escape(filename);
    if (!xmlfilename)
        goto out_of_memory;
    
    
    if (!is_binary(path))
    {
        for (i = 0; i <depth; i++)
            printf("\t");
        printf("<file name=\"%s\" type=\"text\">\n", xmlfilename);
        writetextfile(stdout, path);
        printf("\n");
        for (i= 0; i <depth;i++)
            printf("\t");
        printf("</file>\n");
    }
    else
    {
        for (i = 0; i <depth; i++)
            printf("\t");
        printf("<file name=\"%s\" type=\"binary\">\n", xmlfilename);
        writebinaryfile(stdout, path);
        printf("\n");
        for (i= 0; i <depth;i++)
            printf("\t");
        printf("</file>\n");
    }
    free(filename);
    free(xmlfilename);
    return;
    
out_of_memory:
    free(filename);
    free(xmlfilename);
}

void processdirectory_r(const char *path, int depth)
{
    DIR *dirp;
    struct dirent *dp;
    char *pathslash;
    char *filepath;
    char *xmlfilename;
    int i;

    pathslash = mystrconcat(path, "/");
    if (!pathslash)
        goto out_of_memory;
    
    if ((dirp = opendir(path)) == NULL) {
        perror("couldn't open directory");
        return;
    }


    do {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL) {
          filepath = mystrconcat(pathslash, dp->d_name);
          if (is_directory(filepath) && dp->d_name[0] != '.')
          {
              xmlfilename = xml_escape(dp->d_name);
              if (!xmlfilename)
                  goto out_of_memory;
              for (i = 0; i < depth; i++)
                      printf("\t");
              printf("<directory name=\"%s\">\n", xmlfilename);
              processdirectory_r(filepath, depth +1);
              for (i = 0; i < depth; i++)
                      printf("\t");
              printf("</directory>\n");
              free(xmlfilename);
              xmlfilename = 0;
          }
          else if (dp->d_name[0] != '.' && is_regular_file(filepath))
                processregularfile(filepath, depth);
          free(filepath);
        }
    } while (dp != NULL);

    if (errno != 0)
        perror("error reading directory");

    free(pathslash);
    closedir(dirp);
    return;
out_of_memory:
    free(pathslash);
    closedir(dirp);
}

int directorytoxml(const char *directory)
{
    char *filename= 0 ;
    char *xmlfilename = 0;
    
    if (!is_directory(directory))
    {
        fprintf(stderr, "Can't open directory %s\n", directory);
        return -1;
    }
    
    filename = getfilename(directory);
    xmlfilename = xml_escape(filename);
    
    printf("<FileSystem>\n");
    printf("\t<directory name=\"%s\">\n", xmlfilename);
    processdirectory_r(directory, 2);
    printf("\t</directory>\n");
    printf("</FileSystem>\n");
    
    return 0;
}

void usage(void)
{
    fprintf(stderr, "directorytoxml: converts a directory to an xml file\n");
    fprintf(stderr, "Usage: directortoxml <directory>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "by Malcolm McLean\n");
    fprintf(stderr, "For use with the program directory to query the XML for files\n");
}

int main(int argc, char **argv)
{
    int error;
    
    if (argc == 1)
        error = directorytoxml(".");
    else if (argc == 2)
        error = directorytoxml(argv[1]);
    else
        error = -1;
    
    if (error)
        usage();
    return 0;
}
