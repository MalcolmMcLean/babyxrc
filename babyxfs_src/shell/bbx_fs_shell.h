//
//  bbx_fs_shell.h
//  
//
//  Created by Malcolm McLean on 04/06/2024.
//

#ifndef bbx_fs_shell_h
#define bbx_fs_shell_h

#include <stdio.h>
#include "bbx_filesystem.h"

typedef struct bbx_fs_shell BBX_FS_SHELL;

BBX_FS_SHELL *bbx_fs_shell(BBX_FileSystem *bbx_fs);
int bbx_fs_shell_run(BBX_FS_SHELL *shell, FILE *output, FILE *input, FILE *error);
void bbx_fs_shell_kill(BBX_FS_SHELL *shell);
FILE *bbx_fs_shell_fopen(BBX_FS_SHELL *shell, const char *path, const char *mode);
int bbx_fs_shell_fclose(BBX_FS_SHELL *shell, FILE *fp);
int bbx_fs_shell_addcommand(BBX_FS_SHELL *shell, const char *command,
                            int (*fptr)(int argc, char **argv, FILE *out, FILE *in, FILE *err, BBX_FS_SHELL *shell, void *ptr), void *ptr);
int bbx_fs_shell_set_editor(BBX_FS_SHELL *shell, const char *editor);

#endif /* bbx_fs_shell_h */
