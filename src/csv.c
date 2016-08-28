/*
  csv.c
  code to load a comma-separated value file
  by Malcolm McLean
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h> 
#include <assert.h>

#include "csv.h"

static int hasheader(char ***data, int width, int height);
static int getcoltype(char ***data, int col, int width, int height);
static int gettype(const char *str);
static char **loadline(FILE *fp, int *N);
static char *loadfield(FILE *fp);
static char *loadraw(FILE *fp);
static char *loadquote(FILE *fp);
static void striptrail(char *str);
static char *mystrdup(const char *str);
static double makenan(void);
static int myisnan(double x);

/*
  load a csv file.
  Params: fname - the name of the csv file to load.
  Returns: a CSV object, 0 on fail. 
 */
CSV *loadcsv(const char *fname)
{
  CSV *answer;
  int *len = 0;        /* number of records in each line */
  char ***rec = 0;     /* square matrix of records */
  FILE *fp;
  int i = 0;
  int ii;
  int N;              /* number of lines */
  int width = 0;
  int height = 0;
  void *temp;

  answer = malloc(sizeof(CSV));
  if(!answer)
    return 0;
  answer->data = 0;
  answer->names = 0;
  answer->types = 0;

  fp = fopen(fname, "r");
  if(!fp)
    return 0;

  /* load each row of the csv data */
  do
  {
    temp = realloc(rec, (i+1) * sizeof(char **));
    rec = temp;
    temp = realloc(len, (i+1) * sizeof(int));
    len = temp;
    rec[i] = loadline(fp, &len[i]);
    
    if(len[i] == -2)
      break;
    i++;
  } 
  while(!ferror(fp));
  
  fclose(fp);
  
  /* csv file may contain lines of the wrong length.
     Assume the first line contains the right number of columns.
  */
 
  N = i;
  
  for(i=0;i<N;i++)
  {
    /* if the line is not the right length, take action against it */
    if(len[i] != len[0])
    {
      /* does the line contain records ? */
      for(ii=0;ii<len[i];ii++)
        if(rec[i][ii])
          break;
      /* if it contains no records, just delete it */
      if(ii == len[i])
      {
        for(ii=0;ii<len[i];ii++)
          free(rec[i][ii]);
        free(rec[i]);
        memmove(&len[i], &len[i+1], (N - i - 1) * sizeof(int));
        memmove(&rec[i], &rec[i+1], (N - i -1) * sizeof(char **));
        N--;
        i--;
      }
      /* if the bad lline contains records, just throw the file out */
      else
      { 
        goto error_exit;
      }
    }
  }

  width = len[0];
  free(len);
  
  /* if the CSV file contains a header, fill data */
  if(hasheader(rec, width, N))
  {
    height = N - 1;
    answer->types = malloc(width * sizeof(int));
    answer->names = malloc(width * sizeof(char *));
    answer->data = malloc(width * height * sizeof(CSV_VALUE));
    if(!answer->data || !answer->names || !answer->types)
      goto error_exit;
    for(i=0;i<width;i++)
    {
      answer->names[i] = rec[0][i];
      answer->types[i] = getcoltype(rec + 1, i, width, height);
    }
    for(i=0;i<height;i++)
    {
      for(ii=0;ii<width;ii++)
      {
        switch(answer->types[ii])
        {
          case CSV_REAL:
            if(rec[i+1][ii])
              answer->data[i * width + ii].x = strtod(rec[i+1][ii], 0);
            else
              answer->data[i * width + ii].x = makenan();
            free(rec[i+1][ii]);
            break;
          case CSV_STRING:
            answer->data[i * width + ii].str = rec[i+1][ii];
            rec[i+1][ii] = 0;
            break;
        }
      }
    }
  }
  /* if no header, fill differently */
  else
  {
    height = N;
    answer->names = 0;
    answer->types = malloc(width * sizeof(int));
    answer->data = malloc(width * height * sizeof(CSV_VALUE));
    if(!answer->data || !answer->types)
      goto error_exit;

    for(i=0;i<width;i++)
      answer->types[i] = getcoltype(rec, i, width, height);

    for(i=0;i<height;i++)
      for(ii=0;ii<width;ii++)
      {
        switch(answer->types[ii])
	{
          case CSV_REAL:
            if(rec[i][ii])
              answer->data[i * width + ii].x = strtod(rec[i][ii], 0);
            else
              answer->data[i * width + ii].x = makenan();
            free(rec[i][ii]);
            break;
          case CSV_STRING:
            answer->data[i * width + ii].str = rec[i][ii];
            rec[i][ii] = 0;
            break;
	}
      }
  }

  /* clean up temporary records */
  for(i=0;i<N;i++)
    free(rec[i]);
  free(rec); 

  answer->width = width;
  answer->height = height;
  
  return answer;
  
  /* clean up and return NULL */
 error_exit:
  for(i=0;i<N;i++)
  {
    if(len)
    {
      for(ii=0;ii<len[i];ii++)
        free(rec[i][ii]);
    }
    else
    {
      for(ii=0;ii<width;ii++)
        free(rec[i][ii]);
    }
    free(rec[i]);
  }
  free(rec);
  free(len);
  
  free(answer->data);
  free(answer->names);
  free(answer->types);
  free(answer);

  return 0;
}

