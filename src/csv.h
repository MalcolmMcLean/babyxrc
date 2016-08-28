
#ifndef csv_h
#define csv_h

#define CSV_NULL 0       /* null data */
#define CSV_REAL 1       /* floating-point data */
#define CSV_STRING 2     /* string data */
#define CSV_BOOL 3       /* boolean data */

typedef union
{
  double x;        /* real value   */
  char *str;       /* string value */
} CSV_VALUE;

typedef struct
{
  char **names;     /* column names (can be NULL) */
  int *types;       /* type of column CSV_REAL, CSV_STRING etc */
  int width;        /* number of columns */
  int height;       /* number of row (excl header) */
  CSV_VALUE *data;  /* data in row-major format */
} CSV;

CSV *loadcsv(const char *fname);
void killcsv(CSV *csv);
void csv_getsize(CSV *csv, int *width, int *height);
int csv_hasdata(CSV *csv, int col, int row);
double csv_get(CSV *csv, int col, int row);
const char *csv_getstr(CSV *csv, int col, int row);
int csv_hasheader(CSV *csv);
const char *csv_column(CSV *csv, int col, int *type);

#endif
