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

BBX_FS_SHELL *bbx_fs_shell(BBX_FileSystem *bbx_fs, FILE *output, FILE *error);
void bbx_fs_shell_kill(BBX_FS_SHELL *shell);
int bbx_fs_shell_inputline(BBX_FS_SHELL *shell, const char *line);

#endif /* bbx_fs_shell_h */
