//
//  listdirectory.c
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

int matchwild(const char *text, const char *glob)
{
   int i = 0;
   int j = 0;

   while (text[j] != 0 && glob[i] != 0)
   {
      if (glob[i] == '?')
           ;
       else if (glob[i] == '*')
           break;
       else if (glob[i] != text[j])
          break;
        i++;
        j++;
   }
   
   if (text[j] == 0 && glob[i] == 0)
       return 1;
    else if (glob[i] == '*')
    {
        do
        {
            if (matchwild(text + j++, glob + i + 1))
                return 1;
        } while (text[j-1]);
    }

   return 0;
}

char *mystrdup(const char *str)
{
    char *answer;
    
    answer = malloc(strlen(str) +1);
    if (answer)
        strcpy(answer, str);
    
    return answer;
}

/*
   Does a string consist entirely of white space? (also treat nulls as white)
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

char **cat_string(char **list, const char *str)
{
    char **answer = 0;
    char **temp;
    int N = 0;
    int i;
    
    if (!list)
    {
        answer = malloc(2 * sizeof(char *));
        answer[1] = 0;
        answer[0] = mystrdup(str);
    }
    else
    {
        for (i = 0; list[i]; i++)
            N++;
        temp = realloc(list, (N+ 2) * sizeof(char *));
        answer = temp;
        answer[N+1] = 0;
        answer[N] = mystrdup(str);
    }
    
    return answer;;
}

char **cat_list(char **lista, char **listb)
{
    char **answer;
    int Na = 0;
    int Nb = 0;
    int i;
    
    if (!lista)
        return listb;
    if (!listb)
        return  lista;
    for (i = 0; lista[i]; i++)
        Na++;
    for (i = 0; listb[i]; i++)
        Nb++;
    
    answer = realloc(lista, (Na + Nb + 1) * sizeof(char **));
    for (i = 0; listb[i]; i++)
        answer[Na + i] = listb[i];
    answer[Na+i] = 0;
    
    return answer;
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


char **directory_list_r(XMLNODE *node, const char *glob, int pos)
{
    char **answer = 0;
    int N = 0;
    const char *nodename;
    char *nameglob = 0;
    XMLNODE *child;
    char **sub;
    int lastdir;
    
    nameglob = directoryname(glob, pos);
    lastdir = glob[pos + strlen(nameglob) + 1] == 0 ? 1 : 0;
    while (node)
    {
        if (!strcmp(xml_gettag(node), "directory"))
        {
            nodename = xml_getattribute(node, "name");
            if (nodename && matchwild(nodename, nameglob))
            {
                if (lastdir)
                    answer = cat_string(answer, nodename);
                else
                {
                    sub = directory_list_r(node->child, glob, pos + (int) strlen(nameglob) + 1);
                    answer = cat_list(answer, sub);
                }
            }
        }
        else if(lastdir && !strcmp(xml_gettag(node), "file"))
        {
            nodename = xml_getattribute(node, "name");
            if (nodename && matchwild(nodename, nameglob))
            {
                answer = cat_string(answer, nodename);
            }
        }
        
        node = node->next;
    }
    
    free(nameglob);
    return  answer;
}

char **xml_listdirectory_r(XMLNODE *node, const char *glob)
{
    char **answer = 0;
    while (node)
    {
        if (!strcmp(xml_gettag(node), "FileSystem"))
        {
            answer = directory_list_r(node->child, glob, 0);
            if (answer)
                break;
        }
        if (node->child)
        {
            answer = xml_listdirectory_r(node->child, glob);
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
char **xml_listdirectory(XMLDOC *doc, const char *glob)
{
    return xml_listdirectory_r(xml_getroot(doc), glob);
}

void usage()
{
    fprintf(stderr, "listdirectory: ls command for FileSystem XML files\n");
    fprintf(stderr, "Usage: listdirectory <filesystem.xml> <pathtofile>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "For example, listdirectory poemfiles.xml /poems/Blake/*\n");
    fprintf(stderr, "The XML files poemfiles.xml is FileSystem file which\n");
    fprintf(stderr, "contains poems. The command will list all the poems \n");
    fprintf(stderr, "by Blake\n");
    fprintf(stderr, "Generate the FileSystem files with the program directorytoxml\n");
}

int main(int argc, char **argv)
{
    XMLDOC *doc = 0;
    char error[1024];
    char **list;
    int i;
    
    if (argc != 3)
    {
        usage();
        return -1;
    }
    
    doc = loadxmldoc(argv[1], error, 1024);
    if (!doc)
    {
        fprintf(stderr, "%s\n", error);
        return -1;
    }
    
    list = xml_listdirectory(doc, argv[2]);
    if (list)
    {
        for (i = 0; list[i]; i++)
            printf("%s\n", list[i]);
    }
    else
    {
        fprintf(stderr, "Can't list %s\n", argv[2]);
    }

    if (list)
    {
        for (i = 0; list[i]; i++)
            free(list[i]);
        free(list);
    }
    
    killxmldoc(doc);
    
    return 0;
}

