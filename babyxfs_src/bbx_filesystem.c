#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "xmlparser2.h"
#include "bbx_write_source_archive.h"

#define BBX_FS_STDIO 1
#define BBX_FS_STRING 2

typedef struct bbx_filesystem
{
    int mode;
    FILE *openfiles[FOPEN_MAX+1];
    char filemodes[FOPEN_MAX+1];
    char *paths[FOPEN_MAX + 1];
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
int babyxfs_cp(XMLNODE *root, const char *path, const unsigned char *data, int N);

static char **listdirectory(XMLNODE *node);

static const char *getfilesystemname_r(XMLNODE *node);
static char *makepath(const char *base, const char *query);
static unsigned char *fslurpb(FILE *fp, int *len);
static FILE *xml_fopen(XMLDOC *doc, const char *path, const char *mode);
static XMLNODE *findnodebypath(XMLNODE *node, const char *path, int pos);
static XMLNODE *bbx_fs_getfilesystemroot(XMLNODE *root);

BBX_FileSystem *bbx_filesystem(void)
{
    BBX_FileSystem *bbx_fs;
    int i;

    bbx_fs = bbx_malloc(sizeof(BBX_FileSystem));
    
    bbx_fs->mode = 0;
    for (i = 0; i < FOPEN_MAX + 1; i++)
    {
        bbx_fs->openfiles[i] = 0;
        bbx_fs->filemodes[i] = 0;
        bbx_fs->paths[i] = 0;
    }
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
    
  if (bbx_fs->Nopenfiles >= FOPEN_MAX)
  {
      fprintf(stderr, "Baby X file system, too many open files\n");
      return 0;
  }

  if (mode == NULL || (mode[0] != 'r' && mode[0] != 'w'))
  {
      fprintf(stderr, "Baby X files system, fopen only support for reading \"r\"\n");
      fprintf(stderr, "and writing \"w\" mode");
      return 0;
  }

  if (bbx_fs->mode == BBX_FS_STDIO)
  {
     stdpath = makepath(bbx_fs->filepath, path);
     fp = fopen(stdpath, mode);
     free(stdpath);
    
     if (fp)
     {
         bbx_fs->openfiles[bbx_fs->Nopenfiles] = fp;
         bbx_fs->filemodes[bbx_fs->Nopenfiles] = mode[0];
         bbx_fs->paths[bbx_fs->Nopenfiles] = bbx_strdup(path);
         bbx_fs->Nopenfiles++;
     }

     return fp;
  }
  else if (bbx_fs->mode == BBX_FS_STRING)
  {
      fp = xml_fopen(bbx_fs->filesystemdoc, path, mode);
      
      printf("Here path %s %p\n", path, fp);
      
      if (fp)
      {
          bbx_fs->openfiles[bbx_fs->Nopenfiles] = fp;
          bbx_fs->filemodes[bbx_fs->Nopenfiles] = mode[0];
          bbx_fs->paths[bbx_fs->Nopenfiles] = bbx_strdup(path);
          bbx_fs->Nopenfiles++;
      }

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
            if (bbx_fs->filemodes[i] == 'w')
            {
                if (bbx_fs->mode == BBX_FS_STRING)
                {
                    XMLNODE *node = 0;
                    XMLNODE *root;
                    unsigned char *data;
                    int N;
                    const char *datatype = 0;
                    
                    root = bbx_fs_getfilesystemroot(xml_getroot(bbx_fs->filesystemdoc));
                    
                    fseek(fp, 0, SEEK_SET);
                    data = fslurpb(fp, &N);
                    
                    //node = findnodebypath(root, bbx_fs->paths[i], 0);
                    //if (nide)
                    //datatype = xml_getattribute(node, "type");
                    //bbx_write_source_archive_write_to_file_node(node, data, N, datatype);
                    
                    babyxfs_cp(root, bbx_fs->paths[i], data, N);
                    
                    free(data);

                    
                }
            }
             
             free(bbx_fs->paths[i]);
            for (j = i + 1; j < bbx_fs->Nopenfiles + 1; j++)
            {
                bbx_fs->openfiles[j-1] = bbx_fs->openfiles[j];
                bbx_fs->filemodes[j-1] = bbx_fs->filemodes[j];
                bbx_fs->paths[j-1] = bbx_fs->paths[j];
            }
             bbx_fs->openfiles[j-1] = 0;
             bbx_fs->filemodes[j-1] = 0;
             bbx_fs->paths[j-1] = 0;
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

unsigned char *bbx_filesystem_slurp(BBX_FileSystem *bbx_fs, const char *path, const char *mode, int *N)
{
    FILE *fp;
    unsigned char *answer = 0;
    int dummy = 0;
    
    if (N)
        *N = 0;
    else
        N = &dummy;
    
    fp = bbx_filesystem_fopen(bbx_fs, path, mode);
    if (!fp)
        return 0;
    answer = fslurpb(fp, N);
    if (answer && *N >= 0)
        answer[*N] = 0;
    bbx_filesystem_fclose(bbx_fs, fp);
    
    return answer;
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

char **bbx_filesystem_list(BBX_FileSystem *bbx_fs, const char *path)
{
    if (bbx_fs->mode == BBX_FS_STDIO)
        return 0;
    if (bbx_fs->mode == BBX_FS_STRING)
    {
        XMLNODE *node = 0;
        XMLNODE *root;
        char *trimmedpath = 0;
        char **answer = 0;
        
        if (!path)
            return 0;
        
        trimmedpath = bbx_strdup(path);
        if (strrchr(trimmedpath, '/') == trimmedpath + strlen(trimmedpath) - 1)
            *strrchr(trimmedpath, '/') = 0;
    
        root = bbx_fs_getfilesystemroot(xml_getroot(bbx_fs->filesystemdoc));
        if (!strcmp(path, "/"))
            node = root;
        else
             node = findnodebypath(root, trimmedpath, 0);
        if (node)
            answer = listdirectory(node);
        
        free(trimmedpath);
        return answer;;
    }
    
    return 0;
}

static char **listdirectory(XMLNODE *node)
{
    int N = 0;
    int i = 0;
    XMLNODE *child;
    char **answer = 0;
    const char *name;
    char *str;
    
    if (strcmp(xml_gettag(node), "directory") && strcmp(xml_gettag(node), "FileSystem"))
        return 0;
    child = node->child;
    while (child)
    {
        if (!strcmp(xml_gettag(child), "directory"))
            N++;
        else if (!strcmp(xml_gettag(child), "file"))
            N++;
        child = child->next;
    }
    
    answer = bbx_malloc( (N + 1) * sizeof(char *));
    child = node->child;
    i = 0;
    while (child)
    {
        if (!strcmp(xml_gettag(child), "directory"))
        {
            name = xml_getattribute(child, "name");
            if (!name)
                name = "?";
            answer[i] = bbx_malloc(strlen(name) + 2);
            strcpy(answer[i], name);
            strcat(answer[i], "/");
            i++;
        }
        else if (!strcmp(xml_gettag(child), "file"))
        {
            name = xml_getattribute(child, "name");
            if (!name)
                name = "?";
            answer[i] = bbx_malloc(strlen(name) + 2);
            strcpy(answer[i], name);
            i++;
        }
        child = child->next;
    }
    
    answer[i] = 0;
    
    return answer;
    
}

/*
   THec FileSystem node should have one child, which is a directory with the name of the directory passed to directorytoxml
*/
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
            break;
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

/*
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
    if (!data)
        return fp;
    len = (int) strlen(data);
    last = strrchr(data, '\n');
    if (last && strwhitespace(last))
        trailing = len - (int)(last - data);
    if (len - trailing < 1)
        goto error_exit;
    if (!strcmp(type, "text"))
    {
        if (fwrite(data + 1, 1, len - trailing - 1, fp) != len - trailing - 1)
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
*/
static FILE *file_fopen(XMLNODE *node)
{
    FILE *fp = 0;
    int len;
    const char *data;
    int trailing = 0;
    int leading = 0;
    unsigned char *plain = 0;
    int Nplain;
    const char *type;
    int i;
    
    type = xml_getattribute(node, "type");
    if (!type)
        goto error_exit;
    fp = tmpfile();
    if (!fp)
        goto error_exit;
    data = xml_getdata(node);
    if (!data)
        return fp;
    
    if (!strcmp(type, "text"))
    {
        leading = 0;
        len = (int) strlen(data);
        for (i = 0; data[i]; i++)
            if (!isspace((unsigned char) data[i]) || data[i] == '\n')
                break;
        if (data[i] == '\n')
            leading = i + 1;
        
        trailing = 0;
        i = len - 1;
        for (i = len - 1; i > 0; i--)
            if (!isspace((unsigned char) data[i]) || data[i] == '\n')
                break;
        if (i > 0 && data[i] == '\n')
            trailing = len - i;
        
        if (trailing + leading >= len )
            ;
        else
        {
            if (fwrite(data + leading, 1, len - trailing - leading, fp) != len - trailing - leading)
                goto error_exit;
        }
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

/*
  Find a node from a path.
 
  pos indicates the position along the path to start from.
 */
static XMLNODE *findnodebypath(XMLNODE *node, const char *path, int pos)
{
    XMLNODE *answer = 0;
    const char *name = 0;
    int i;
    
   while (node)
   {
       if (!strcmp(xml_gettag(node), "file"))
       {
           name = xml_getattribute(node, "name");
           if (!name)
               goto nextnode;
           if (!strcmp(name, path + pos))
               return node;
       }
       else if (!strcmp(xml_gettag(node), "directory"))
       {
           name = xml_getattribute(node, "name");
           if (!name)
               continue;
           for (i = 0; name[i] && path[pos+i]; i++)
           {
               if (name[i] != path[pos+i])
               {
                   goto nextnode;
               }
           }
           
           if (name[i] == 0 && path[pos + i] == 0)
               return node;
           if (name[i] == 0 && path[pos + i] == '/')
               return findnodebypath(node->child, path, pos + i + 1);
       }
       else if (!strcmp(xml_gettag(node), "FileSystem"))
       {
           if (path[pos] != '/')
               return 0;
           node = node->child;
           while (node)
           {
               answer = findnodebypath(node, path, pos + 1);
               if (answer)
                   return answer;
               node = node->next;
           }
           return 0;
       }
       
   nextnode:
       node = node->next;
   }
    
    return  0;
}

/*
  Get the node with the tag "FileSystem"
 */
static XMLNODE *bbx_fs_getfilesystemroot(XMLNODE *root)
{
    XMLNODE *answer;
    
    answer = root;
    while (root)
    {
        if (!strcmp(xml_gettag(root), "FileSystem"))
            return root;
        root = root->next;
    }
    
    root = answer;
    while (root)
    {
        answer = bbx_fs_getfilesystemroot(root->child);
        if (answer)
            return answer;
        root = root->next;
    }
    
    return 0;
}

const char *basename(const char *path)
{
    const char *answer = 0;
    answer = strrchr(path, '/');
    if (answer)
        answer = answer + 1;
    else
        answer = path;
    
    return answer;
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

/*
  Find a node from a path.
 
  pos indicates the position along the path to start from.
 */
XMLNODE *createnodebypath(XMLNODE *node, const char *path, int pos)
{
    XMLNODE *answer = 0;
    const char *name = 0;
    int i;
    
   while (node)
   {
       if (!strcmp(xml_gettag(node), "file"))
       {
           name = xml_getattribute(node, "name");
           if (!name)
               goto nextnode;
           if (!strcmp(name, path + pos))
               return node;
       }
       else if (!strcmp(xml_gettag(node), "directory"))
       {
           name = xml_getattribute(node, "name");
           if (!name)
               continue;
           for (i = 0; name[i] && path[pos+i]; i++)
           {
               if (name[i] != path[pos+i])
               {
                   goto nextnode;
               }
           }
           
           if (name[i] == 0 && (path[pos + i] == 0 || path[pos + i] == '/'))
           {
               XMLNODE *newnode = 0;
               
               if (name[i] == 0 && path[pos + i] == '/')
               {
                   newnode = createnodebypath(node->child, path, pos + i + 1);
                   if (newnode)
                       return newnode;
                   else if(strchr(path + pos + i + 1, '/'))
                       return 0;
               }
            
                   
                newnode = bbx_malloc (sizeof(XMLNODE));
                newnode->tag = bbx_strdup("newnode");                 /* tag to identify data type */
                newnode->attributes = 0;  /* attributes */
                newnode->data = bbx_strdup("\n\tttt\n");                /* data as ascii */
                newnode->position = 0;              /* position of the node within parent's data string */
                newnode->lineno = -1;                /* line number of node in document */
                newnode->next = 0;      /* sibling node */
                newnode->child = 0;     /* first child node */
                   
                newnode->next = node->child;
                node->child = newnode;
            
               return newnode;
           }
       }
       else if (!strcmp(xml_gettag(node), "FileSystem"))
       {
           if (path[pos] != '/')
               return 0;
           node = node->child;
           while (node)
           {
               answer = createnodebypath(node, path, pos + 1);
               if (answer)
                   return answer;
               node = node->next;
           }
           return  0;
       }
       
   nextnode:
       node = node->next;
   }
    
    return  0;
}



/*
 */
int babyxfs_cp(XMLNODE *root, const char *path, const unsigned char *data, int N)
{
    XMLNODE *node;
    XMLATTRIBUTE *attr;
    const char *filename = 0;
    const char *datatype = 0;

    int i;
   
    filename = basename(path);
    datatype = isbinary(data, N) ? "binary" : "text";
    node = findnodebypath(root, path, 0);
    if (!node)
        node = createnodebypath(root, path, 0);
    if (node)
    {
        if (!strcmp(xml_gettag(node), "newnode"))
        {
            XMLATTRIBUTE *nameattr;
            XMLATTRIBUTE *typeattr;
            
            nameattr = bbx_malloc(sizeof(XMLATTRIBUTE));
            nameattr->name = bbx_strdup("name");
            nameattr->value = bbx_strdup(filename);
            nameattr->next = 0;
            
            typeattr = bbx_malloc(sizeof(XMLATTRIBUTE));
            typeattr->name = bbx_strdup("type");
            typeattr->value = bbx_strdup(datatype);
            typeattr->next = 0;
            
            nameattr->next = typeattr;
            
            node->attributes = nameattr;
            
            free (node->tag);
            node->tag = bbx_strdup("file");
        }
        
        if (!strcmp(xml_gettag(node), "file"))
        {
            if (!xml_getattribute(node, "type"))
            {
                XMLATTRIBUTE *typeattr;
                
                typeattr = bbx_malloc(sizeof(XMLATTRIBUTE));
                typeattr->name = bbx_strdup("type");
                typeattr->value = bbx_strdup(datatype);
                typeattr->next = 0;
                
                typeattr->next = node->attributes;
                node->attributes = typeattr;
                
            }
            if (strcmp(datatype, xml_getattribute(node, "type")))
            {
                attr = node->attributes;
                
                while (attr)
                {
                    if (!strcmp(attr->name, "type"))
                    {
                        free(attr->value);
                        attr->value = bbx_strdup(datatype);
                    }
                    attr = attr->next;
                }
            }
            if (!strcmp(datatype, "binary"))
            {
                bbx_write_source_archive_write_to_file_node(node, data, N, "binary");
            }
            else if (!strcmp(datatype, "text"))
            {
                bbx_write_source_archive_write_to_file_node(node, data, N, "text");
            }
        }
      
    }

    return 0;
}
