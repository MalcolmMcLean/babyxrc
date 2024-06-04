#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum PrintFType
{
    PF_UNKNOWN,
    PF_INT,
    PF_DOUBLE,
    PF_CHAR_STAR,
    PF_LONG_INT,
    PF_LONG_LONG_INT,
    PF_LONG_DOUBLE,
    PF_SIZE_T,
};
  

int extractfield(char *field, int Nfield, const char *fmt)
{
   int i = 0;
    int answer = 0;

   if (fmt[i] == '%' && i < Nfield -1)
   {
      field[i++] = '%';
   }
   if (fmt[i] == '%')
   {
      if (fmt[i] == '%' && i < Nfield - 1)
      {
          field[i++] = '%';
          field[i++] = 0;
          return '%';
      }
       return 0;
   }
   while (fmt[i] && !isalpha(fmt[i]) && i < Nfield-1)
   {
      field[i] = fmt[i];
      i++;
   }
   while (fmt[i] && strchr("hljztL", fmt[i]) && i < Nfield -1)
   {
       field[i] = fmt[i];
       i++;
   }
   if (strchr("diouxXeEfFgGaAcs", fmt[i]) && i < Nfield-1)
   {
       field[i] = fmt[i];
       answer = fmt[i];
       i++;
       field[i++] = 0;
   }
   else
   {
       if (i < Nfield -1)
       {
           field[i] = fmt[i];
           i++;
           field[i++] = 0;
       }
       answer = 0;
   }
       
   return answer;
}

const char *getformattype(const char *fmt)
{
    int i, j;
    char specifier[16];
    
    while (fmt[0] && (fmt[0] != '%' || (fmt[0] == '%' && fmt[1] == '%')))
    {
        if (fmt[0] == '%')
            fmt++;
        fmt++;
    }
    if (fmt[0] == 0)
        return "unknown";
    for (i = 0; fmt[i]; i++)
    {
        if (isalpha(fmt[i]))
        {
            j = 0;
            while(fmt[i] == 'h' ||
                  fmt[i] == 'l' ||
                  fmt[i] == 'j' ||
                  fmt[i] == 'z' ||
                  fmt[i] == 't' ||
                  fmt[i] == 'L')
            {
                specifier[j++] = fmt[i++];
                specifier[j] = 0;
                if (j > 4)
                    return "unknown";
            }
            specifier[j++] = fmt[i++];
            specifier[j] = 0;
            
            if (strlen(specifier) == 1)
            {
                if (strchr("diouxX", specifier[0]))
                    return "int";
                if (strchr("aAeEfFgG", specifier[0]))
                    return "double";
                if (strchr("s", specifier[0]))
                    return "char *";
                return "unknown";
            }
            else if(strlen(specifier) == 2)
            {
                switch (specifier[0]) {
                    case 'h':
                        if (strchr("diouxX", specifier[1]))
                            return "int";
                        else
                            return "unknown";
                        break;
                    case 'l':
                        if (strchr("diouxX", specifier[1]))
                            return "long int";
                        else
                            return "unknown";
                        break;
                    case 'z':
                        if (strchr("diouxX", specifier[1]))
                            return "size_t";
                        else
                            return "unknown";
                        break;
                    case 'L':
                        if (strchr("aAeEfFgG", specifier[1]))
                            return "long double";
                        else
                            return "unknown";
                        break;
                    default:
                        return "unknown";
                        break;
                }
            }
            else if(strlen(specifier) == 3)
            {
                if (specifier[0] == 'l' && specifier[1] == 'l' &&
                    strchr("diouxX", specifier[2]))
                        return "long long int";
                else
                    return "unknown";
            }
            
        }
    }
    
    return "unknown";
}

enum PrintFType getformattype_pf(const char *fmt)
{
    const char *type;
    
    type = getformattype(fmt);
    
    if (!strcmp(type, "unknown"))
        return PF_UNKNOWN;
    else if (!strcmp(type, "int"))
        return PF_INT;
    else if (!strcmp(type, "double"))
        return PF_DOUBLE;
    else if (!strcmp(type, "char *"))
        return PF_CHAR_STAR;
    else if (!strcmp(type, "long int"))
        return PF_LONG_INT;
    else if (!strcmp(type, "long long int"))
        return PF_LONG_LONG_INT;
    else if (!strcmp(type, "long double"))
        return PF_LONG_DOUBLE;
    else if (!strcmp(type, "size_t"))
        return PF_SIZE_T;
    
