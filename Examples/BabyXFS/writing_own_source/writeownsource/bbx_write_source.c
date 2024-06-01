//
//  bbx_write_source.c
//  testbabyxfilesystem
//
//  Created by Malcolm McLean on 31/05/2024.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "bbx_write_source.h"
#include "asciitostring.h"
#include "xmlparser2.h"

/*
   Does a string consist entirely of white space? (also treat nulls as white)
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
    Concatenate two strings, returning an allocated result.
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

int makedirectory(const char *path)
{
    int status;

    //status = mkdir("/home/cnd/mod1", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    status = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    return status;
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



int bbx_write_source_r(XMLNODE *node, const char *source_xml, const char *path, const char *source_xml_file, const char *source_xml_name)
{
    char *pathslash = 0;
    char *filename = 0;
    const char *name;
    FILE *fpout = 0;
    FILE *fpin = 0;
    int ch;
    int err = 0;
    
    pathslash = mystrconcat(path, "/");
    while (node)
    {
        if (!strcmp(xml_gettag(node), "file"))
        {
            name = xml_getattribute(node, "name");
            if (!name)
                goto skip_node;
            filename = mystrconcat(pathslash, name);
            if (!filename)
                goto out_of_memory;
            fpout = fopen(filename, "w");
            if (!fpout)
                break;
            fpin = file_fopen(node);
            if (!fpin)
                break;
            if (!strcmp(name, source_xml_file))
            {
                char *escaped = texttostring(source_xml);
                if (!escaped)
                    break;
                fprintf(fpout, "char %s[] = %s;\n", source_xml_name, escaped);
                free(escaped);
            }
            else
            {
               while ((ch = fgetc(fpin)) != EOF)
                   err |= (fputc(ch, fpout) == EOF) ? -1 : 0;
            }
            fclose(fpout);
            fclose(fpin);
            fpout = 0;
            fpin = 0;
            free(filename);
            filename = 0;
        }
        else if (!strcmp(xml_gettag(node), "directory"))
        {
            name = xml_getattribute(node, "name");
            if (!name)
                goto skip_node;
            filename = mystrconcat(pathslash, name);
            if (!filename)
                goto out_of_memory;
            makedirectory(filename);
            err |= bbx_write_source_r(node->child, source_xml, filename, source_xml_file, source_xml_name);
            free (filename);
            filename = 0;
        }
        
    skip_node:
        node = node->next;
    }
    
    free (pathslash);
    if (fpin || fpout)
    {
        fclose(fpin);
        fclose(fpout);
        err = -2;
    }
    
    return err;
    
out_of_memory:
    free (pathslash);
    fclose(fpin);
    fclose(fpout);
        
    return -1;
}

int bbx_write_source(const char *source_xml, const char *path, const char *source_xml_file, const char *source_xml_name)
{
    XMLDOC *doc = 0;
    char error[1024];
    XMLNODE *root;
    int answer = 0;
    
    doc = xmldocfromstring(source_xml, error, 1024);
    if (!doc)
    {
        fprintf(stderr, "%s\n", error);
        return -1;
    }
    root = xml_getroot(doc);
    if (strcmp(xml_gettag(root), "FileSystem"))
    {
        killxmldoc(doc);
        return -1;
    }
    
    answer = bbx_write_source_r(root->child, source_xml, path, source_xml_file, source_xml_name);
    
    killxmldoc(doc);
    
    return  answer;
}
