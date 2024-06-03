#ifndef bbx_options_h
#define bbx_options_h

typedef struct bbx_options BBX_Options;

BBX_Options *bbx_options(int argc, char **argv, char *flags);
void bbx_options_kill(BBX_Options *opt);
int bbx_options_get(BBX_Options *opt, char *name, char *fmt, ...);
int bbx_options_error(BBX_Options *opt, char *errormessage, int Nerror);
int bbx_options_Nargs(BBX_Options *opt);
char *bbx_options_arg(BBX_Options *opt, int index);

#endif
