#include <stdio.h>
#include <string.h>

void starline(void)
{
   int i;

   for (i = 0; i < 79; i++)
     printf("*");
   printf("\n");
}

void blankline(void)
{
   int i;

   printf("*");
   for (i = 0; i < 77; i++)
     printf(" ");
   printf("*");
   printf("\n");
}

void centreline(const char *text)
{
    int leading;
    int trailing;
    int i;
    
    leading = (77 - strlen(text))/2;
    trailing = 77 - leading - strlen(text);
    
    printf("*");
    for (i = 0; i < leading; i++)
        printf(" ");
    printf("%s", text);
    for (i =0; i <trailing; i++)
        printf(" ");
    printf("*");
    printf("\n");
}

int main(int argc, char **argv)
{
   char *commandname;
   int i;
    
    char buff[100];

   if (argc == 2)
      commandname = argv[1];
   else 
     commandname = "";


   starline();
   blankline();
    centreline("Baby X shell");
   blankline();
    snprintf(buff, 100, "Help on %s", commandname);
    centreline(buff);
    blankline();
   starline();
   
   return 0;

}
