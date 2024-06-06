#include <stdio.h>

/*
   rot 13 encryption for ASCII

   By Malcolm McLean
*/
int main(void)
{
   int ch;

   while ( (ch = fgetc(stdin)) != EOF)
   {
       ch -= 'A';
       if (ch >= 0 && ch < 26)
         ch = (ch + 13) % 26;
       else if (ch >= 0 && ch > 26 + 6 && ch < 52)
         ch = ((ch - 6 + 13) % 26) + 6 + 26;
       fputc(ch + 'A', stdout);
   }
   return 0;
}
