//
//  babyxlsxml.c
//  directorytoxml
//
//  Created by Malcolm McLean on 28/05/2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "bbx_options.h"
#include "bbx_filesystem.h"

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

static void *bbx_malloc(size_t size)
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

static char *bbx_strdup(const char *str)
{
    char *answer = bbx_malloc(strlen(str) +1);
    strcpy(answer, str);
    
    return answer;
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

int isdirectory(const char *name)
{
    char *slash;
    slash = strrchr(name, '/');
    if (!slash)
        return 0;
    if (strlen(slash) > 1)
        return 0;
    return 1;
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

char **fs_listdirectory_r(BBX_FileSystem *fs, char *dir, const char *glob, int pos)
{
    char **answer = 0;
    int N = 0;
    char *filename;
    char *nameglob = 0;
    char **sub;
    int nullpos;
    int lastdir;
    char buff[FILENAME_MAX];
    char **list;
    int i;
    
    nullpos = strlen(dir);
    nameglob = directoryname(glob, pos);
    list = bbx_filesystem_list(fs, dir);
    if (!list)
        return 0;
    lastdir = glob[pos + strlen(nameglob) + 1] == 0 ? 1 : 0;
    
    for (i = 0; list[i]; i++)
    {
       filename = bbx_strdup(list[i]);
       if (strrchr(list[i], '/') == list[i] + strlen(list[i]) - 1)
       {
           filename[strlen(filename) - 1] = 0;
           if (filename && matchwild(filename, nameglob))
           {
               if (lastdir)
               {
                   answer = cat_string(answer, list[i]);
               }
               else
               {
                   if (!strcmp(dir, "/"))
                       strcat(dir, filename);
                   else
                   {
                       strcat(dir, "/");
                       strcat(dir, filename);
                   }
                   sub = fs_listdirectory_r(fs, dir, glob, pos + (int) strlen(nameglob) + 1);
                   dir[nullpos] = 0;
                   answer = cat_list(answer, sub);
               }
           }
       }
        else if(lastdir)
        {
            if (filename && matchwild(filename, nameglob))
            {
                answer = cat_string(answer, filename);
            }
        }
        free (filename);
       
    }
    
    free(nameglob);
    
    return answer;
    
}


char **fs_listdirectory(BBX_FileSystem *fs, const char *glob)
{
    char path[PATH_MAX];
    strcpy(path, "/");
    return fs_listdirectory_r(fs, path, glob, 0);
}

char **listwild(BBX_FileSystem *fs, const char *glob, int dosize)
{
    char **answer;
    int i;
    int j;
    char fullpath[PATH_MAX];
    char buff[256];
    FILE *fp = 0;
    int size;
    
    if (!strchr(glob, '*') && !strchr(glob, '?'))
    {
        answer = bbx_filesystem_list(fs, glob);
        if (dosize)
        {
            strcpy(fullpath, glob);
            for (i = 0; answer[i]; i++)
            {
                if (isdirectory(answer[i]))
                    continue;
                strcat(fullpath, "/");
                strcat(fullpath, answer[i]);
                fprintf(stderr, "%s\n", fullpath);
                fp = bbx_filesystem_fopen(fs, fullpath, "r");
                if (fp)
                {
                    fseek(fp, 0, SEEK_END);
                        size = (int) ftell(fp);
                    bbx_filesystem_fclose(fs, fp);
                }
                else
                    size = -1;
                snprintf(buff, 256, "%s %d", answer[i], size);
                free(answer[i]);
                answer[i] = bbx_strdup(buff);
                j = strlen(fullpath);
                while(fullpath[j] != '/')
                    j--;
                fullpath[j] = 0;
            }
        }
        return answer;
    }
    else
        return fs_listdirectory(fs, glob);
}

void usage()
{
    fprintf(stderr, "babyxfs_ls: ls command for FileSystem XML files\n");
    fprintf(stderr, "Usage: babyxfs_ls [options] <filesystem.xml> <pathtofile>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "\t -l long mode\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "For example, babyxfs_ls poemfiles.xml /poems/Blake/*\n");
    fprintf(stderr, "The XML files poemfiles.xml is FileSystem file which\n");
    fprintf(stderr, "contains poems. The command will list all the poems \n");
    fprintf(stderr, "by Blake\n");
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
    char **list;
    int i;
    BBX_Options *bbx_opt;
    int size = 0;
    int Nargs;
    char *path;
    char errormessage[10224];
    
    bbx_opt = bbx_options(argc, argv, "");
    size = bbx_options_get(bbx_opt, "-l", 0);
    Nargs = bbx_options_Nargs(bbx_opt);
    bbx_options_error(bbx_opt, errormessage, 1024);
    if (Nargs != 1)
        usage();
    path = bbx_options_arg(bbx_opt, 0);

    list = listwild(fs, path, size);
    if (!list)
        return -1;
    
    for (i = 0; list[i]; i++)
        printf("%s\n", list[i]);
    
    for (i = 0; list[i]; i++)
        free(list[i]);
    free(list);
    
    free(path);
    bbx_options_kill(bbx_opt);
    
    return 0;
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

