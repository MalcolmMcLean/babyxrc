/*
  version 1.1 null flags bug fixed.
  version 1.2 bug fixed with trailing options without args
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

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

static void checkunusedoptions(OPTIONS *opt);
static int parseoptionsv(OPTIONS *opt, int idx, char *fmt, va_list ret);
static void seterror(OPTIONS *opt, char *fmt, ...);
static int canbeflags(char *argv1, char *flags);
static int firstopt(char *argv1, char *name);
static int contains(char *name, char *opt);
static int oneletteropt(char *name);
static int empty(const char *str);
static char **dupargs(char **argv);
static char *mystrdup(const char *str);

/*
  options object constructor
  Params: argc - number of arguments 
          argv - command line (terminating null pointer)
          flags - list of legitimate 1 character flags
  Returns: constructed object
  Notes:
    assumes a comandline of the form

      programname -options -longoption -param 5 filename1 filename2

  The first argument can be a plain argument, can be a long option
    or can be a list of flags introduced with '-'.
 
 */
OPTIONS *options(int argc, char **argv, char *flags)
{
    OPTIONS *answer;
    int i;

    answer = malloc(sizeof(OPTIONS));
    if (!answer)
        goto error_exit;
    answer->argc = argc;
    answer->argv = 0;
    answer->used = 0;
    answer->firstused = 0;
    answer->firsttouched = 0;
    answer->error = 0;
    answer->errstr[0] = 0;
    answer->flags = 0;

    answer->argv = dupargs(argv);
    if(!answer->argv)
        goto error_exit;

    answer->used = malloc(argc * sizeof(int));
    if (!answer->used)
        goto error_exit;
    if (argc > 1 && argv[1])
    {
        answer->firstused = malloc(strlen(argv[1]) * sizeof(int) );
        if (!answer->firstused)
            goto error_exit;
        memset(answer->firstused, 0, strlen(argv[1]) * sizeof(int));
    }
  
    if (flags)
    {
        answer->flags = mystrdup(flags);
        if (!answer->flags)
            goto error_exit;
    }
    else
        answer->flags = 0;
  
    answer->firstflag = canbeflags(argv[1], flags);

    for(i=0;i<argc;i++)
        answer->used[i]  = 0;
    return answer;

 error_exit:
    killoptions(answer);
    return 0;
}

/*
  options object destructor
 */
void killoptions(OPTIONS *opt)
{
    int i;

    if (opt)
    {
        if (opt->argv)
        {
            for (i=0;i<opt->argc;i++)
                free(opt->argv[i]);
            free(opt->argv);
        }
        free(opt->flags);
        free(opt->firstused);
        free(opt->used);
        free(opt);
    }
}

/*
  get an option form the command line
  Params: opt - the options
          name - name of option
          fmt - option arguements format
  Returns: number of arguments consumed.
  Notes:
    name - "-option -o -opt -OPT" - list of alternative names
    fmt - "%d%32s%f" - list of option parameters
          %d - an integer - argument int *
          %f - a real - argument double *
          %s - a string - argument char *
      strings should take a buffer length qualifer (default 256)
   
  usage 
    if(opt_get(opt, "-f"))
      f_flagisset();

    opt_get("-dimensions -d", "%d%d", &width, &height)

    Note that there is usually no need to error check. opt_error
      will report badly-formed parameters.
 */
int opt_get(OPTIONS *opt, char *name, char *fmt, ...)
{
    int i;
    int j;
    va_list ret;
    int answer = 0;
    int idx;

    if (!opt || opt->argc < 2)
        return 0;

    va_start(ret, fmt);
    if (opt->firstflag && firstopt(opt->argv[1], name) && empty(fmt) )
    {
        idx = strchr(opt->argv[1], oneletteropt(name)) - opt->argv[1];
        if (opt->firstused[idx])
            seterror(opt, "Duplicate option -%c", oneletteropt(name));
        opt->firstused[idx] = 1;
        opt->firsttouched = 1;
        opt->used[1] = 1;
        answer = 1;
    }
    else
    {
        for (i=1;i<opt->argc;i++)
        {
            if(contains(name, opt->argv[i]))
            {
                answer = parseoptionsv(opt, i, fmt, ret) + 1;
                for (j=i;j<i+answer;j++)
                {
                    if (opt->used[j])
                        seterror(opt, "Bad options string %s", opt->argv[j]);
                    opt->used[j] = 1;
                }
                break;
            }
        }
    }
    va_end(ret);

    return answer;
}

/*
  check for errors on the commandline
  Params: opt - the options object
          fp - pointer to a stream for error message
  Returns: 0 if no error, 1 if errors
  Notes:
    Caller must parse every option with opt_get()to prevent it being 
    flagged as an unrecognised option.
    No need to check for null OPTION objects - code is null safe
    and opt_error returns an out of memory error.
  
 */
