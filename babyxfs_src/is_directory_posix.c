//
//  is_directory_posix.c
//  bbx_ls
//
//  Created by Malcolm McLean on 08/06/2024.
//
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*
  is a file a directory ?
 */
int is_directory_posix(const char *path)
{
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}
