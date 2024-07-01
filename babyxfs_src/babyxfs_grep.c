#include <stdio.h>
#include "re.h"

/*
 -i, --ignore-case: Ignores case distinctions in patterns and input data.
 -v, --invert-match: Selects the non-matching lines of the provided input pattern.
 -n, --line-number: Prefix each line of the matching output with the line number in the input file.
 -w: Find the exact matching word from the input file or string.
 -c: Count the number of occurrences of the provided pattern.
 */
int usage()
{
    fprintf(stderr,"bbx_grep - te Baby X regular expression parser\n");
    fprintf(stderr, "Usage: bbx_grep [options] <pattern> <file.txt>\n");
    
    return 0;
}

int main(int argc, char **argv)
{
   FILE *fp;
   char line[1024];
    int length;
    
   if (argc != 3)
   {
       usage();
       return 0;
   }

    char *pattern = argv[1];
   fp = fopen(argv[2], "r");
    if (!fp)
    {
        fprintf(stderr, "Can't open %s\n", argv[2]);
    }

   while (fgets(line, 1024, fp))
   {
       int m = re_match(pattern, line, &length);
       if (m >= 0)
           printf("%s", line);
   }
    fclose(fp);
   return 0;
}