int opt_error(OPTIONS *opt, FILE *fp)
{
    if (!opt)
    {
        if (fp)
            fprintf(fp, "Out of memory\n");
        return -1;
    }
    checkunusedoptions(opt);
    if (opt->error && fp)
        fprintf(fp, "%s\n", opt->errstr);

    return opt->error;
}

/*
  get the number of arguments
  Params: opt - the options object
  Returns: number of commandline arguments that are not options
  Notes: must call after you have finished option parsing.
 */
int opt_Nargs(OPTIONS *opt)
{
    int answer = 0;
    int i;

    if (!opt)
        return 0;
    for (i=opt->argc-1;i >= 1; i--)
    {
        if (!opt->used[i])
            answer++;
        else
            break;
    }

    return answer;
}

/*
  extract an argument form the command line
  Params: opt - the options object
          index - zero-based option index
  Returns: argument, or NULL on fail
 */
char *opt_arg(OPTIONS *opt, int index)
{
    char *answer;
    int N;

    if (!opt)
        return 0;
    N = opt_Nargs(opt);
    if (index >= N)
        return 0;
    answer = mystrdup(opt->argv[opt->argc - N + index]);
    if (!answer)
        seterror(opt, "Out of memory\n");
    return answer;
}

/*
  check for any unparsed options and report them
 */
static void checkunusedoptions(OPTIONS *opt)
{
    int i;
    int N;

    N = opt_Nargs(opt);
    for (i=1;i<opt->argc - N; i++)
        if (!opt->used[i])
            seterror(opt, "Unrecognised or duplicate option %s.", opt->argv[i]);
  /* this can happen if caller does not parse all flags */
    if (opt->firsttouched)
    {
        for (i=1;opt->argv[1][i];i++)
            if (!opt->firstused[i])
                seterror(opt, "Illegal flag -%c\n", opt->argv[1][i]);
    }

  /* now check for illegal flags */
    if (N > 0 && N == opt->argc - 1 && opt->argv[1][0] == '-' && opt->flags)
    {
        for (i=1;opt->argv[1][i];i++)
            if (!strchr(opt->flags, opt->argv[1][i]))
                seterror(opt, "Unrecognised flag -%c\n", opt->argv[1][i]);
    }
 
  /* check if the first argument looks like an option */
    if (N > 0 && opt->argv[opt->argc-N][0] == '-')
        seterror(opt, "Unrecognised or duplicate option %s.", opt->argv[opt->argc-N]);
}

/*
  parse an option
  Params: opt - the OPTIONS object
          idx - index of option
          fmt - scanf-like format string
          ret - return pointers
  Returns: number of parameters parsed
 */
static int parseoptionsv(OPTIONS *opt, int idx, char *fmt, va_list ret)
{
    char *ptr = fmt;
    int answer = 0;
    char *sptr;
    int *iptr;
    double *fptr;
    char *end;
    long len;
    long ival;

    if (!fmt)
        return 0;

    while (*ptr)
    {
        if (*ptr == '%')
        {
            len = 0;
            if (isdigit(ptr[1]))
            {
                len = strtol(ptr + 1, &end, 10);
                ptr = end-1;
            }
            switch (ptr[1])
            {
                case 's':
                    sptr = va_arg(ret, char *);
                    if (len == 0)
                        len = 256;
                    if (idx + answer + 1 >= opt->argc)
                    {
                        seterror(opt, "Option %s expects an argument\n", opt->argv[idx]);
                        return 0;
                    }
                    if (strlen(opt->argv[idx + answer + 1]) < len)
                        strcpy(sptr, opt->argv[idx + answer + 1]);
                    else
                    {
                        seterror(opt, "Option %s argument too long\n", opt->argv[idx]);
                        return 0;
                    }
                    break;
                case 'd':
                    iptr = va_arg(ret, int *);
                    if (idx + answer + 1 >= opt->argc)
                    {
                        seterror(opt, "Option %s expects an integer argument\n", opt->argv[idx]);
                        return 0;
                    }
        
                    ival = strtol(opt->argv[idx + answer + 1], &end, 10);
                    if (ival < INT_MIN || ival > INT_MAX || ival == LONG_MIN || ival == LONG_MAX)
                    {
                        seterror(opt, "Option %s integer out of range\n", opt->argv[idx]);
                        return 0;
                    }
                    *iptr = (int) ival;
                    if (*end)
                    {
                        seterror(opt, "Option %s must be an integer\n", opt->argv[idx]);
                        return 0;
                    }
                    break;
                case 'f':
                    fptr = va_arg(ret, double *);
                    if (idx + answer + 1 >= opt->argc)
                    {
                        seterror(opt, "Option %s expects a numerical argument\n", opt->argv[idx]);
                        return 0;
                    }
        
                    *fptr = strtod(opt->argv[idx + answer + 1], &end);
                    if (*end)
                    {
                        seterror(opt, "Option %s must be a number\n", opt->argv[idx]);
                        return 0;
                    }
                    break;
                default:
                    assert(0);
            }
            ptr+=2;
            answer++;
        }
        else
            assert(0);
    }

  return answer;
} 

