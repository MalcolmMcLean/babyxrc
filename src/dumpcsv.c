#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "asciitostring.h"
#include "dumpcsv.h"



static char *getfieldname(const char *columnheader)
{
    char *answer = malloc(strlen(columnheader) + 32);
    int j = 0;
    int i;
    
    if (!answer)
        return 0;
    
    if (!isalpha((unsigned char)columnheader[0]))
    {
        answer[j++] = 'x';
    }
    for (i = 0; columnheader[i]; i++)
    {
        if (isalnum((unsigned char) columnheader[i]) || columnheader[i] == '_')
            answer[j++] = columnheader[i];
        else if (!isspace((unsigned char) columnheader[i]))
            answer[j++] = '_';
    }
    answer[j] = 0;

    return answer;
}

static char *makestructname(const char *name)
{
    int flag = 0;
    int i;
    char *answer;
    
    answer = getfieldname(name);
    if (!answer)
        return 0;
    for (i = 0; answer[i]; i++)
    {
        if (islower((unsigned char)answer[i]))
        {
            answer[i] = toupper(answer[i]);
            flag = 1;
        }
    }
    if (flag == 0)
    {
        strcat(answer, "_STR");
    }
    
    return answer;
}

static int isrealmatrix(CSV *csv)
{
    int width, height;
    int fieldtype;
    int i;
    
    csv_getsize(csv, &width, &height);
    if (width <= 0 || height <= 0)
        return 0;
    for (i =0; i < width; i++)
    {
        csv_column(csv, i, &fieldtype);
        if (fieldtype != CSV_REAL)
            return 0;
    }
    
    return 1;
    
}

static int isstringtable(CSV *csv)
{
    int width, height;
    int fieldtype;
    int i;
    
    csv_getsize(csv, &width, &height);
    if (width <= 0 || height <= 0)
        return 0;
    for (i =0; i < width; i++)
    {
        csv_column(csv, i, &fieldtype);
        if (fieldtype != CSV_STRING)
            return 0;
    }
    
    return 1;
}


static int dumpasmatrix(FILE *fp, const char *name, CSV *csv)
{
    int width, height;
    int i, ii;
    
    csv_getsize(csv, &width, &height);
    if (width <= 0 || height <= 0)
        return -2;
    
    fprintf(fp, "double %s[%d][%d] =\n", name, height, width);
    fprintf(fp, "{\n");
    for (i = 0; i < height; i++)
    {
        fprintf(fp, "\t{");
        for (ii =0; ii < width; ii++)
        {
            if (csv_hasdata(csv, ii, i))
            {
                double x = csv_get(csv, ii, i);
                fprintf(fp, "%g, ", x);
            }
            else
            {
                fprintf(fp, "NAN, ");
            }
        }
        fprintf(fp, "},\n");
    }
    fprintf(fp, "};\n\n");

    return 0;
}

static int dumpasstringmatrix(FILE *fp, const char *name, CSV *csv)
{
    int width, height;
    int i, ii;
    const char *str;
    char *cstr;
    
    csv_getsize(csv, &width, &height);
    if (width <= 0 || height <= 0)
        return -2;
    
    fprintf(fp, "const char *%s[%d][%d] =\n", name, height, width);
    fprintf(fp, "{\n");
    for (i = 0; i < height; i++)
    {
        fprintf(fp, "\t{");
        for (ii =0; ii < width; ii++)
        {
            if (csv_hasdata(csv, ii, i))
            {
                str = csv_getstr(csv, ii, i);
                cstr = texttostring(str);
                fprintf(fp, "%s, ", cstr);
                free(cstr);
            }
            else
            {
                fprintf(fp, "\"\", ");
            }
        }
        fprintf(fp, "},\n");
    }
    fprintf(fp, "};\n\n");

    return 0;
}


static int dumpwithheader(FILE *fp, const char *name, CSV *csv)
{
    int width, height;
    int i, ii;
    char *fieldname;
    int fieldtype;
    char *structname;
    const char *str;
    char *cstr;
    
    structname = makestructname(name);
    
    
    csv_getsize(csv, &width, &height);
            
    fprintf(fp, "typedef struct\n");
    fprintf(fp, "{\n");
            
    for (i =0; i < width; i++)
    {
        fieldname = getfieldname(csv_column(csv, i, &fieldtype));
        switch (fieldtype)
        {
            case CSV_NULL:
                fprintf(fp, "\tint %s_null;\n", fieldname);
                break;
            case CSV_REAL:
                fprintf(fp, "\tdouble %s;\n", fieldname);
                break;
            case CSV_STRING:
                fprintf(fp, "\tconst char *%s;\n", fieldname);
                break;
            case CSV_BOOL:
                fprintf(fp, "\tint %s;\n", fieldname);
                break;
        }
        free(fieldname);
    }
    fprintf(fp, "}%s;\n\n", structname);
    
    fprintf(fp, "%s %s[%d] =\n", structname, name, height);
    fprintf(fp, "{\n");
    for (i = 0; i < height; i++)
    {
        fprintf(fp, "    {");
        for (ii = 0; ii < width; ii++)
        {
            csv_column(csv, ii, &fieldtype);
            switch (fieldtype)
            {
                case CSV_NULL:
                    fprintf(fp, "0, ");
                    break;
                case CSV_REAL:
                    fprintf(fp, "%g, ", csv_get(csv, ii, i));
                    break;
                case CSV_STRING:
                    str = csv_getstr(csv, ii, i);
                    cstr = texttostring(str);
                    fprintf(fp, "%s, ", cstr);
                    free(cstr);
                    break;
                case CSV_BOOL:
                    fprintf(fp, "%d, ", (csv_get(csv, ii, i) == 0.0) ? 0 : 1);
                    break;
            }
        }
        fprintf(fp, "},\n");
    }
    fprintf(fp, "};\n\n");

    free(structname);

    return 0;

}

int dumpcsv(FILE *fp, const char *name, CSV *csv)
{
  int answer = 0;
    
  if (csv_hasheader(csv))
  {
      dumpwithheader(fp, name, csv);
  }
  else
  {
      if (isrealmatrix(csv))
          dumpasmatrix(fp, name, csv);
      else if (isstringtable(csv))
          dumpasstringmatrix(fp, name, csv);
      else
          fprintf(stderr, "csv data %s has no headers for struct field names\n", name);
  }
    
  return answer;
}
