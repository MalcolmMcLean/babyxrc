#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "strnatcmp.h"
#include "bbx_options.h"

char **readdirectory_posix(const char *path);
int is_directory_posix(const char *path);

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

static char *bbx_strdup(const char *str)
{
    char *answer;
    
    answer = bbx_malloc(strlen(str) +1);
    assert(answer);
    strcpy(answer, str);
    
    return answer;
}

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

static int matchwild(const char *text, const char *glob)
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

static int isglob(const char *glob)
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

static char *directoryname(const char *path)
{
    char *answer = 0;
    const char *base = 0;
    
    base = strrchr(path, '/');
    
    if (base)
    {
        answer = bbx_malloc(base - path + 1);
        memcpy(answer, path, base - path);
        answer[base - path] = 0;
    }
    
    
    return answer;
}

static char *filename(const char *directory, const char *base)
{
    char *answer = 0;
    int dirlen;
    int baselen;
    
    if (!directory)
        return bbx_strdup(base);
    
    dirlen = strlen(directory);
    baselen = strlen(base);
    
    answer = bbx_malloc(baselen + dirlen + 1 + 1);
    if (!answer)
        goto out_of_memory;
    
    strcpy(answer, directory);
    strcat(answer, "/");
    strcat(answer, base);
    
    return answer;
    
out_of_memory:
    return 0;
}



