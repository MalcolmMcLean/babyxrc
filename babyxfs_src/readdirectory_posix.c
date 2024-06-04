#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 strdup drop in replacement
 */
static char *mystrdup(const char *str)
{
  char *answer;

  answer = malloc(strlen(str) + 1);
  if(answer)
    strcpy(answer, str);

  return answer;
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

static char **mystrcatlist(char **list, char *str)
{
    int i;
    int N = 0;
    char **answer = 0;
    
    if (list)
    {
        for (i = 0; list[i]; i++)
            N++;
    }
    
    answer = realloc(list, (N+1) * sizeof(char *));
    if (!answer)
        return 0;
    
    answer[N] = str ? mystrdup(str) : 0;
    
    return answer;
    
    
}


/*
    is a file a regular file ?
 */
static int is_regular_file(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

/*
  is a file a directory ?
 */
static int is_directory(const char *path)
{
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

char **readdirectory_posix(const char *path)
{
    DIR *dirp;
    struct dirent *dp;
    char *pathslash;
    char *filepath;
    char *xmlfilename;
    int i;
    char **answer = 0;

    pathslash = mystrconcat(path, "/");
    if (!pathslash)
        goto out_of_memory;
    
    if ((dirp = opendir(path)) == NULL) {
        perror("couldn't open directory");
        return 0;
    }


    do {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL) {
          filepath = mystrconcat(pathslash, dp->d_name);
          if (is_directory(filepath) && dp->d_name[0] != '.')
          {
              xmlfilename = mystrconcat(dp->d_name, "/");
              if (!xmlfilename)
                  goto out_of_memory;
              answer = mystrcatlist(answer, xmlfilename);
              free (xmlfilename);
              xmlfilename = 0;
          }
          else if (dp->d_name[0] != '.' && is_regular_file(filepath))
              answer = mystrcatlist(answer, dp->d_name);
            
          free(filepath);
        }
    } while (dp != NULL);

    if (errno != 0)
        perror("error reading directory");

    free(pathslash);
    closedir(dirp);
    return answer;
    
out_of_memory:
    if (answer)
    {
        for (i = 0; answer[i]; i++)
            free(answer[i]);
        free(answer);
    }
    free(pathslash);
    closedir(dirp);
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    char **list;
    
    list = readdirectory_posix(argv[1]);
   if (list)
   {
       for (i = 0; list[i]; i++)
           printf("%s\n", list[i]);
   }
  else
    fprintf(stderr, "error opening %s\n", argv[1]);

  return 0;
}
