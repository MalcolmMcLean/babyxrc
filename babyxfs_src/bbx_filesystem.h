//
//  bbx_filesystem.h
//  babyxfs
//
//  Created by Malcolm McLean on 31/05/2024.
//

#ifndef bbx_filesystem_h
#define bbx_filesystem_h

#define BBX_FS_STDIO 1
#define BBX_FS_STRING 2

typedef struct bbx_filesystem BBX_FileSystem;

BBX_FileSystem *bbx_filesystem(void);
void bbx_filesystem_kill(BBX_FileSystem *bbx_fs);
int bbx_filesystem_set(BBX_FileSystem *bbx_fs, const char *pathorxml, int mode);
FILE *bbx_filesystem_fopen(BBX_FileSystem *bbx_fs, const char *path, const char *mode);
int bbx_filesystem_fclose(BBX_FileSystem *bbx_fs, FILE *fp);
char *bbx_filesystem_slurp(BBX_FileSystem *bbx_fs, const char *path, const char *mode);
unsigned char *bbx_filesystem_slurpb(BBX_FileSystem *bbx_fs, const char *path, const char *mode, int *N);
int bbx_filesystem_unlink(BBX_FileSystem *bbx_fs, const char *path);
const char *bbx_filesystem_getname(BBX_FileSystem *bbx_fs);
int bbx_filesystem_setreadir(BBX_FileSystem *bbx_fs, char **(*fptr)(const char *path, void *ptr), void *ptr);
int bbx_filesystem_dump(BBX_FileSystem *bbx_fs, FILE *fp);
char **bbx_filesystem_mkdir(BBX_FileSystem *bbx_fs, const char *path);
char **bbx_filesystem_rmdir(BBX_FileSystem *bbx_fs, const char *path);
char **bbx_filesystem_list(BBX_FileSystem *bbx_fs, const char *path);

#endif /* bbx_filesystem_h */
