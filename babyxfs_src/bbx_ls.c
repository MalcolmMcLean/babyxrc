#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strnatcmp.h"

#include "bbx_options.h"

char **readdirectory_posix(const char *path);

static int getsize(const char *fname)
{
    FILE *fp;
    long pos;
    
    fp = fopen(fname, "r");
    if (!fp)
        return 0;
    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    fclose(fp);
    
    return (int) pos;
}

static int is_binary(const char *name)
{
    FILE *fp;
    int ch;
    int answer = 0;
    
    fp = fopen(name, "r");
    if (!fp)
        return 0;
    
    while ( (ch = fgetc(fp)) != EOF)
    {
        if (ch > 127)
            answer = 1;
        else if (ch < 32)
        {
            const char *escapes = "\b\f\n\r\t\v";
            if (!strchr(escapes, ch))
                answer = 1;
        }
        if (answer)
            break;
    }
    
    fclose(fp);
    
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

int isglob(const char *glob)
{
    if (strchr(glob, '*'))
        return 1;
    if (strchr(glob, '?'))
        return 1;
    
    return 0;
}

static const char *basename(const char *path)
{
    const char *answer = 0;
    answer = strrchr(path, '/');
    if (answer)
        answer = answer + 1;
    else
        answer = path;
    
    return answer;
}


static int is_directory(const char *name)
{
    int N;
    
    N = strlen(name);
    if (N == 0)
        return 0;
    
    return name[N-1] ==  '/' ? 1 : 0;
}



static int compf(const void *namea, const void *nameb)
{
    const char *stra = *(const char **) namea;
    const char *strb = *(const char **) nameb;
    
    if (is_directory(stra) && is_directory(strb))
        return strcmp(stra, strb);
    else if (!is_directory(stra) && !is_directory(strb))
        return strcmp(stra, strb);
    else if (!is_directory(stra))
        return 1;
    else
        return -1;
}

static int comp_extension(const void *namea, const void *nameb)
{
    const char *stra = *(const char **) namea;
    const char *strb = *(const char **) nameb;
    char *exta;
    char *extb;
    
    exta = strrchr(stra, '.');
    extb = strrchr(strb, '.');
    
    if (!exta && !extb)
        return compf(namea, nameb);
    if (!exta)
        return -1;
    if (!extb)
        return 1;
    return strcmp(exta, extb);
}

static int comp_natural(const void *namea, const void *nameb)
{
    const char *stra = *(const char **) namea;
    const char *strb = *(const char **) nameb;
    
    if (is_directory(stra) && is_directory(strb))
        return strnatcmp(stra, strb);
    else if (!is_directory(stra) && !is_directory(strb))
        return strnatcmp(stra, strb);
    else if (!is_directory(stra))
        return 1;
    else
        return -1;
}

typedef struct
{
    char *name;
    int isbinary;
    unsigned long long size;
} DIRENTRY;

int bbx_ls(FILE *fp, char **directory, const char *glob)
{
  int i;
    int N = 0;
    
    for (i = 0; directory[i]; i++)
        N++;
        
  // qsort(directory, N, sizeof(char *), compf);

  for (i = 0; directory[i]; i++)
  {
     int size = is_directory(directory[i]) ? 0 : getsize(directory[i]);
      const char *type;
      
      if (glob && !matchwild(directory[i], glob))
          continue;
      
      if (is_directory(directory[i]))
          type = "dir";
      else
          type = is_binary(directory[i]) ? "binary" : "text";
      
     fprintf(fp, "%-8s %10d \t%s\n", type, size, directory[i]);
  }

  return 0;
}

void filter_out_directories(char **list)
{
    int i = 0;
    int j = 0;
    
    for (i = 0; list[i]; i++)
        if (!is_directory(list[i]))
            list[j++] = list[i];
    list[j] = 0;
}

void filter_out_files(char **list)
{
    int i = 0;
    int j = 0;
    
    for (i = 0; list[i]; i++)
        if (is_directory(list[i]))
            list[j++] = list[i];
    list[j] = 0;
}

void filter_in_glob(char **list, const char *glob)
{
    char *glob_d;
    int i = 0;
    int j = 0;
    
    if (!glob)
        return;
    glob_d = malloc(strlen(glob) + 2);
    strcmp(glob_d, glob);
    strcat(glob_d, "/");
    
    for (i = 0; list[i]; i++)
    {
        if (is_directory(list[i]) && matchwild(list[i], glob_d))
            list[j++] = list[i];
        else if (!is_directory(list[i]) && matchwild(list[i], glob))
            list[j++] = list[i];
    }
    list[j] = 0;
    
    free(glob_d);
}

int main(int argc, char **argv)
{
    DIRENTRY *directory = 0;
    char **dir = 0;
    char *glob = 0;
    char sortmode [32] = "default";
    int l_flag = 0;
    int f_flag = 0;
    int d_flag = 0;
    char *path = 0;
    char *pathandglob = 0;
    int N;
    int i;
    BBX_Options *bbx_opt;
    int Nargs = 0;
    
    bbx_opt = bbx_options(argc, argv, "");
    l_flag = bbx_options_get(bbx_opt, "-l", 0);
    f_flag = bbx_options_get(bbx_opt, "-f", 0);
    d_flag = bbx_options_get(bbx_opt, "-d", 0);
    bbx_options_get(bbx_opt, "-sort", "%32s", sortmode, 0);
    Nargs = bbx_options_Nargs(bbx_opt);
    
    printf("l %d\n", l_flag);
    printf("f %d\n", f_flag);
    printf("d %d\n", d_flag);
    printf("sort %s\n", sortmode);
    if (Nargs > 0)
        pathandglob = bbx_options_arg(bbx_opt, 0);
    for (i = 0; i <Nargs; i++)
        printf("-%s\n", bbx_options_arg(bbx_opt,i));
    bbx_options_kill(bbx_opt);
    bbx_opt  = 0;
    
    
    if (pathandglob)
    {
        char * base = strrchr(pathandglob, '/');
        if (base)
        {
            if (isglob(base + 1))
            {
                glob = base + 1;
                path = malloc(base - pathandglob + 1);
                memcpy(path, pathandglob, base -  pathandglob);
                path[base - pathandglob] = 0;
            }
            else
            {
                path = pathandglob;
            }
        }
        else
        {
            if (isglob(pathandglob))
            {
                glob = pathandglob;
                path = ".";
            }
            else
            {
                path = pathandglob;
            }
                
        }
    }
    else
        path = ".";
    
    printf("here path %s\n", path);
    
    FILE *fp = 0;
    fp = fopen(path, "r");
    if (fp)
    {
        dir = malloc(sizeof(char *) * 2);
        dir[0] = basename(path);
        dir[1] = 0;
        if (strrchr(path, '/'))
        {
            char *copy_path;
            char *ptr = strrchr(path, '/');
            int len;
            
            len = ptr - path;
            copy_path = malloc((len+1) *sizeof(char *));
            memcpy(copy_path, path, len);
            copy_path[len] = 0;
            path = copy_path;
        }
        else
            path = ".";
        fclose(fp);
    }
    else
        dir = readdirectory_posix(path);
    if (!dir)
        return 0;
    
   if (f_flag)
       filter_out_directories(dir);
    if (d_flag)
        filter_out_files(dir);
    if (glob)
        filter_in_glob(dir, glob);
    
    for (i = 0; dir[i]; i++)
        printf("%s\n", dir[i]);
    printf("\n");
    
    N = 0;
    for (i =0; dir[i]; i++)
        N++;
    if (!strcmp(sortmode, "default"))
        qsort(dir, N, sizeof(char *), compf);
    else if (!strcmp(sortmode, "ext"))
        qsort(dir, N, sizeof(char *), comp_extension);
    else if (!strcmp(sortmode, "natural"))
        qsort(dir, N, sizeof(char *), comp_natural);
    
    if (1)
    {
        int N = 0;
        
        for (i = 0; dir[i]; i++)
            N++;
        directory = malloc((N +1) * sizeof(DIRENTRY));
        if (!directory)
        {
            return -1;
        }
        for (i = 0; i < N; i++)
        {
            char fname[1024];
            if (strcmp(path, "."))
                snprintf(fname, 1024, "%s/%s", path, dir[i]);
            else
                snprintf(fname, 1024, "%s", dir[i]);
            
            directory[i].name = dir[i];
            if (!is_directory(directory[i].name))
            {
                printf("calling getsize with %s\n", fname);
                directory[i].size = getsize(fname);
                directory[i].isbinary = is_binary(fname);
            }
            else
            {
                directory[i].size = 0;
                directory[i].isbinary = 0;
            }
        }
        
        
        for (i = 0; i < N; i++)
        {
            const char *type = "dir";
            if (!is_directory(directory[i].name))
                type = directory[i].isbinary ? "binary" : "text";
            fprintf(stdout, "%-8s %10d \t%s\n", type, (int) directory[i].size, directory[i].name);
        }
    }
        
   // bbx_ls(stdout, dir, glob);

   return 0;
}
