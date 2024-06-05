//
//  bbx_fs_shell.c
//  
//
//  Created by Malcolm McLean on 04/06/2024.
//

#include "bbx_fs_shell.h"
#include "bbx_options.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "basic.h"

extern char cp_usage[];
extern char help_usage[];

typedef struct bbx_fs_command
{
    char *name;
    int (*fptr)(int argc, char **argv, FILE *out, FILE *in, FILE *err);
    struct bbx_fs_command *next;
} BBX_FS_COMMAND;

typedef struct bbx_fs_shell
{
    BBX_FileSystem *bbx_fs;
    char path[1204];
    FILE *stdin;
    FILE *stdout;
    FILE *stderr;
    BBX_FS_COMMAND *commands;
} BBX_FS_SHELL;


static int bbx_fs_shell_inputline(BBX_FS_SHELL *shell, const char *line);
static int donormalcommand(BBX_FS_SHELL *shell, const char *command, int argc, char **argv);
static int run_external_command(BBX_FS_SHELL *shell, const char *command,
                                int argc, char **argv);
static int run_internal_command(BBX_FS_SHELL *shell, const char *command, int argc, char **argv);

static int help(BBX_FS_SHELL *shell, int argc, char **argv);
static int rm(BBX_FS_SHELL *shell, int argc, char **argv);
static int cp(BBX_FS_SHELL *shell, int argc, char **argv);
static int mv(BBX_FS_SHELL *shell, int argc, char **argv);
static int cat(BBX_FS_SHELL *shell, int argc, char **argv);
static int mkdir(BBX_FS_SHELL *shell, int argc, char **argv);
static int rmdir(BBX_FS_SHELL *shell, int argc, char **argv);

static int bbx_fs_system(BBX_FS_SHELL *shell, int argc, char **argv);
static int import(BBX_FS_SHELL *shell, int argc, char **argv);
static int export(BBX_FS_SHELL *shell, int argc, char **argv);
static int babybasic(BBX_FS_SHELL *shell, int argc, char **argv);

static char **getargs(char *str);
static int getcommand(char *command, int N, const char *input);

#define bbx_malloc malloc

static char *bbx_strdup(const char *str)
{
    char *answer = bbx_malloc(strlen(str) +1);
    strcpy(answer, str);
    
    return answer;
}

BBX_FS_SHELL *bbx_fs_shell(BBX_FileSystem *bbx_fs)
{
    BBX_FS_SHELL *shell;
    
    shell = malloc(sizeof(BBX_FS_SHELL));
    shell->bbx_fs = bbx_fs;
    shell->stdin = stdin;
    shell->stdout = stdout;
    shell->stderr = stderr;
    strcpy(shell->path, "/");
    shell->commands = 0;
   
    
    return shell;
}

void bbx_fs_shell_kill(BBX_FS_SHELL *shell)
{
   if (shell)
   {
       free(shell);
   }
}

int bbx_fs_shell_run(BBX_FS_SHELL *shell, FILE *output, FILE *input, FILE *error)
{
    char line[1024];
    int err = 0;
    
    shell->stdin = input;
    shell->stdout = output;
    shell->stderr = error;
    fprintf(shell->stdout, "BBX$ ");
    
    while (fgets(line, 1024, shell->stdin))
    {
        err = bbx_fs_shell_inputline(shell, line);
        if (err)
            break;
        fprintf(shell->stdout, "BBX$ ");
    }
    
    return err;
}

int bbx_fs_shell_addcommand(BBX_FS_SHELL *shell, const char *command,
                            int (*fptr)(int argc, char **argv, FILE *out, FILE *in, FILE *err))
{
    BBX_FS_COMMAND *bbx_command;
    
    bbx_command = bbx_malloc(sizeof(BBX_FS_COMMAND));
    bbx_command->fptr = fptr;
    bbx_command->name = bbx_strdup(command);
    bbx_command->next = shell->commands;
    shell->commands = bbx_command;
}