/*
  destructor for csv object
  Params: csv - pointer to object to destroy
 */
void killcsv(CSV *csv)
{
  int i;
  int ii;

  if(!csv)
    return;

  for(i=0;i<csv->width;i++)
  {
    if(csv->types[i] == CSV_STRING)
      for(ii=0;ii<csv->height;ii++)
        free(csv->data[ii * csv->width + i].str);
  }
  free(csv->data);
  free(csv->types);
  if(csv->names)
  {
    for(i=0;i<csv->width;i++)
      free(csv->names[i]);
  }
  free(csv->names);

  free(csv);
}

/*
  get the dimensions of a CSV object
  Params: CSV - the comma-separated data
          width - return pointer for no columns
          height - return pointer for no rows (excl header)
 */
void csv_getsize(CSV *csv, int *width, int *height)
{
  *width = csv->width;
  *height = csv->height;
}

/*
  is a data element present
  Params: csv - pointer tot he object
          col - coumn to test
          row - row to test
  Returns: 1 if data present for that object, else 0
 */
int csv_hasdata(CSV *csv, int col, int row)
{
  if(col < 0 || col >= csv->width)
    return 0;
  if(row < 0 || row >= csv->height)
    return 0;
  switch(csv->types[col])
  {
    case CSV_REAL:
      if(myisnan(csv->data[row * csv->width + col].x))
        return 0;
      return 1;
    case CSV_STRING:
      if(csv->data[row * csv->width + col].str == 0)
        return 0;
      return 1;
    default:
      return 0;
  }
  return 0;
}

/*
  get numerical data
  Params: csv - pointer to the object
          col - column of data item
          row - row of data item
  Returns: value of data item.          
 */
double csv_get(CSV *csv, int col, int row)
{
  assert(col >= 0 && col < csv->width);
  assert(row >= 0 && row < csv->height);
  assert(csv->types[col] == CSV_REAL);
 
  return csv->data[row * csv->width + col].x;
}

/*
  get string data
  Params: csv - pointer to the object
          col - column of data item
          row - row of data item
  Returns: pointer to string data item.
 */
const char *csv_getstr(CSV *csv, int col, int row)
{
  assert(col >= 0 && col < csv->width);
  assert(row >= 0 && row < csv->height);
  assert(csv->types[col] == CSV_STRING);

  return csv->data[row * csv->width + col].str;
}

/*
  test if comma-separted values contains header row
  Params: csv - pointer to the object
  Returns: 1 if header present, 0 if absent
 */
int csv_hasheader(CSV *csv)
{
  return csv->names ? 1 : 0;
}

/*
  get information about a csv column
  Parmas: csv - pointer to object
          col - colmn to get infromation about
          type - return pointer type of colum (CSV_REAL, CSV_STRING)
  Returns: pointer to column name, NULL if not present
 */
const char *csv_column(CSV *csv, int col, int *type)
{
  if(type)
    *type = csv->types[col];
  if(csv->names)
    return csv->names[col];
  else
    return 0;
}

/*
  tests if the csv file has a header.
  Params: data - the read in data
          width - number of columns
          height - number of rows
  Returns: 1 if the data has a header row, else 0
  Notes: applies a heuristic. If the first has a different type from
    the rest of the data, it is a header.
 */
static int hasheader(char ***data, int width, int height)
{
  int t1;
  int t2;
  int i;
  int ii;

  if(height < 2)
    return 0;
  for(i=0;i<width;i++)
  {
    t1 = gettype(data[0][i]);
    t2 = gettype(data[1][i]);
    
    for(ii=2;ii<height;ii++)
    {
      if(data[ii][i] && gettype(data[ii][i]) != t2)
        break;
    }
    if(ii == height && t1 != t2)
      return 1;
  }

  return 0;
}

/*
  get the type of a column
  Params: data - the array of read-in strings
          col - column to test
          width - data width
          height - data height
  Returns: type of that column (CSV_STRING if mixed)
 */
static int getcoltype(char ***data, int col, int width, int height)
{
  int t = CSV_NULL;
  int t2;
  int i;

  assert(col < width);

  for(i=0;i<height;i++)
  {
    t2 = gettype(data[i][col]);
    if(t2 != t)
    {
      if(t == CSV_NULL)
        t = t2;
      if(t == CSV_REAL && t2 == CSV_STRING)
        t = CSV_STRING;
    }
  }

  return t;
}

/*
  get the type of a field
  Params: str - string containg data
  Returns: the type
    CSV_NULL - no data
    CSV_REAL - numerical data
    CSV_STRING - string data
 */
static int gettype(const char *str)
{
  char *ptr;
  double x;

  if(!str)
    return CSV_NULL;

  x = strtod(str, &ptr);
  if(*ptr == 0)
    return CSV_REAL;
  else
    return CSV_STRING;
}

