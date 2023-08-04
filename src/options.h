#ifndef options_h
#define options_h

typedef struct
{
  int argc;             /* number of arguments */
  char **argv;          /* argument list */
  char *flags;          /* set of allowed one char flags */
  int *used;            /* flag for each argument read */
  int firsttouched;     /* set if the first arguement interpreted as flags */
  int firstflag;        /* set if the first arguement can be flags */
  int *firstused;       /* set if each character of first arguement read */
  int error;            /* set if the parser encounters an error */
  char errstr[1024];    /* parser error text */
} OPTIONS;

OPTIONS *options(int argc, char **argv, char *flags);
void killoptions(OPTIONS *opt);
int opt_get(OPTIONS *opt, char *name, char *fmt, ...);
int opt_error(OPTIONS *opt, FILE *fp);
int opt_Nargs(OPTIONS *opt);
char *opt_arg(OPTIONS *opt, int index);

#endif
