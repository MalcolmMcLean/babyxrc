//
//  directory.c
//  directorytoxml
//
//  Created by Malcolm McLean on 28/05/2024.
//
#include "xmlparser2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
int strwhitespace(const char *str)
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


char *directoryname(const char *path, int pos)
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

FILE *file_fopen(XMLNODE *node)
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

FILE *directory_fopen_r(XMLNODE *node, const char *path, int pos)
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

FILE *xml_fopen_r(XMLNODE *node, const char *path)
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
FILE *xml_fopen(XMLDOC *doc, const char *path, const char *mode)
{
    return xml_fopen_r(xml_getroot(doc), path);
}

void usage()
{
    fprintf(stderr, "directory: query an FileSystem XML file for a file\n");
    fprintf(stderr, "Usage: directory <filesystem.xml> <pathtofile>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "For example, directory poemfiles.xml /poems/Blake/Tyger\n");
    fprintf(stderr, "The XML files poemfiles.xml is FileSystem file which\n");
    fprintf(stderr, "contains poems. The command will extract the poem \"Tye=ger\"\n");
    fprintf(stderr, "by Blake\n");
    fprintf(stderr, "Generate the FileSystem files with the program directorytoxml\n");
}

int main(int argc, char **argv)
{
    XMLDOC *doc = 0;
    char error[1024];
    FILE *fp;
    int ch;
    
    if (argc != 2)
        usage();
    
    doc = loadxmldoc(argv[1], error, 1024);
    if (!doc)
    {
        fprintf(stderr, "%s\n", error);
        return -1;
    }
    
    fp = xml_fopen(doc, argv[2], "r");
    if (fp)
    {
        while ( (ch = fgetc(fp)) != EOF)
            fputc(ch, stdout);
    }
    else
    {
        fprintf(stderr, "Can't open %s\n", argv[2]);
    }

    fclose(fp);
    killxmldoc(doc);
    
    return 0;
}

