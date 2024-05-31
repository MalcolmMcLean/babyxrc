#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "xmlparser2.h"

#define BBX_FS_STDIO 1
#define BBX_FS_STRING 2

typedef struct bbx_filesystem
{
    int mode;
    FILE *openfiles[32];
    int Nopenfiles;
    char *filepath;
    XMLDOC *filesystemdoc;
} BBX_FileSystem;

#define bbx_malloc malloc

static char *bbx_strdup(const char *str)
{
    char *answer = bbx_malloc(strlen(str) +1);
    strcpy(answer, str);
    
    return answer;
}

static const char *getfilesystemname_r(XMLNODE *node);
static char *makepath(const char *base, const char *query);
static FILE *xml_fopen(XMLDOC *doc, const char *path, const char *mode);

BBX_FileSystem *bbx_filesystem(void)
{
    BBX_FileSystem *bbx_fs;
    
    bbx_fs = bbx_malloc(sizeof(BBX_FileSystem));
    
    bbx_fs->mode = 0;
    bbx_fs->openfiles[0] = 0;
    bbx_fs->Nopenfiles = 0;
    bbx_fs->filepath = 0;;
    XMLDOC *filesystemdoc = 0;
    
    return bbx_fs;
}

void bbx_filesystem_kill(BBX_FileSystem *bbx_fs)
{
    if (bbx_fs)
    {
        if (bbx_fs->Nopenfiles)
            fprintf(stderr, "warning, exiting with open files\n");
        killxmldoc(bbx_fs->filesystemdoc);
        free(bbx_fs->filepath);
        
        free(bbx_fs);
    }
}


int bbx_filesystem_set(BBX_FileSystem *bbx_fs, const char *pathorxml, int mode)
{
  char error[1024];
    
  if (bbx_fs->mode != 0)
  {
      fprintf(stderr, "Baby X file system already initialised\n");
      return -1;
  }

  if (mode == BBX_FS_STDIO)
  {
      bbx_fs->filepath = bbx_strdup(pathorxml);
      bbx_fs->mode = mode;
  }
  else if (mode == BBX_FS_STRING)
  {
   
      bbx_fs->filesystemdoc = xmldocfromstring(pathorxml, error, 1024);
      if (!bbx_fs->filesystemdoc)
      {
          fprintf(stderr, "%s\n", error);
          return -1;
      }
      bbx_fs->mode = mode;
  }
  else
  {
      fprintf(stderr, "Initialising Baby X file system in unsupported mode\n");
      return -1;
  }
    
  return 0;
}

FILE *bbx_filesystem_fopen(BBX_FileSystem *bbx_fs, const char *path, const char *mode)
{
  FILE *fp = 0;
  char *stdpath = 0;
    
  if (bbx_fs->Nopenfiles > 30)
  {
      fprintf(stderr, "Baby X file system, too many open files\n");
      return 0;
  }

  if (strcmp(mode, "r"))
  {
      fprintf(stderr, "Baby X files system, fopen only support for reading \"r\" mode");
      return 0;
  }

  if (bbx_fs->mode == BBX_FS_STDIO)
  {
     stdpath = makepath(bbx_fs->filepath, path);
     fp = fopen(stdpath, mode);
     free(stdpath);
    
     if (fp)
         bbx_fs->openfiles[bbx_fs->Nopenfiles++] = fp;

     return fp;
  }
  else if (bbx_fs->mode == BBX_FS_STRING)
  {
      fp = xml_fopen(bbx_fs->filesystemdoc, path, mode);
      
       if (fp)
         bbx_fs->openfiles[bbx_fs->Nopenfiles++] = fp;

       return fp;
  }
  else
  {
      fprintf(stderr, "BabyX file system not intialised correctly\n");
      return 0;
  }
} 

int bbx_filesystem_fclose(BBX_FileSystem *bbx_fs, FILE *fp)
{
   int i, j;

   if (bbx_fs->mode == BBX_FS_STDIO || bbx_fs->mode == BBX_FS_STRING)
   {
      if (fp == NULL)
         return 0;

      for (i = 0; i < bbx_fs->Nopenfiles; i++)
      {
         if (bbx_fs->openfiles[i] == fp)
         {
            for (j = i + 1; j < bbx_fs->Nopenfiles + 1; j++)
              bbx_fs->openfiles[j-1] = bbx_fs->openfiles[j];
            bbx_fs->Nopenfiles--;
            return fclose (fp);
         }
      }
      fprintf(stderr, "bbx_filesystem_fclose, failed to close file\n");
      assert(0);
      return -1;  
   }
     
   fprintf(stderr, "bbx_filessytem_fclose, file system not initialised\n");
   assert(0);
   return -1;
}

const char *bbx_filesystem_getname(BBX_FileSystem *bbx_fs)
{
    const char *answer = "";
    
    if (bbx_fs->mode == BBX_FS_STDIO)
    {
        answer = strrchr(bbx_fs->filepath, '/');
        if (answer)
            answer = answer + 1;
        else
            answer = bbx_fs->filepath;
        
    }
    else
    {
        answer = getfilesystemname_r(xml_getroot(bbx_fs->filesystemdoc));
    }
    
    return answer;
}