static int bbx_fs_shell_inputline(BBX_FS_SHELL *shell, const char *line)
{
    char command[32];
    char **args;
    char *linecopy;
    int Nargs = 0;
    int i;
    
    linecopy = bbx_strdup(line);
    args = getargs(linecopy);
    free(linecopy);
    
    for (i = 0; args[i]; i++)
        Nargs++;
    
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
    else if (!strcmp(command, "cd"))
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
    else
    {
        donormalcommand(shell, command, Nargs, args);
    }
    
    for (i = 0; args[i]; i++)
        free(args[i]);
    free(args);
 
    return 0;
}

static int donormalcommand(BBX_FS_SHELL *shell, const char *command, int argc, char **argv)
{
    run_internal_command(shell, command, argc, argv);
    run_external_command(shell, command, argc, argv);
    return 0;
}

static int run_external_command(BBX_FS_SHELL *shell, const char *command,
                                int argc, char **argv)
{
    BBX_FS_COMMAND *cmd = shell->commands;
    
    while (cmd)
    {
        if (!strcmp(cmd->name, command))
        {
            (*cmd->fptr)(argc, argv,shell->stdout, shell->stdin, shell->stderr);
        }
        cmd = cmd->next;
    }
    
    return  0;;
}


static int run_internal_command(BBX_FS_SHELL *shell, const char *command, int argc, char **argv)
{
   if (!strcmp(command, "help"))
   {
       help(shell, argc, argv);
   }
   else if (!strcmp(command, "rm"))
   {
       rm(shell, argc, argv);
   }
   else if (!strcmp(command, "cp"))
   {
       cp(shell, argc, argv);
   }
   else if (!strcmp(command, "mv"))
   {
       mv(shell, argc, argv);
   }
   else if (!strcmp(command, "cat"))
   {
       cat(shell, argc, argv);
   }
   else if (!strcmp(command, "mkdir"))
   {
       mkdir(shell, argc, argv);
   }
   else if (!strcmp(command, "rmdir"))
   {
       rmdir(shell, argc, argv);
   }
   else if (!strcmp(command, "system"))
   {
       bbx_fs_system(shell, argc, argv);
   }
   else if (!strcmp(command, "import"))
   {
       import(shell, argc, argv);
   }
   else if (!strcmp(command, "export"))
   {
       export(shell, argc, argv);
   }
   else if (!strcmp(command, "edit"))
   {
       char *exportargs[4];
       char *importargs[4];
       char buff[1024];
       if (argc == 2)
       {
           exportargs[0] = "export";
           exportargs[1] = argv[1];
           exportargs[2] = argv[1];
           exportargs[3] = 0;
           export(shell, 3, exportargs);
           snprintf(buff, 1024, "nano %s\n", argv[1]);
           system(buff);
           importargs[0] = "import";
           importargs[1] = argv[1];
           importargs[2] = argv[1];
           importargs[3] = 0;
           import(shell, 3, importargs);
       }
   }
   else if (!strcmp(command, "bb"))
   {
       babybasic(shell, argc, argv);
   }
    else
    {
        fprintf(shell->stderr, "unknown command %s\n", command);
    }
    
    return  0;;
       
}

