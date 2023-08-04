#ifndef dumpcsv_h
#define dumpcsv_h

#include "csv.h"

int dumpcsv(FILE *fpout, int headerfile, const char *name, CSV *csv);
#endif
