//
//  bbx_write_source_archive.c
//  testbabyxfilesystem
//
//  Created by Malcolm McLean on 31/05/2024.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#include "bbx_write_source_archive.h"
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

/*
    encode binary to text using uuencoding.
 */
static char *uuencodestr(const unsigned char *binary, int N)
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



/*
  test if a character is escaped in C
  Params: ch - the character to test
  Returns: 1 if escaped in C strings, else 0
*/
static int c_escaped(int ch)
{
  char *escapes = "\a\b\f\n\r\t\v\?\'\"\\";

  if(ch == 0)
    return 1;
  return strchr(escapes, ch) ? 1 : 0;

}

/*
  get the C escape character to represent ch
  Params: ch - an escaped character
  Returns: character that stands in for it in C escape sequence,
    0 if ch is not an escaped character
*/
static char c_escapechar(int ch)
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

/*
  convert a string to a C language string;
  Params:
    str - the string to convert
  Returns: C version of string, 0 on out of memory
  Notes: newlines are represented by breaks in the string.
*/
char *text_to_cstring(const char *str)
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
    else if(c_escaped(str[i]))
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
    else if(c_escaped(str[i]))
    {
      answer[j++] = '\\';
      answer[j++] = c_escapechar(str[i]);
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
   Take plain text from fpin and write as xml to fpout
 */
static int xml_escapefilter(FILE *fpout, FILE *fpin)
{
    char *text = 0;
    char *xmltext = 0;
    int i;

    text = fslurp(fpin);
    if (!text)
        goto out_of_memory;
    
    xmltext = xml_escape(text);
    if (!xmltext)
        goto out_of_memory;
    for (i = 0; xmltext[i];i++)
        fputc(xmltext[i], fpout);
    
    free(text);
    free(xmltext);
    
    return 0;
    
out_of_memory:
    free(text);
    free(xmltext);
    
    return -1;
}
static int xml_writecdata(FILE *fp, const char *data)
{
    int i;
    int err;
    
    err = fprintf(fp, "<![CDATA[");
    if (err < 0)
        return -1;
    for (i = 0; data[i]; i++)
    {
        err = fputc(data[i], fp);
        if (err == EOF)
            return -1;
        if (data[i] == ']' && data[i+1] == ']' && data[i+2] == '>')
        {
            i++;
            err = fputc(data[i], fp);
            if (err == EOF)
                return -1;
            err = fprintf(fp, "]]>");
            if (err < 0)
                return  -1;
            err = fprintf(fp, "<![CDATA[");
            if (err < 0)
                return -1;
        }
    }
    err = fprintf(fp, "]]>");
    if (err < 0)
        return -1;
    return 0;
}


static int xml_uuencode_filter(FILE *fpout, FILE *fpin)
{
    int N;
    unsigned char *binary = 0;
    char *uucode = 0;
    char *badclose;
    int i;
    
    binary = fslurpb(fpin, &N);
    if (!binary)
        goto out_of_memory;
    uucode = uuencodestr(binary,N);
    if (!uucode)
        goto out_of_memory;
    xml_writecdata(fpout, uucode);
    free(binary);
    free(uucode);
    
    return 0;
    
out_of_memory:
    free(binary);
    free(uucode);
    return -1;
}



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



static int writefile(FILE *fp, XMLNODE *node)
{
    FILE *fpin;
    const char *type = 0;
    
    if (strcmp(xml_gettag(node), "file"))
        return -1;
    
    type = xml_getattribute(node, "type");
    if (!type)
        return -1;
    
    fpin = file_fopen(node);
    if (!fpin)
        return -1;
    
    if (!strcmp(type, "binary"))
    {
        xml_uuencode_filter(fp, fpin);
    }
    else if (!strcmp(type, "text"))
    {
        xml_escapefilter(fp, fpin);
    }
    else
        return -1;
    
    fclose(fpin);
    return 0;
}

static int processregularfile(FILE *fp, XMLNODE *node, int depth)
{
    int error;
    const char *filename = 0;
    const char *type = 0;
    char *xmlfilename = 0;
    int i;
    
    filename = xml_getattribute(node, "name");
    if (!filename)
        goto out_of_memory;
    xmlfilename = xml_escape(filename);
    if (!xmlfilename)
        goto out_of_memory;
    
    type = xml_getattribute(node, "type");
    if (!type)
        goto out_of_memory;
    if (strcmp(type, "binary") && strcmp(type, "text"))
        return -1;
    
    for (i = 0; i <depth; i++)
        fprintf(fp, "\t");
    fprintf(fp, "<file name=\"%s\" type=\"%s\">\n", xmlfilename, type);
    writefile(fp, node);
    fprintf(fp, "\n");
    for (i = 0; i <depth;i++)
        fprintf(fp, "\t");
    fprintf(fp, "</file>\n");
    
    free(xmlfilename);
    return 0;
    
out_of_memory:
    free(xmlfilename);
    return -1;
}

static int processsourcefile(FILE *fp, XMLNODE *node, int depth, const char *source, const char *source_xml_name)
{
    int error;
    const char *filename = 0;
    const char *type = 0;
    char *xmlfilename = 0;
    char *escaped = 0;
    char *xmltext = 0;
    int i;
    
    filename = xml_getattribute(node, "name");
    if (!filename)
        goto out_of_memory;
    xmlfilename = xml_escape(filename);
    if (!xmlfilename)
        goto out_of_memory;
    
    type = "text";
    if (!type)
        goto out_of_memory;
    if (strcmp(type, "binary") && strcmp(type, "text"))
        return -1;
    
    for (i = 0; i <depth; i++)
        fprintf(fp, "\t");
    fprintf(fp, "<file name=\"%s\" type=\"%s\">\n", xmlfilename, type);
    escaped = text_to_cstring(source);
    if (!escaped)
        goto out_of_memory;
    
    fprintf(fp, "char %s[] = ", source_xml_name);
    xmltext = xml_escape(escaped);
    if (!xmltext)
        goto out_of_memory;
    for (i= 0; xmltext[i];i++)
        fputc(xmltext[i], fp);
    
    fprintf(fp, ";\n");
    
    free(escaped);
    free(xmltext);
    fprintf(fp, "\n");
    for (i = 0; i <depth;i++)
        fprintf(fp, "\t");
    fprintf(fp, "</file>\n");
    
    free(xmlfilename);
    return 0;
    
out_of_memory:
    free(xmlfilename);
    return -1;
}


static int bbx_write_source_archive_r(FILE *fp, XMLNODE *node, int depth, const char *source_xml, const char *source_xml_file, const char *source_xml_name)
{
    const char *name = 0;
    int ch;
    int err = 0;
    int i;
    char *xmlname = 0;
    
    while (node)
    {
        if (!strcmp(xml_gettag(node), "file"))
        {
            name = xml_getattribute(node, "name");
            if (!name)
                goto skip_node;
            if (!strcmp(name, source_xml_file))
            {
                processsourcefile(fp, node, depth, source_xml, source_xml_name);
            }
            else
            {
                processregularfile(fp, node, depth);
            }
        }
        else if (!strcmp(xml_gettag(node), "directory"))
        {
            name = xml_getattribute(node, "name");
            if (!name)
                goto skip_node;
        
            xmlname = xml_escape(name);
            if (!xmlname)
                goto out_of_memory;
            for (i = 0; i < depth; i++)
                    fprintf(fp, "\t");
            fprintf(fp, "<directory name=\"%s\">\n", xmlname);
            err |= bbx_write_source_archive_r(fp, node->child, depth + 1, source_xml, source_xml_file, source_xml_name);
            for (i = 0; i < depth; i++)
                    fprintf(fp, "\t");
            fprintf(fp, "</directory>\n");
            free(xmlname);
            xmlname = 0;
        }
        
    skip_node:
        node = node->next;
    }
    
    return err;
    
out_of_memory:
    return -1;
}

int bbx_write_source_archive_root(FILE *fp, XMLNODE *node, int depth, const char *source_xml, const char *source_xml_file, const char *source_xml_name)
{
    int i;
    int err = 0;

    while (node)
    {
        if (!strcmp(xml_gettag(node), "FileSystem"))
        {
            for (i =0; i < depth; i++)
                fprintf(fp, "\t");
            fprintf(fp, "<FileSystem>\n");
            err |= bbx_write_source_archive_r(fp, node->child, depth + 1, source_xml, source_xml_file, source_xml_name);
            for (i =0; i < depth; i++)
                fprintf(fp, "\t");
            fprintf(fp, "</FileSystem>\n");
        }
        else
        {
            err |= bbx_write_source_archive_root(fp, node->child, depth, source_xml, source_xml_file, source_xml_name);
        }
        
        node = node->next;
    }
    
    return err;
}

int bbx_write_source_archive(FILE *fp, const char *source_xml, const char *source_xml_file, const char *source_xml_name)
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
    
    answer = bbx_write_source_archive_root(fp, root, 0, source_xml, source_xml_file, source_xml_name);
    
    killxmldoc(doc);
    
    return  answer;
}
