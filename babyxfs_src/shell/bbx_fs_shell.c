//
//  bbx_fs_shell.c
//  
//
//  Created by Malcolm McLean on 04/06/2024.
//

#include "bbx_fs_shell.h"
#include "bbx_options.h"
#include "strnatcmp.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "basic.h"

extern char cp_usage[];
extern char help_usage[];
extern char rm_usage[];
extern char mv_usage[];
extern char cat_usage[];
extern char mkdir_usage[];
extern char rmdir_usage[];
extern char system_usage[];
extern char export_usage[];
extern char import_usage[];
extern char bb_usage[];

extern char help_on_bb[];
extern char help_on_cat[];
extern char help_on_cd[];
extern char help_on_cp[];
extern char help_on_edit[];
extern char help_on_export[];
extern char help_on_help[];
extern char help_on_import[];
extern char help_on_ls[];
extern char help_on_mkdir[];
extern char help_on_mv[];
extern char help_on_rm[];
extern char help_on_rmdir[];
extern char help_on_system[];


typedef struct bbx_fs_command
{
    char *name;
    int (*fptr)(int argc, char **argv, FILE *out, FILE *in, FILE *err, struct bbx_fs_shell *shell, void *ptr);
    void *ptr;
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
    char *editor;
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

static int ls(BBX_FS_SHELL *shell, int argc, char **argv);

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
    shell->editor = bbx_strdup("nano");
    
    return shell;
}