/*
  report an error condition
  Params: opt - the OPTIONS object
          fmt - fprintf style format string
  Notes: only first error reported to user
 */
static void seterror(OPTIONS *opt, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!opt->error)
    {
        vsnprintf(opt->errstr, 1024, fmt, ap);
        opt->error = 1;
    }
    va_end(ap);

}

/*
  test if the first argument can be flags
  Params: argv1 - the first commandline argument, argv[1]
          flags - set of allowed falgs introduced with '-'
  Returns: true if a possible options string
  Notes: there is a difficulty if any long option is also a legal
         set of flags.
 */
static int canbeflags(char *argv1, char *flags)
{
    int i;

    if (!argv1)
        return 0;
    if (argv1[0] == 0)
        return 0;
    if (!flags)
        return 0;
    if (flags[0] == 0)
        return 0;

    if (argv1[0] != '-')
        return 0;

    for (i=0;argv1[i];i++)
    {
        if (!strchr(flags, argv1[i]))
            return 0;
        if (strchr(argv1 + i + 1, argv1[i]))
            return 0;
    }

    return 1;
}
 
/*
  get the first option
  Params: argv1 - the first command line argument
          name - the flags names
  Returns: 1 if the command line contains the flag in the
     name strign, else 0
 */
static int firstopt(char *argv1, char *name)
{
    int ch;

    if (!argv1)
        return 0;
    ch = oneletteropt(name);
    if (ch && argv1[0] == '-' && strchr(argv1, ch))
        return 1;
    return 0;
}

/*
  does the list of aliases contain the command line option
  Params: name - list of aliases for option 
          opt - option user typed
  Returns: 1 if the option matches the list
 */
static int contains(char *name, char *opt)
{
    size_t optlen;
    int i;
   
    if (!strcmp(name, opt))
        return 1;
    optlen = strlen(opt);
    if (!strncmp(name, opt, optlen) && isspace((unsigned char)name[optlen]))
        return 1;
  
    for (i = 0; name[i]; i++)
    {
        if (isspace((unsigned char) name[i]))
        {
            if (!strncmp(name + i + 1, opt, optlen) && (isspace((unsigned char) name[i+optlen+1])
              || name[i+optlen+1] == 0))
              return 1;
        }
    }
    
    return 0;
}

/*
  Extract one letter flag option from option list
  Params: name - list of option aliases
  Returns: character of the one letter option.
  Notes: should pass only one one letter alias
 */
static int oneletteropt(char *name)
{
    char *ptr;
  
    ptr = name;
    while ((ptr = strchr(ptr, '-')))
    {
      if (isalnum(ptr[1]) && (isspace(ptr[2]) || ptr[2] == 0) )
          return ptr[1];
      ptr++;
    }
    
    return 0;
}

/*
  duplicate the argument string
  Params: argv - null-termianted list of strings
  Returns: malloced list of malloced strings
 */
static char **dupargs(char **argv)
{
    int len;
    char **answer;
    int i;
 
    for (len = 0;argv[len];len++);

    answer = malloc( (len + 1) * sizeof(char *));
    if (!answer)
        goto error_exit;
    for (i=0;i<=len;i++)
        answer[i] = 0;
   
    for (i=0;i<len;i++)
    {
        answer[i] = mystrdup(argv[i]);
        if (!answer[i])
            goto error_exit;
    }

    return answer;
 error_exit:
    if (answer)
    {
        for (i=0;i<len;i++)
            free(answer[i]);
        free(answer);
    }

    return 0;
}

/*
  test if a pointer is null or the empty string
 */
static int empty(const char *str)
{
    if (str == 0)
        return 1;
    if( str[0] == 0)
        return 1;
    return 0;
}

/*
  duplicate a string
 */
static char *mystrdup(const char *str)
{
    char *answer;

    answer = malloc(strlen(str) + 1);
    if (answer)
        strcpy(answer, str);

    return answer;
}

int optionsmain(int argc, char **argv)
{
    OPTIONS *opt;
    int Nargs;
    int i;
    char mess[256];
    int age = -1;
    int b = 0;

    opt = options(argc, argv, "-abc");
  
    strcpy(mess, "isdead");
    opt_get(opt, "-fred -a", "%32s", mess);
    opt_get(opt, "-age -AGE", "%d", &age);
    b= opt_get(opt, "-b", 0);
    

    printf("mess %s\n", mess);
    printf("age %d\n", age);
    printf("b %d\n", b);

    Nargs = opt_Nargs(opt);
    for (i=0;i<Nargs;i++)
        printf("argument %d ***%s***\n", i, opt_arg(opt, i));

    if (opt_error(opt, stderr))
        fprintf(stderr, "Bad inputs\n");
    killoptions(opt);

    printf("%d ", contains("-fred", "-fred"));
    return 0;
}