static int listcommands(BBX_FS_SHELL *shell)
{
    BBX_FS_COMMAND *cmd;
    
    fprintf(shell->stdout, "cd\n");
    fprintf(shell->stdout, "ls\n");
    fprintf(shell->stdout, "help\n");
    fprintf(shell->stdout, "rm\n");
    fprintf(shell->stdout,"cp\n");
    fprintf(shell->stdout, "mv\n");
    fprintf(shell->stdout, "cat\n");
    fprintf(shell->stdout, "mkdir\n");
    fprintf(shell->stdout, "rmdir\n");
    fprintf(shell->stdout, "system\n");
    fprintf(shell->stdout, "import\n");
    fprintf(shell->stdout, "export\n");
    fprintf(shell->stdout, "edit\n");
    fprintf(shell->stdout, "bb\n");
    
    cmd = shell->commands;
    while (cmd)
    {
        fprintf(shell->stdout, "%s\n", cmd->name);
        cmd = cmd->next;
    }
}
static char **getargs(char *str)
{
    int N = 0;
    int onspace = 1;
    int i;
    char *arg;
    char **answer = 0;
    
    for (i = 0; str[i]; i++)
    {
        if (onspace && !isspace(str[i]))
        {
            onspace = 0;
            N++;
        }
        else if(!onspace && isspace(str[i]))
            onspace = 1;
    }
    answer = bbx_malloc((N + 1) * sizeof(char *));
    
    N = 0;
    for (i = 0; str[i]; i++)
    {
        if (onspace && !isspace(str[i]))
        {
            answer[N] = str + i;
            onspace = 0;
            N++;
        }
        else if(!onspace && isspace(str[i]))
        {
            str[i] = 0;
            onspace = 1;
        }
    }
    answer[N] = 0;
    for (i=0; i < N; i++)
        answer[i] = bbx_strdup(answer[i]);
    
    return  answer;
    
    
    
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

static int help(BBX_FS_SHELL *shell, int argc, char **argv)
{
    BBX_Options *bbx_opt;
    int list = 0;
    int Nargs = 0;
    
    bbx_opt = bbx_options(argc, argv, "");
    list = bbx_options_get(bbx_opt, "-list", 0);
    Nargs = bbx_options_Nargs(bbx_opt);
    bbx_options_kill(bbx_opt);
    bbx_opt = 0;
    
    if (Nargs > 0)
    {
        fprintf(shell->stdout, "%s", help_usage);
        return 0;
    }
    
    if (list)
    {
        listcommands(shell);
        return 0;
    }
    
    fprintf(shell->stdout, "Baby X FileSystem shell\n");
    fprintf(shell->stdout,"\n");
    fprintf(shell->stdout, "Explore this FileSystem XML file with the shell\n");
    fprintf(shell->stdout, "Commands:\n");
    fprintf(shell->stdout, "\tcd - change directory\n");
    fprintf(shell->stdout, "\tls - list files\n");
    fprintf(shell->stdout, "\tcp - copy a file\n");
    fprintf(shell->stdout, "\trm - delete file\n");
    fprintf(shell->stdout, "\tcat - print out file contents\n");
    fprintf(shell->stdout, "\timport - read file from host\n");
    fprintf(shell->stdout, "\texport - write file to host\n");
    fprintf(shell->stdout, "\tsystem - host operating system command\n");
    fprintf(shell->stdout, "\tbb - Baby Basic\n");
    fprintf(shell->stdout, "\n");
    fprintf(shell->stdout, "\tquit - exit the shell\n");
    fprintf(shell->stdout, "\n");
    
    return  0;
}


static int cp(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char *source;
    char *target;
    char path[1024];
    unsigned char *data;
    FILE *fp;
    int N;
    int i;

    if (argc != 3)
    {
        fprintf(shell->stderr, "%s", cp_usage);
        return 0;
    }
    source = argv[1];
    target = argv[2];
    
    snprintf(path, 1024, "%s/%s", shell->path, source);
    data = bbx_filesystem_slurpb(shell->bbx_fs, path, "rb", &N);
    
    if (!data)
        return;
    snprintf(path, 1024, "%s/%s", shell->path, target);
    fp = bbx_filesystem_fopen(shell->bbx_fs, path, "w");
    if (data && fp)
    {
        for (i = 0; i < N; i ++)
            fputc(data[i], fp);
    }
    
    free(data);
    bbx_filesystem_fclose(shell->bbx_fs, fp);
    
    return  0;
}

static int rm(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char *target;
    char path[1024];

    if (argc != 2)
        return 0;
    target = argv[1];
    
    snprintf(path, 1024, "%s/%s", shell->path, target);
    bbx_filesystem_unlink(shell->bbx_fs, path);
    
    return  0;
}

static int mv(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char *target;
    char *source;
    char path[1024];
    FILE *fp = 0;
    unsigned char *data;
    int len;
    
    if (argc != 3)
        return 0;
    
    target = argv[2];
    source = argv[1];
    
    snprintf(path, 1024, "%s/%s", shell->path, source);
    data = bbx_filesystem_slurpb(shell->bbx_fs, path, "r", &len);
    if (!data)
        return 0;
    
    snprintf(path, 1024, "%s/%s", shell->path, target);
    fp = bbx_filesystem_fopen(shell->bbx_fs, path, "w");
    if (!fp)
    {
        free(data);
        return 0;
    }
    fwrite(data, 1, len, fp);
    bbx_filesystem_fclose(shell->bbx_fs, fp);
    
    snprintf(path, 1024, "%s/%s", shell->path, source);
    bbx_filesystem_unlink(shell->bbx_fs, path);
    
    free (data);
    
    return  0;
}

static int cat(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char *target;
    char path[1024];
    unsigned char *data;
    int N;
    int i;
    
    if (argc != 2)
        return 0;

    target = argv[1];
    snprintf(path, 1024, "%s/%s", shell->path, target);
    
    data = bbx_filesystem_slurpb(shell->bbx_fs, path, "rb", &N);
    if (data)
    {
        for (i = 0; i < N; i ++)
            fputc(data[i], shell->stdout);
    }
    
    free(data);
    
    return  0;
}

static int mkdir(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char *target;
    char path[1024];

    if (argc != 2)
        return 0;
    target = argv[1];
    
    snprintf(path, 1024, "%s/%s", shell->path, target);
    bbx_filesystem_mkdir(shell->bbx_fs, path);
    
    return  0;
}

static int rmdir(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char *target;
    char path[1024];

    if (argc != 2)
        return 0;
    target = argv[1];
    
    snprintf(path, 1024, "%s/%s", shell->path, target);
    bbx_filesystem_rmdir(shell->bbx_fs, path);
    
    return  0;
}

static int bbx_fs_system(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char line[1024];
    int i;
    
    strcpy(line, "");
    
    for (i = 1; i < argc; i++)
    {
        strncat(line, argv[i], 1024);
        strncat(line, " ", 1024);
    }
    
    system(line);
    
    return 0;
}

static int import(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char path[1024];
    char *target;
    FILE *fpin;
    FILE *fpout;
    int ch;
    int i;
    
    if (argc != 3)
        return 0;
    
    fpin = fopen(argv[1], "r");
    if (!fpin)
    {
        fprintf(shell->stderr, "Can't open %s on host\n", argv[1]);
        return 0;
    }
    
    target = argv[2];
    snprintf(path, 1024, "%s/%s", shell->path, target);
    fpout = bbx_filesystem_fopen(shell->bbx_fs, path, "w");
    if (fpout)
    {
        while ( (ch = fgetc(fpin)) != EOF)
            if (fputc(ch, fpout) == EOF)
                break;
    }
    bbx_filesystem_fclose(shell->bbx_fs, fpout);
    fclose(fpin);
    
    return 0;
}

static int export(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char path[1024];
    char *source;
    unsigned char *data;
    int N;
    FILE *fpout;
    int i;
    
    if (argc != 3)
        return 0;
    
    source = argv[1];
    snprintf(path, 1024, "%s/%s", shell->path, source);
    data = bbx_filesystem_slurpb(shell->bbx_fs, path, "rb", &N);
    if (!data)
        return 0;
    
    fpout = fopen(argv[2], "w");
    if (fpout)
    {
        for (i = 0; i < N; i++)
            fputc(data[i], fpout);
        fclose(fpout);
    }
    else
        fprintf(shell->stderr, "Can't open %s on host\n", argv[2]);
    free(data);
    
    return 0;
}


static int babybasic(BBX_FS_SHELL *shell, int argc, char **argv)
{
    char *target;
    FILE *fp;
    char *script;
    char path[1024];
    
    if (argc != 2)
        return 0;
    target = argv[1];
    snprintf(path, 1024, "%s/%s", shell->path, target);
    script = bbx_filesystem_slurp(shell->bbx_fs, path, "r");
    if (script)
        basic(script, shell->stdin, shell->stdout, shell->stderr);
    free(script);
    
    return 0;
}
