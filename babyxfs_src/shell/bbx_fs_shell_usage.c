/*How to use the cd command*/
char cd_usage[] = "\n"
"cd - change directory\n"
"Usage: cd <target directory>\n"
" use cd .. to move up.\n";
/*How to use the help command*/
char help_usage[] = "\n"
"heel - get help\n"
"Usage: help [options] [command]\n"
"  Options: -list - list all commands\n"
"  use help command for help on a command.\n"
"\n";
/*How to use the rm command*/
char rm_usage[] = "\n"
"rm - remove a file\n"
"Usage: rm <target file>\n"
" \n";
/*How to use the cp command*/
char cp_usage[] = "\n"
"cp - copy a file\n"
"Usage: cp <source file> <target file>\n"
"\n";
/*How to use the mv command*/
char mv_usage[] = "\n"
"mv move a file\n"
"Usage: mv <source file> <target file>\n"
"\n";
/*How to use the cat command*/
char cat_usage[] = "\n"
"cat - print of a file\n"
"Usage: cat <source file>\n"
"\n";
/*How to use the mkdir command*/
char mkdir_usage[] = "\n"
"mkdir - make a directory\n"
"Usage: mkdir <target directory>\n"
"\n";
/*How to use the rmdir command*/
char rmdir_usage[] = "\n"
"rmdir - remove directory\n"
"Usage: rmdir <target directory>\n"
" \n";
/*How to use the system command*/
char system_usage[] = "\n"
"system - call a system command on host operating system\n"
"Usage: system <host command> ...\n"
" usually works, can go horribly wrong\n";
/*How to use the import command*/
char import_usage[] = "\n"
"import - read a file a file to host\n"
"Usage: cd <host source> <target file>\n"
" \n";
/*How to use the export command*/
char export_usage[] = "\n"
"export - write a file to the host\n"
"Usage: export <source file> <host target>\n"
" \n";
/*How to use the edit coomand*/
char edit_usage[] = "\n"
"edit - edit a file\n"
"Usage: edit <source file>\n"
" Note, we have to call an editor on the host.\n"
"It is not possible to write an interactive text editor in\n"
"portable ANSI C. This command might go horribly wrong.\n";
/*How to use the bb coomand*/
char bb_usage[] = "\n"
"bb - Baby Basic\n"
"Usage: cd <basicfile.bas>\n"
"   BabyBasic is the BabyXF Shell\'s scripting language.\n";