    return PF_UNKNOWN;
}

int Nformatspecifiers(const char *fmt)
{
    int answer = 0;
    
    while (*fmt)
    {
        if (*fmt == '%')
        {
            if (fmt[1] == '%')
                fmt++;
            else
                answer++;
        }
        fmt++;
    }
    
    return answer;
}

int getformatfieldtypes(const char *fmt, char *fieldtype, int Nfields)
{
    char field[256];
    int conversion;
    int i;
    int j = 0;

    if (!fieldtype && Nfields > 0)
        return -1;
    for (i = 0; i < Nfields; i++)
        fieldtype[i] = 0;
    if (Nfields != Nformatspecifiers(fmt))
        return -1;
    
    for (i = 0; fmt[i]; i++)
    {
        if (fmt[i] == '%' && fmt[i+1] == '%')
            i++;
         else if (fmt[i] == '%')
         {
             conversion = extractfield(field, 255, &fmt[i]);
             if (conversion)
                 fieldtype[j++] = conversion;
             else
                 return -1;
             
             i+= strlen(field) - 1;
         }
    }
    
    return 0;
}

int checkprintformat(const char *fmt, int *Nfields, char *error, int Nerr)
{
    int i;
    char field[256] = {0};
    int Nf = 0;
    int conversion;
    int badconversion;
    int len;
    
    if (Nfields)
        *Nfields = -1;
    
    if (!fmt)
    {
        snprintf(error, Nerr, "Format NULL");
        return -1;
    }
   
    for (i = 0; fmt[i]; i++)
    {
       if (fmt[i] == '%' && fmt[i+1] == '%')
           i++;
        else if (fmt[i] == '%')
        {
            conversion = extractfield(field, 255, &fmt[i]);
            if (conversion == 0)
            {
                len = strlen(field);
                if (len > 0)
                    badconversion = field[len-1];
                else
                    badconversion = '?';
                
                snprintf(error, Nerr, "unrecognised conversion specifier %c", badconversion);
                return -1;
            }
            i += strlen(field) -1;
            Nf++;
            
        }
    }
    
    if (Nfields)
        *Nfields = Nf;
    
    return 0;
    
}

 
int fprintfv(FILE*fp, const char *fmt, double *numbers, char **strings)
{
   int i;
   char field[256];
   enum PrintFType pftype = PF_UNKNOWN;
    int in = 0;
    int is = 0;
   int conversion;

   for (i = 0; fmt[i]; i++)
   {
      if (fmt[i] != '%')
         fputc(fmt[i], fp);
      else
      {
          conversion = extractfield(field, 256, &fmt[i]);
          if (conversion == '%')
              fputc('%', fp);
          else if (conversion != 0 && conversion != 'c')
          {
              pftype = getformattype_pf(field);
              switch (pftype)
              {
                  case PF_UNKNOWN:
                      return -1;
                      break;
                  case PF_INT:
                      fprintf(fp, field, (int) numbers[in++]);
                      break;
                  case PF_DOUBLE:
                      fprintf(fp, field, numbers[in++]);
                      break;
                  case PF_CHAR_STAR:
                      fprintf(fp, field, strings[is++]);
                      break;
                  case PF_LONG_INT:
                      fprintf(fp, field, (long) numbers[in++]);
                      break;
                  case PF_LONG_LONG_INT:
                      fprintf(fp, field, (long long) numbers[in++]);
                      break;
                  case PF_LONG_DOUBLE:
                      fprintf(fp, field, (long double) numbers[in++]);
                      break;
                  case PF_SIZE_T:
                      fprintf(fp, field, (size_t) numbers[in++]);
                      break;
              }
          }
          else
              return -1;
          
          i += strlen(field) -1;
      }
   }
    
    return 0;
}


int printfvmain(void)
{
   char *strings[] = {"Fred", "Jim", "Bert", "Harry"};
   double numbers[] = { 1.0, 2.0, -3.0, 0.0};

   fprintfv(stdout, "Hello printfv %3.2f and goodbye\n", numbers, 
strings);

   return 0;
}