/*
  load a line (record) from a csv file.
  Params: fp - pointer to an opne file
          N - return pointer for number of fields read
  Returns: malloced list of malloced strings
 */
static char **loadline(FILE *fp, int *N)
{
  char **answer = 0;
  int ch;
  int i = 0;
  char **temp;

  ch = fgetc(fp);
  if(ch == EOF)
  {
    *N = -2;
    return 0;
  }
  ungetc(ch, fp);

  do
  {
    temp = realloc(answer, (i + 1) * sizeof(char *));
    if(!temp)
    {
      while(i >= 0)
        free(answer[i--]);
      free(answer);
      *N = 0;
      return 0;
    }
    answer = temp;
    answer[i] = loadfield(fp);
    i++;
  }
  while(fgetc(fp) == ',');

  if(i==1 && answer[0] == 0)
  {
    free(answer);
    *N = 0;
    return 0;
  }
  *N = i;

  return answer;
}
  
/*
  load a field from a csv file.
  Params: fp - pointer to an open file.
  Returns: a field read as a character string
    empty fields are returned as 0
 */
static char *loadfield(FILE *fp)
{
  int ch;
  char *answer = 0;

  while( (ch = fgetc(fp)) != EOF)
  {
    if(ch == ',' || ch == '\n')
      break;
    if(isspace(ch))
      continue;
    if(ch == '"')
      answer = loadquote(fp);
    else
    {
      ungetc(ch, fp);
      answer = loadraw(fp);
    }
  }
  ungetc(ch, fp);
  
  return answer;
}

/*
  load a raw (comma-delimited) field from file.
  Params: fp - pointer to open file
  Returns: the field. fp points to delimiter
 */
static char *loadraw(FILE *fp)
{
  char *answer;
  char *temp;
  int len = 64;
  int N = 0;
  int ch;

  answer = malloc(64);
  if(!answer)
    return 0;

  while( (ch = fgetc(fp)) != EOF)
  {
    if(ch == ',' || ch == '\n')
      break;
    answer[N++] = (char) ch;
    if(N == len -1)
    {
      temp = realloc(answer, len + 128);
      if(!temp)
      {
        free(answer);
        return 0;
      }
      answer = temp;
      len += 128;
    }
  }

  ungetc(ch, fp);
  answer[N] = 0;

  striptrail(answer);
  temp = mystrdup(answer);
  free(answer);

  return temp;
}

/*
  load a quote-delimited field
  Params: fp - pointer to an open file.
  Returns: string loaded, 0 on fail. fp points to character after quote.
  Notes: first quote assumed to be read. On entry fp points to first
    character after the quote.
 */
static char *loadquote(FILE *fp)
{
  char *answer;
  char *temp;
  int len = 64;
  int N = 0;
  int ch;
  int ch2;

  answer = malloc(64);
  if(!answer)
    return 0;

  while( (ch = fgetc(fp)) != EOF)
  {
    if(ch != '"')
      answer[N++] = (char) ch;
    else
    {
      ch2 = fgetc(fp);
      if(ch2 == '"')
        answer[N++] = '"';
      else
      {
        ungetc(ch2, fp);
        break;
      }
      if(N == len -2)
      {
        temp = realloc(answer, len + 128);
        if(!temp)
        {
          free(answer);
          return 0;
        }
        len += 128;
      }
    }
  }

  answer[N] = 0;

  temp = mystrdup(answer);
  free(answer);
  return temp;
}

/*
  strip trailing whitespace from a string.
  Params: str - the string.
  Notes: trailing whitespace set to 0
 */
static void striptrail(char *str)
{
  int len;
  len = strlen(str);
  while(len--)
  {
    if(isspace(str[len]))
      str[len] = 0;
    else
      break; 
  }
}

/*
  duplicate a string.
  Params; str - the string to duplicate
  Returns: malloced pointer to duplicated string, 0 on fail.
 */
static char *mystrdup(const char *str)
{
  char *answer = malloc(strlen(str) + 1);
  if(answer)
    strcpy(answer, str);
  return answer;
}

/*
  create a not-a-number (nan)
  Returns: nan
  Portability warning: no ANSI standard way of generating nan()
 */
static double makenan(void)
{
  return sqrt(-1.0);
}

/*
  check for not-a number
  Params: x - number to check
  returns: 1 if a NAN, else 0.
  Portability warning: not all compliers provide an isnan().
*/
static int myisnan(double x)
{
  return isnan(x);
}

/*
  test function
  Loads a csv file and prints out header.
 */
int csvmain(int argc, char **argv)
{
  CSV *csv = 0;
  int width;
  int height;
  int i;
  int type;
  const char *name;

  if(argc == 2)
    csv = loadcsv(argv[1]);
  if(!csv)
  {
    printf("Failed\n");
    exit(EXIT_FAILURE);
  }
  csv_getsize(csv, &width, &height);
  printf("width %d height %d\n", width, height);

  for(i=0;i<width;i++)
  {
    name = csv_column(csv, i, &type);
    printf("%s %s\n", name, type == CSV_STRING ? "string" : "real");
  }

  killcsv(csv);

  return 0;
}