void bbx_fs_shell_kill(BBX_FS_SHELL *shell)
{
   if (shell)
   {
       free(shell->editor);
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
                            int (*fptr)(int argc, char **argv, FILE *out, FILE *in, FILE *err, BBX_FS_SHELL *shell, void *ptr), void *ptr)
{
    BBX_FS_COMMAND *bbx_command;
    
    bbx_command = bbx_malloc(sizeof(BBX_FS_COMMAND));
    bbx_command->fptr = fptr;
    bbx_command->ptr = ptr;
    bbx_command->name = bbx_strdup(command);
    bbx_command->next = shell->commands;
    shell->commands = bbx_command;
}

FILE *bbx_fs_shell_fopen(BBX_FS_SHELL *shell, const char *path, const char *mode)
{
    char fspath[1024];
    snprintf(fspath, 1024, "%s/%s", shell->path, path);
    
    return bbx_filesystem_fopen(shell->bbx_fs, fspath, mode);
}

int bbx_fs_shell_fclose(BBX_FS_SHELL *shell, FILE *fp)
{
    return bbx_filesystem_fclose(shell->bbx_fs, fp);
}

int bbx_fs_shell_set_editor(BBX_FS_SHELL *shell, const char *editor)
{
    if (editor)
    {
        free(shell->editor);
        shell->editor = bbx_strdup(editor);
        return 0;
    }
    
    return  -1;
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
    /*
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
     */
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
    int didrun;
    
    didrun = run_external_command(shell, command, argc, argv);
    if (!didrun)
        run_internal_command(shell, command, argc, argv);
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
            (*cmd->fptr)(argc, argv,shell->stdout, shell->stdin, shell->stderr, shell, cmd->ptr);
            return 1;
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
   else if (!strcmp(command, "ls"))
   {
       ls(shell, argc, argv);
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
           char *tempfile = 0;
           tempfile = tmpnam(0);
           if (tempfile)
           {
               exportargs[0] = "export";
               exportargs[1] = argv[1];
               exportargs[2] = tempfile;
               exportargs[3] = 0;
               export(shell, 3, exportargs);
               snprintf(buff, 1024, "%s %s\n", shell->editor, tempfile);
               system(buff);
               importargs[0] = "import";
               importargs[1] = tempfile;
               importargs[2] = argv[1];
               importargs[3] = 0;
               import(shell, 3, importargs);
           }
           /* safer not to free(tempfile), it could  retutn anything */
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
    char * command = 0;
    int list = 0;
    int Nargs = 0;
    
    bbx_opt = bbx_options(argc, argv, "");
    list = bbx_options_get(bbx_opt, "-list", 0);
    Nargs = bbx_options_Nargs(bbx_opt);
    if (Nargs == 1)
        command = bbx_options_arg(bbx_opt, 0);
    bbx_options_kill(bbx_opt);
    bbx_opt = 0;
    
    if (Nargs > 1)
    {
        fprintf(shell->stdout, "%s", help_usage);
        return 0;
    }
    if (Nargs == 1)
    {
        if (!strcmp(command, "bb"))
            fprintf(shell->stdout, "%s\n", help_on_bb);
        if (!strcmp(command, "cat"))
            fprintf(shell->stdout, "%s\n", help_on_cat);
        if (!strcmp(command, "cd"))
            fprintf(shell->stdout, "%s\n", help_on_cd);
        if (!strcmp(command, "cp"))
            fprintf(shell->stdout, "%s\n", help_on_cp);
        if (!strcmp(command, "edit"))
            fprintf(shell->stdout, "%s\n", help_on_edit);
        if (!strcmp(command, "export"))
            fprintf(shell->stdout, "%s\n", help_on_export);
        if (!strcmp(command, "help"))
            fprintf(shell->stdout, "%s\n", help_on_help);
        if (!strcmp(command, "import"))
            fprintf(shell->stdout, "%s\n", help_on_import);
        if (!strcmp(command, "ls"))
            fprintf(shell->stdout, "%s\n", help_on_ls);
        if (!strcmp(command, "mkdir"))
            fprintf(shell->stdout, "%s\n", help_on_mkdir);
        if (!strcmp(command, "mv"))
            fprintf(shell->stdout, "%s\n", help_on_mv);
        if (!strcmp(command, "rm"))
            fprintf(shell->stdout, "%s\n", help_on_rm);
        if (!strcmp(command, "rmdir"))
            fprintf(shell->stdout, "%s\n", help_on_rmdir);
        if (!strcmp(command, "system"))
            fprintf(shell->stdout, "%s\n", help_on_system);
        
        return  0;;
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
    fprintf(shell->stdout, "\thelp help - help on using help\n");
    fprintf(shell->stdout, "\thelp -list - list all external commands\n");
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
        return 0;
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
    {
        fprintf(shell->stdout, "%s", rm_usage);
        return 0;
    }
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
    {
        fprintf(shell->stdout, "%s\n", mv_usage);
        return 0;
    }
    
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
    {
        fprintf(shell->stdout, "%s", cat_usage);
        return 0;
    }

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
    {
        fprintf(shell->stdout, "%s", mkdir_usage);
        return 0;
    }
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
    {
        fprintf(shell->stdout, "%s", rmdir_usage);
        return 0;
    }
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
    
    if (argc <= 1)
    {
        fprintf(shell->stdout, "%s", system_usage);
    }
    for (i = 1; i < argc; i++)
    {
        strncat(line, argv[i], 1023);
        strncat(line, " ", 1023);
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
    {
        fprintf(stdout, "%s", import_usage);
        return 0;
    }
    
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
    {
        fprintf(shell->stdout, "%s", export_usage);
        return 0;
    }
    
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
    {
        fprintf(stdout, "%s", bb_usage);
        return 0;
    }
    target = argv[1];
    snprintf(path, 1024, "%s/%s", shell->path, target);
    script = bbx_filesystem_slurp(shell->bbx_fs, path, "r");
    if (script)
        basic(script, shell->stdin, shell->stdout, shell->stderr);
    free(script);
    
    return 0;
}


/*
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
 */

static int getsize(BBX_FS_SHELL *shell, const char *fname)
{
    FILE *fp;
    long pos;
    
    fp = bbx_filesystem_fopen(shell->bbx_fs, fname, "r");
    if (!fp)
        return 0;
    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    bbx_filesystem_fclose(shell->bbx_fs, fp);
    
    return (int) pos;
}

static int is_binary(BBX_FS_SHELL *shell, const char *name)
{
    FILE *fp;
    int ch;
    int answer = 0;
    
    fp = bbx_filesystem_fopen(shell->bbx_fs, name, "r");
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
    
    bbx_filesystem_fclose(shell->bbx_fs, fp);
    
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

static void ls_usage()
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
    fprintf(stderr, "\t\t\t\"alpha\" alphabetical sort\n");
    fprintf(stderr, "\t\t\t\"ext\" sort by extension\n");
    fprintf(stderr, "\t\t\t\"none\" don't sort (OS order)\n");
}

static int ls(BBX_FS_SHELL *shell, int argc, char **argv)
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
    for (i = 0; i <Nargs; i++)
      //  printf("-%s\n", bbx_options_arg(bbx_opt,i));
    if (bbx_options_error(bbx_opt, errormessage, 1024))
    {
        err = 1;
        fprintf(stderr, "%s\n", errormessage);
    }
    if (Nargs > 1 && err == 0)
    {
        err = 1;
        ls_usage();
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
        char fs_path[1024];
        snprintf(fs_path, 1024, "%s/%s", shell->path, path);
        dir = bbx_filesystem_list(shell->bbx_fs, fs_path);
        if (!dir)
        {
            FILE *fp;
            int fileexists;
            
            fp = bbx_filesystem_fopen(shell->bbx_fs, fs_path, "r");
            if (fp)
                fileexists = 1;
            bbx_filesystem_fclose(shell->bbx_fs, fp);
            
            dir = bbx_malloc(sizeof(char *) * 2);
            if (fileexists)
                dir[0] = bbx_strdup(basename(path));
            else
                dir[0] = 0;
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
    }
    else
        dir = bbx_filesystem_list(shell->bbx_fs, shell->path);
    
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
            char fname[1024];
            
            if(!strcmp(path, "."))
                snprintf(fname, 1024, "%s/%s", shell->path, dir[i]);
            else
            {
                snprintf(fname, 1024, "%s/%s/%s", shell->path, path, dir[i]);
                //fname = filename(path, dir[i]);
                //if (!fname)
                  //  return - 1;
            }
            
            directory[i].name = dir[i];
            if (!is_directory(directory[i].name))
            {
                directory[i].size = getsize(shell, fname);
                directory[i].isbinary = is_binary(shell, fname);
            }
            else
            {
                directory[i].size = 0;
                directory[i].isbinary = 0;
            }
            //free(fname);
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
  
