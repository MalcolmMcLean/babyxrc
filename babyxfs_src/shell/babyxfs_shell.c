//
//  babyxfs_test.c
//  babyxfs
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
#include "bbx_fs_shell.h"

#include "re.h"


/*
    This program is an impementation of the shell command for FileSystem.xml files.
 
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
      buffsize = buffsize + 100 + buffsize/10;
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

static void strtoupper(char *str)
{
    int i;
    
    for (i =0; str[i];i++)
        str[i] = toupper((unsigned char) str[i]);
}

static void pattoupper(char *pat)
{
    int i;
    
    for (i = 0; pat[i]; i++)
    {
        if (islower((unsigned char) pat[i]) &&
                (i == 0 || pat[i-1] != '\\'))
            pat[i] = toupper((unsigned char) pat[i]);
    }
}

static int matchword(char *pattern, char *line, int *length)
{
    char *word;
    int len;
    int answer = -1;
    
    if (length)
        *length = 0;
    len = strlen(pattern);
    
    word = strstr(line, pattern);
    if (word)
    {
        if ( (word == line ||
              isspace(*(word -1)) ||
              ispunct(*(word -1))) &&
            (word[len] == 0 ||
            (isspace(word[len]) ||
             ispunct(word[len]))))
        {
            answer = (int) (word - line);
            if (length)
                *length = len;
        }
    }
            
    return answer;
}

/*
 -i, --ignore-case: Ignores case distinctions in patterns and input data.
 -v, --invert-match: Selects the non-matching lines of the provided input pattern.
 -n, --line-number: Prefix each line of the matching output with the line number in the input file.
 -w: Find the exact matching word from the input file or string.
 -c: Count the number of occurrences of the provided pattern.
 */
static int grep_usage(FILE *out)
{
    fprintf(out, "grep - the Baby X regular expression parser\n");
    fprintf(out, "Usage: grep [options] <pattern> <file.txt>\n");
    fprintf(out, "\toptions:\n");
    fprintf(out, "\t\t-i - ignore case.\n");
    fprintf(out, "\t\t-v - invert (report non-matching lines).\n");
    fprintf(out, "\t\t-n - show line numbers.\n");
    fprintf(out, "\t\t-w - match exact word.\n");
    fprintf(out, "\t\t-c - count number of matching lines.\n");
    fprintf(out, "\n");
    return 0;
}


int babyxfs_grep_main(int argc, char **argv,FILE *out, FILE *in, FILE *err, BBX_FS_SHELL *shell, void *ptr)
{
   FILE *fp;
   char line[1024];
   char pline[1024];
    char errormessage[1024];
    
    int length;
    BBX_Options *opt;
    char *pattern = 0;
    char *fname = 0;
    int i_flag;
    int v_flag;
    int n_flag;
    int w_flag;
    int c_flag;
    int Nargs;
    int count = 0;
    int lineno = 0;
    
    opt = bbx_options(argc, argv, "-ivnwc");
    i_flag = bbx_options_get(opt, "-i", 0);
    v_flag = bbx_options_get(opt, "-v", 0);
    n_flag = bbx_options_get(opt, "-n", 0);
    w_flag = bbx_options_get(opt, "-w", 0);
    c_flag = bbx_options_get(opt, "-c", 0);
    
    if (bbx_options_error(opt, errormessage, 1024))
    {
        fprintf(err, "%s\n", errormessage);
        return 0;
    }
    
    Nargs = bbx_options_Nargs(opt);
    
   if (Nargs != 2)
   {
       grep_usage(out);
       return 0;
   }

    pattern = bbx_options_arg(opt, 0);
    fname = bbx_options_arg(opt, 1);
    
    bbx_options_kill(opt);
    opt = 0;
    
    if (i_flag)
        pattoupper(pattern);
    
   fp =  bbx_fs_shell_fopen(shell, fname, "r");
    if (!fp)
    {
        fprintf(err, "Can't open %s\n", argv[2]);
    }

   while (fgets(line, 1024, fp))
   {
       int m;
       strcpy(pline, line);
       if (i_flag)
           strtoupper(pline);
       if (w_flag)
           m = matchword(pattern, pline, &length);
       else
           m = re_match(pattern, pline, &length);
       lineno++;
       if ( (v_flag && m < 0) || (!v_flag && m >= 0))
       {
           if (c_flag)
               count++;
           else
           {
               if (n_flag)
                   fprintf(out, "%d: ", lineno);
               fprintf(out, "%s", line);
           }
       }
   }
    
   if (c_flag)
       fprintf(out, "%d matching lines\n", count);
    
   bbx_fs_shell_fclose(shell, fp);
   return 0;
}


int helloworld(int argc, char **argv, FILE *out, FILE *in, FILE *err, BBX_FS_SHELL *shell, void *ptr)
{
    return fprintf(out, "Hello world\n");
};

void usage()
{
    fprintf(stderr, "babyxfs_shell: run a shell on a FileSystem XML archive\n");
    fprintf(stderr, "Usage: - babyxfs_shell <filesystem.xml> [options] \n");
    fprintf(stderr, "\t options:\n");
    fprintf(stderr, "\t\t -editor - set the text file editor (e.g. \"vi\")\n");
    fprintf(stderr, "\n");
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
    BBX_FS_SHELL *shell;
    char line[1204];
    int err;
    
    BBX_Options *bbx_opt;
    char editor[256];
    int hasedit;
    int Nargs;
    
    strcpy(editor, "nano");
    
    bbx_opt = bbx_options(argc, argv, "");
    hasedit = bbx_options_get(bbx_opt, "-editor", "%256s", editor);
    Nargs = bbx_options_Nargs(bbx_opt);
    bbx_options_kill(bbx_opt);

    if (Nargs != 0)
        usage();
    
    if (hasedit)
    {
        fprintf(stderr, "editor %s\n", editor);
    }
    
    printf("Welcome to the babyxfs_shell \n");
    printf("\n");
    printf("type help for help\n");
    printf("type quit to exit the shell\n");
    printf("\n");
   
    shell = bbx_fs_shell(fs);
    bbx_fs_shell_addcommand(shell, "hello", helloworld, 0);
    bbx_fs_shell_addcommand(shell, "grep", babyxfs_grep_main, 0);
    
    if (hasedit)
    {
        bbx_fs_shell_set_editor(shell, editor);
    }
    
    err = bbx_fs_shell_run(shell, stdout, stdin, stderr);
    bbx_fs_shell_kill(shell);
    
    return err;
    
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
    
    fp = fopen(argv[1], "w");
    if (!fp)
    {
        fprintf(stderr, "Can't open xml file to write\n");
        exit(EXIT_FAILURE);
    }
    err = bbx_filesystem_dump(bbx_fs_xml, fp);
    if (err)
        fprintf(stderr, "Error writing FileSystem XML file to disk\n");
    fclose(fp);
    fp = 0;
    
    bbx_filesystem_kill(bbx_fs_xml);
    free(xmlstring);
    
    return 0;
}