static const char *getfilesystemname_r(XMLNODE *node)
{
    const char *answer = 0;
    XMLNODE *child;
    
    while (node)
    {
        if (!strcmp(xml_gettag(node), "FileSystem"))
        {
            for (child = node->child; child != NULL; child = child->next)
            {
                if (!strcmp(xml_gettag(child), "directory"))
                {
                    if (xml_getattribute(child, "name"))
                    {
                        answer = xml_getattribute(child, "name");
                        break;
                    }
                }
            }
        }
        if (answer == 0)
            answer = getfilesystemname_r(node->child);
        else
            break;
        node = node->next;
    }
    
    return answer;
}
/*
    The external directory mounted should be of the form
         "/users/fred/babyxdevelopment/mydir"
     The query should be of the form
         "/mydir/readme.txt"
 */
static char *makepath(const char *base, const char *query)
{
    const char *local;
    char *answer;
    int i;
    
    local = strrchr(base, '/');
    if (!local)
        local = base;
    
    if (strncmp(local, query, strlen(local)))
    {
        fprintf(stderr, "Baby X files system, bad path, mounted %s query %s\n",
                local, query);
        return 0;
    }
    if (query[strlen(local)] != '/')
    {
        fprintf(stderr, "Baby X files system, bad path, mounted %s query %s\n",
                local, query);
        return 0;
    }
    
    answer = bbx_malloc(strlen(base) + strlen(query) + 1);
    for (i = 0; base + i != local; i++)
    {
        answer[i] = base[i];
    }
    strcpy(answer + i, query);
    
    return answer;
}

/*
    This program is designed to show off the capabilities of the XML Parser.
 
     We generate an XML file which represents a filesystem. Then we query it for files.
     Unfortunatley there is no way to traverse a physical computer's filesystem with
     complete portablity, and so the encoder, directorytoxml, will not run everywhere.
     But the decoder is completetely portable. And you can incorporate it into your own
     programs to have embeded files. Use the Baby X resource compiler to convert the XML
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
                    <file name="Tyger" tyoe="text">
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

/*
   Does a string cnsist entirely of white space? (also treat nulls as white)
 */
static int strwhitespace(const char *str)
{
    if (!str)
        return 1;
    while (*str)
    {
        if (!isspace((unsigned char) *str))
            return  0;
        str++;
    }
    
    return 1;
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

static unsigned char *uudecodestr(const char *uucode, int *N)
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


static char *directoryname(const char *path, int pos)
{
    int i;
    int j = 0;
    int len = 0;
    char *answer = 0;
    
    for(i = pos + 1; path[i]; i++)
        if(path[i] == '/')
            break;
    len = i - pos - 1;
    answer = malloc(len + 1);
    if (!answer)
        goto out_of_memory;
    for(i = pos + 1; path[i]; i++)
    {
        if(path[i] == '/')
            break;
        answer[j++] = path[i];
    }
    answer[j++] = 0;
    return answer;
out_of_memory:
    return 0;
}

static FILE *file_fopen(XMLNODE *node)
{
    FILE *fp = 0;
    int len;
    const char *data;
    char *last;
    int trailing = 0;
    unsigned char *plain = 0;
    int Nplain;
    const char *type;
    
    type = xml_getattribute(node, "type");
    if (!type)
        goto error_exit;
    fp = tmpfile();
    if (!fp)
        goto error_exit;
    data = xml_getdata(node);
    len = (int) strlen(data);
    last = strrchr(data, '\n');
    if (last && strwhitespace(last))
        trailing = len - (int)(last - data);
    if (len - trailing < 1)
        goto error_exit;
    if (!strcmp(type, "text"))
    {
        if (fwrite(data +1, 1, len - trailing - 1, fp) != len - trailing - 1)
            goto error_exit;
    }
    else if (!strcmp(type, "binary"))
    {
        plain = uudecodestr(data, &Nplain);
        if (!plain)
            goto error_exit;
        if (fwrite(plain, 1, Nplain, fp) != Nplain)
            goto error_exit;
        free(plain);
        plain = 0;
    }
    fseek(fp, 0, SEEK_SET);
    return fp;
    
error_exit:
    free(plain);
    fclose(fp);
    return 0;
}

static FILE *directory_fopen_r(XMLNODE *node, const char *path, int pos)
{
    FILE *answer = 0;
    const char *nodename;
    char *name = 0;
    XMLNODE *child;
    int lastdir;
    
    name = directoryname(path, pos);
    while (node)
    {
        if (!strcmp(xml_gettag(node), "directory"))
        {
            nodename = xml_getattribute(node, "name");
            if (nodename && !strcmp(name, nodename))
            {
                answer = directory_fopen_r(node->child, path, pos + (int) strlen(name) + 1);
            }
        }
        else if(!strcmp(xml_gettag(node), "file"))
        {
            nodename = xml_getattribute(node, "name");
            if (nodename && !strcmp(name, nodename))
            {
                answer = file_fopen(node);
                break;
            }
        }
        
        node = node->next;
    }
    
    free(name);
    return  answer;
}

static FILE *xml_fopen_r(XMLNODE *node, const char *path)
{
    FILE *answer = 0;
    while (node)
    {
        if (!strcmp(xml_gettag(node), "FileSystem"))
        {
            answer = directory_fopen_r(node->child, path, 0);
            if (answer)
                break;
        }
        if (node->child)
        {
            answer = xml_fopen_r(node->child, path);
            if (answer)
                break;
        }
        node = node->next;
    }
    
    return answer;
}

/*
   Query an XML document for files under FileSystem tag.
 
   You will need to take this function if using the FileSystem to
     embed files into your own programs.
 
 */
static FILE *xml_fopen(XMLDOC *doc, const char *path, const char *mode)
{
    return xml_fopen_r(xml_getroot(doc), path);
}

