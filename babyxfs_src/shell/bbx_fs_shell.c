//
//  bbx_fs_shell.c
//  
//
//  Created by Malcolm McLean on 04/06/2024.
//

#include "bbx_fs_shell.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct bbx_fs_shell
{
    BBX_FileSystem *bbx_fs;
    char path[1204];
    FILE *stdout;
    FILE *stderr;
} BBX_FS_SHELL;

static int getcommand(char *command, int N, const char *input);

BBX_FS_SHELL *bbx_fs_shell(BBX_FileSystem *bbx_fs, FILE *output, FILE *error)
{
    BBX_FS_SHELL *shell;
    
    shell = malloc(sizeof(BBX_FS_SHELL));
    shell->bbx_fs = bbx_fs;
    shell->stdout = output;
    shell->stderr = error;
    strcpy(shell->path, "/");
    
    fprintf(shell->stdout, "BBX$ ");
    
    return shell;
}

void bbx_fs_shell_kill(BBX_FS_SHELL *shell)
{
   if (shell)
   {
       free(shell);
   }
}


int bbx_fs_shell_inputline(BBX_FS_SHELL *shell, const char *line)
{
    char command[32];
    
    getcommand(command, 32, line);
    if (!strcmp(command, "quit"))
        return -1;
    if (!strcmp(command, "ls"))
    {
        char **dir;
        int i;
        
        dir = bbx_filesystem_list(shell->bbx_fs, shell->path);
        if (dir)
        {
            for (i = 0; dir[i]; i++)
            {
                fprintf(shell->stdout, "%s\n", dir[i]);
            }
            for (i = 0; dir[i]; i++)
            {
                free(dir[i]);
            }
            free(dir);
        }
    }
    if (!strcmp(command, "cd"))
    {
        char target[256];
        char *args;
        char **dir = 0;
        char *pathroot = 0;
        int i;
        
        args = strstr(line, command) + strlen(command);
        getcommand(target, 256, args);
        
        if (!strcmp(target, ".."))
        {
            pathroot = strrchr(shell->path, '/');
            if (pathroot)
                *pathroot = 0;
            if (!strcmp(shell->path, ""))
                strcpy(shell->path, "/");
        }
        
        if (pathroot == 0)
            dir = bbx_filesystem_list(shell->bbx_fs, shell->path);
        if (dir)
        {
            for (i = 0; dir[i]; i++)
            {
                if (!strncmp(dir[i], target, strlen(target)) && dir[i][strlen(target)] == '/')
                {
                    if (strcmp(shell->path, "/"))
                        strcat(shell->path, "/");
                    strcat(shell->path, target);
                }
            }
            for (i = 0; dir[i]; i++)
            {
                free(dir[i]);
            }
            free(dir);
        }
        
    }
    fprintf(shell->stdout, "BBX$ ");
    return 0;
}

static int getcommand(char *command, int N, const char *input)
{
    int i = 0;
    int j = 0;
    
    while (isspace((unsigned char) input[i]))
        i++;

    while (!isspace((unsigned char) input[i]) && j < N - 1)
        command[j++] = input[i++];
    
    command[j++] = 0;
    if (j == N)
        return -1;
    
    return 0;
}