static int dirnamewithglob(const char *pathandglob, char **pathret, char **globret)
{
    char *base = 0;
    char *path = 0;
    char *glob = 0;
    int answer = 0;
    
    if (pathret)
        *pathret = 0;
    if (globret)
        *globret = glob;
    
    base = strrchr(pathandglob, '/');
    
    if (base)
    {
        if (isglob(base + 1))
        {
            path = directoryname(pathandglob);
            if (!path)
                return -1;
            glob = bbx_strdup(base + 1);
        }
        else
            path = bbx_strdup(pathandglob);
    }
    else if (isglob(pathandglob))
        glob = bbx_strdup(pathandglob);
    else
        path = bbx_strdup(pathandglob);
   
    if (pathret)
        *pathret = path;
    if (globret)
        *globret = glob;
    
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



static int comp_alphabetical(const void *namea, const void *nameb)
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

static int comp_extension(const void *namea, const void *nameb)
{
    const char *stra = *(const char **) namea;
    const char *strb = *(const char **) nameb;
    char *exta;
    char *extb;
    
    exta = strrchr(stra, '.');
    extb = strrchr(strb, '.');
    
    if (!exta && !extb)
        return comp_natural(namea, nameb);
    if (!exta)
        return -1;
    if (!extb)
        return 1;
    return strcmp(exta, extb);
}


typedef struct
{
    char *name;
    int isbinary;
    unsigned long long size;
} DIRENTRY;

static int comp_size(const void *dirptra, const void *dirptrb)
{
    const DIRENTRY *dira = (const DIRENTRY *) dirptra;
    const DIRENTRY *dirb = (const DIRENTRY *) dirptrb;
    
    if (is_directory(dira->name) && is_directory(dirb->name))
        return strnatcmp(dira->name, dirb->name);
    else if (!is_directory(dira->name) && !is_directory(dirb->name))
        return (int) (dira->size - dirb->size);
    else if (!is_directory(dira->name))
        return 1;
    else
        return -1;
}

static void filter_out_directories(char **list)
{
    int i = 0;
    int j = 0;
    
    for (i = 0; list[i]; i++)
        if (!is_directory(list[i]))
            list[j++] = list[i];
    list[j] = 0;
}

static void filter_out_files(char **list)
{
    int i = 0;
    int j = 0;
    
    for (i = 0; list[i]; i++)
        if (is_directory(list[i]))
            list[j++] = list[i];
    list[j] = 0;
}

static void filter_in_glob(char **list, const char *glob)
{
    char *glob_d;
    int i = 0;
    int j = 0;
    
    if (!glob)
        return;
    glob_d = bbx_malloc(strlen(glob) + 2);
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

static void usage()
{
    fprintf(stderr, "bbx_ls - the Baby X ls program\n");
    fprintf(stderr,"Usage: bbx_ls [options] [path]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\toptions\n");
    fprintf(stderr, "\t\t-l - long mode\n");
    fprintf(stderr, "\t\t-f - just list files\n");
    fprintf(stderr, "\t\t-d - just list directories\n");
    fprintf(stderr, "\t\t-sort <sorttype> - set the type of sort\n");
    fprintf(stderr, "\t\t\t\"default\" the normal sort\n");
    fprintf(stderr, "\t\t\r\"alpha\" alphabetical sort\n");
    fprintf(stderr, "\t\t\t\"ext\" sort by extension\n");
    fprintf(stderr, "\t\t\t\"none\" don't sort (OS order)\n");
}

int main(int argc, char **argv)
{
    char errormessage[1024];
    int err = 0;
    DIRENTRY *directory = 0;
    char **dir = 0;
    char **dir_mem = 0;
 
    char sortmode [32] = "default";
    int l_flag = 0;
    int f_flag = 0;
    int d_flag = 0;
    char *path = 0;
    char *glob = 0;
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
    
    // printf("l %d\n", l_flag);
    // printf("f %d\n", f_flag);
    // printf("d %d\n", d_flag);
    // printf("sort %s\n", sortmode);
    
    if (strcmp(sortmode, "default") &&
        strcmp(sortmode, "alpha") &&
        strcmp(sortmode, "ext") &&
        strcmp(sortmode, "size") &&
        strcmp(sortmode, "none")
               )
    {
        fprintf(stderr, "unrecognised sort %s\n", sortmode);
        err = 1;
    }
    
    if (Nargs > 0)
        pathandglob = bbx_options_arg(bbx_opt, 0);
  //  for (i = 0; i <Nargs; i++)
    //    printf("-%s\n", bbx_options_arg(bbx_opt,i));
    if (bbx_options_error(bbx_opt, errormessage, 1024))
    {
        err = 1;
        fprintf(stderr, "%s\n", errormessage);
    }
    if (Nargs > 1 && err == 0)
    {
        err = 1;
        usage();
    }
    bbx_options_kill(bbx_opt);
    bbx_opt  = 0;
    
    if (err)
        return -1;
    
    if (pathandglob)
    {
        dirnamewithglob(pathandglob, &path, &glob);
        if (path == 0)
            path = bbx_strdup(".");
         
    }
    else
        path = bbx_strdup(".");
    
    if (strcmp(path, "."))
    {
        if (!is_directory_posix(path))
        {
            dir = bbx_malloc(sizeof(char *) * 2);
            dir[0] = bbx_strdup(basename(path));
            dir[1] = 0;
            if (strrchr(path, '/'))
            {
                *strchr(path, '/') = 0;
            }
            else
            {
                free(path);
                path = bbx_strdup(".");
            }
        }
        else
            dir = readdirectory_posix(path);
    }
    else
        dir = readdirectory_posix(path);
    if (!dir)
        return 0;
    
    N = 0;
    for (i = 0; dir[i]; i++)
        N++;
    dir_mem  = bbx_malloc( (N +1) * sizeof(char *));
    for (i = 0; i < N + 1; i++)
        dir_mem[i] = dir[i];
   
    
   if (f_flag)
       filter_out_directories(dir);
    if (d_flag)
        filter_out_files(dir);
    if (glob)
        filter_in_glob(dir, glob);
    
    
    N = 0;
    for (i = 0; dir[i]; i++)
        N++;
    
    if (!strcmp(sortmode, "default"))
        qsort(dir, N, sizeof(char *), comp_natural);
    else if (!strcmp(sortmode, "ext"))
        qsort(dir, N, sizeof(char *), comp_extension);
    else if (!strcmp(sortmode, "alpha"))
        qsort(dir, N, sizeof(char *), comp_alphabetical);
    
    int needextended = 0;
    if (l_flag)
        needextended = 1;
    if (!strcmp(sortmode, "size"))
        needextended = 1;
        
    if (needextended)
    {
        directory = bbx_malloc((N +1) * sizeof(DIRENTRY));
        
        for (i = 0; i < N; i++)
        {
            char *fname;
            
            if(!strcmp(path, "."))
                fname = bbx_strdup(dir[i]);
            else
                fname = filename(path, dir[i]);
            if (!fname)
                return - 1;
            
            directory[i].name = dir[i];
            if (!is_directory(directory[i].name))
            {
                directory[i].size = getsize(fname);
                directory[i].isbinary = is_binary(fname);
            }
            else
            {
                directory[i].size = 0;
                directory[i].isbinary = 0;
            }
            free(fname);
        }
    }

    if (!strcmp(sortmode, "size"))
    {
        if (needextended == 0 || directory == 0)
            return  - 1;
        qsort(directory, N, sizeof(DIRENTRY), comp_size);
        for(i = 0; i < N; i++)
            dir[i] = directory[i].name;
    }
    
    if (l_flag == 0)
    {
        for (i = 0; i < N; i++)
            fprintf(stdout, "%s\n", dir[i]);
    }
    else
    {
        for (i = 0; i < N; i++)
        {
            const char *type = "dir";
            if (is_directory(directory[i].name))
            {
                fprintf(stdout, "%-8s %10s \t%s\n", type, " ", directory[i].name);
            }
            else
            {
                type = directory[i].isbinary ? "binary" : "text";
                fprintf(stdout, "%-8s %10d \t%s\n", type, (int) directory[i].size, directory[i].name);
            }
        }
    }
    
    free (directory);
    free(path);
    free(glob);
    free(dir);
    
    for (i = 0; dir_mem[i]; i++)
        free (dir_mem[i]);
    free (dir_mem);

   return 0;
}
