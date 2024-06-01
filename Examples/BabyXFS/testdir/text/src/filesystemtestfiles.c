#include <stdio.h>

int main(void)
{
  FILE *fp;
  int i;

  fp = fopen("spaces_12.txt", "w");
  for (i = 0; i < 12 ; i++)
     fputc(' ', fp);
   fclose(fp);
  
  fp = fopen("newlines_12.txt", "w");
  for (i = 0; i < 12 ; i++)
     fputc('\n', fp);
   fclose(fp);

    fp = fopen("tabs_12.txt", "w");
    for (i = 0; i < 12 ; i++)
       fputc('\t', fp);
     fclose(fp);

   fp = fopen("formfeeds_12.txt", "w");
   for (i = 0; i < 12 ; i++)
      fputc('\f', fp);
    fclose(fp);

   fp = fopen("carriagereturns_12.txt", "w");
   for (i = 0; i < 12 ; i++)
      fputc('\r', fp);
    fclose(fp);

    fp = fopen("backspaces_12.txt", "w");
    for (i = 0; i < 12 ; i++)
       fputc('\b', fp);
     fclose(fp);

    fp = fopen("crnl_12.txt", "w");
    for (i = 0; i < 12 ; i++)
    {
       fputc('\r', fp);
       fputc('\n', fp);
     }
     fclose(fp);

    fp = fopen("nlcr_12.txt", "w");
    for (i = 0; i < 12 ; i++)
    {  
       fputc('\n', fp);
       fputc('\r', fp);
     }
     fclose(fp);
 
     fp = fopen("mary_nl.txt", "w");
     fprintf(fp, "Mary had a little lamb\n");
     fclose(fp);

     fp = fopen("nl_mary_nl_nl.txt", "w");
     fprintf(fp, "\nMary had a little lamb\n\n");
     fclose(fp);

     fp = fopen("mary.txt", "w");
     fprintf(fp, "Mary had a little lamb");
     fclose(fp);

     fp = fopen("tab_mary.txt_nl", "w");
     fprintf(fp, "\tMary had a little lamb\n");
     fclose(fp);


     fp = fopen("allbytes.bin", "w");
     for(i = 0; i < 256; i++)
        fputc(i, fp);
      fclose(fp);
      
    fp = fopen("allunder128.bin", "w");
     for (i = 0; i < 128; i++)
        fputc(i, fp);
      fclose(fp);

    fp = fopen("all1to127.bin", "w");
     for (i = 1; i < 128; i++)
        fputc(i, fp);
      fclose(fp);

     fp = fopen("zeroes_12.bin", "w");
     for (i = 0; i < 12; i++)
        fputc(0, fp);
      fclose(fp);

    return  0;

}
