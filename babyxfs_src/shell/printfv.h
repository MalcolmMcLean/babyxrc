#ifndef printfv_h
#define prinrfv_h

int Nformatspecifiers(const char *fmt);
int getformatfieldtypes(const char *fmt, char *fieldtype, int Nfields);
int checkprintformat(const char *fmt, int *Nfields, char *error, int Nerr);
int fprintfv(FILE *fp, const char *fmt, double *numbers, char **strings);

#endif
