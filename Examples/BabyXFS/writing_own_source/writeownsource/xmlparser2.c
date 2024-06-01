/*
 XML parser 2, by Malcolm McLean
 Vanilla XML parser
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define MAXRECURSIONLIMIT 100


typedef struct xmlattribute
{
  char *name;                /* attriibute name */
  char *value;               /* attribute value (without quotes) */
  struct xmlattribute *next; /* next pointer in linked list */
} XMLATTRIBUTE;

typedef struct xmlnode
{
  char *tag;                 /* tag to identify data type */
  XMLATTRIBUTE *attributes;  /* attributes */
  char *data;                /* data as ascii */
  int position;              /* position of the node within parent's data string */
  int lineno;                /* line number of node in document */
  struct xmlnode *next;      /* sibling node */
  struct xmlnode *child;     /* first child node */
} XMLNODE;

typedef struct
{
  XMLNODE *root;             /* the root node */
} XMLDOC;

struct strbuff
{
    const char *str;
    int pos;
};

struct utf16buff
{
    char rack[8];
    int pos;
    FILE *fp;
};

typedef struct
{
  char *str;
  int capacity;
  int N;
} STRING;

typedef struct
{
  int set;
  char message[1024];
    struct lexer *lexer;
    int recursiondepth;
} ERROR;


typedef struct lexer
{
  int (*getch)(void *ptr);
  void *ptr;
  int token;
  int lineno;
  int columnno;
  int badmatch;
  ERROR *err;
} LEXER;

#define UNKNOWNSHRIEK 1000
#define COMMENT 1001
#define CDATA 1002
#define INCLUDE 1002
#define IGNORE 1003
#define DOCTYPE 1004
#define ELEMENT 1005
#define ATTLIST 1006
#define NOTATION 1007

#define FMT_UNKNOWN 0
#define FMT_UTF8 1
#define FMT_UTF16LE 2
#define FMT_UTF16BE 3

static int textencoding(FILE *fp);
static int fileaccess(void *ptr);
static int utf16accessbe(void *ptr);
static int utf16accessle(void *ptr);
static int bbx_utf8_putch(char *out, int ch);
static int stringaccess(void *ptr);

void killxmlnode(XMLNODE *node);
static void killxmlattribute(XMLATTRIBUTE *attr);

static int is_initidentifier(int ch);
static int is_elementnamech(int ch);
static int is_attributenamech(int ch);

static int string_init(STRING *s);
static void string_push(STRING *s, int ch, ERROR *err);
static void string_concat(STRING *s, const char *str, ERROR *err);
static char *string_release(STRING *s);

static XMLDOC *xmldocument(LEXER *lex, ERROR *err);
static XMLNODE *xmlnode(LEXER *lex, ERROR *err);
static XMLNODE *comment(LEXER *lex, ERROR *err);
static XMLATTRIBUTE *attributelist(LEXER *lex, ERROR *err);
static XMLATTRIBUTE *xmlattribute(LEXER *lex, ERROR *err);
static char *quotedstring(LEXER *lex, ERROR *err);
static char *textspan(LEXER *lex, ERROR *err);
static char *cdata(LEXER *lex, ERROR *err);
static char *processinginstruction(LEXER *lex, ERROR *err);
static char *attributename(LEXER *lex, ERROR *err);
static char *elementname(LEXER *lex, ERROR *err);
static int escapechar(LEXER *lex, ERROR *err);
static int shriektype(LEXER *lex, ERROR *err);
static void skipbom(LEXER *lex, ERROR *err);
static void skipunknowntag(LEXER *lex, ERROR *err);
static void skipwhitespace(LEXER *lex, ERROR *err);

static void initerror(ERROR *err);
static void enterrecursion(ERROR *err);
static void endrecursion(ERROR *err);
static void reporterror(ERROR *err, const char *fmt, ...);

static void initlexer(LEXER *lex, ERROR *err, int (*getch)(void *), void *ptr);
static int gettoken(LEXER *lex);
static int match(LEXER *lex, int token);

static char *mystrdup(const char *str);




XMLDOC *loadxmldoc(const char *filename,char *errormessage, int Nerr)
{
   FILE *fp;
    ERROR error;
   LEXER lexer;
   XMLDOC *answer = 0;
    int encoding;
    struct utf16buff utf16buf = {0};
    
    initerror(&error);

   if (errormessage && Nerr > 0)
      errormessage[0] = 0;

   fp = fopen(filename, "r");
   if (!fp)
   {
      snprintf(errormessage, Nerr, "Can't open %s", filename);
      return 0;
   }
   else
   {
      encoding = textencoding(fp);
      if (encoding == FMT_UTF8)
      {
          initlexer(&lexer, &error, fileaccess, fp);
      }
      else if (encoding == FMT_UTF16BE)
      {
          utf16buf.fp  = fp;
          initlexer(&lexer, &error, utf16accessbe, &utf16buf);
      }
       else if (encoding == FMT_UTF16LE)
       {
           utf16buf.fp = fp;
           initlexer(&lexer, &error, utf16accessle, &utf16buf);
       }
       else
       {
           snprintf(errormessage, Nerr, "Can't determine text format of %s", filename);
           return 0;
       }
       
      answer = xmldocument(&lexer, &error);
      if (error.set)
      {
         snprintf(errormessage, Nerr, "%s", error.message);
      }
      return answer;
   }   
}

XMLDOC *floadxmldoc(FILE *fp, char *errormessage, int Nerr)
{
    ERROR error;
    LEXER lexer;
    XMLDOC *answer = 0;
    int encoding;
    struct utf16buff utf16buf = {0};

    initerror(&error);

    if (errormessage && Nerr > 0)
       errormessage[0] = 0;
    
    encoding = textencoding(fp);
    if (encoding == FMT_UTF8)
    {
        initlexer(&lexer, &error, fileaccess, fp);
    }
    else if (encoding == FMT_UTF16BE)
    {
        utf16buf.fp  = fp;
        initlexer(&lexer, &error, utf16accessbe, &utf16buf);
    }
     else if (encoding == FMT_UTF16LE)
     {
         utf16buf.fp = fp;
         initlexer(&lexer, &error, utf16accessle, &utf16buf);
     }
     else
     {
         snprintf(errormessage, Nerr, "Can't determine text format of stream");
         return 0;
     }

    answer = xmldocument(&lexer, &error);
    if (error.set)
    {
       snprintf(errormessage, Nerr, "%s", error.message);
    }
       
    return answer;
}

/*
 Get the text encoding, gobbling the the first '<'.
 */
static int textencoding(FILE *fp)
{
    long pos;
    int ch1, ch2;
    int answer = 0;
    
    ch1 = fgetc(fp);
    ch2 = fgetc(fp);
    
    if (ch1 == '<' && ch2 != 0)
    {
        ungetc(ch2, fp);
        return FMT_UTF8;
    }
    else if (ch1 == '<' && ch2 == 0)
        return FMT_UTF16LE;
    else if (ch1 == 0 && ch2 == '<')
        return FMT_UTF16BE;
    else if (ch1 == 0xFF && ch2 == 0xFE)
    {
        ch1 = fgetc(fp);
        ch2 = fgetc(fp);
        while (ch1 + ch2 * 256 < 128 && isspace(ch1))
        {
            ch1 = fgetc(fp);
            ch2 = fgetc(fp);
        }
        if (ch1 == '<' && ch2 == '0')
            return FMT_UTF16LE;
        else
            return FMT_UNKNOWN;
    }
    else if (ch1 == 0xFE && ch2 == 0xFF)
    {
        ch1 = fgetc(fp);
        ch2 = fgetc(fp);
        while (ch1 * 256 + ch2 < 128 && isspace(ch2))
        {
            ch1 = fgetc(fp);
            ch2 = fgetc(fp);
        }
        if (ch1 == 0 && ch2 == '<')
            return FMT_UTF16BE;
        else
            return FMT_UNKNOWN;
    }
    else if (ch1 == 0xEF && ch2 == 0xBB)
    {
        ch1 = fgetc(fp);
        ch2 = fgetc(fp);
        if (ch1 == 0xBF)
        {
            while (isspace(ch2))
                ch2 = fgetc(fp);
            if (ch2 == '<')
                return FMT_UTF8;
            else
                return FMT_UNKNOWN;
        }
        else
            return FMT_UNKNOWN;
    }
    else if (isspace(ch1) && ch2 == 0)
    {
        while (ch1 + ch2 * 256 < 128 && isspace(ch1))
        {
            ch1 = fgetc(fp);
            ch2 = fgetc(fp);
        }
        if (ch1 == '<' && ch2 == 0)
            return FMT_UTF16LE;
        else
            return FMT_UNKNOWN;
    }
    else if (ch1 == 0 && isspace(ch2))
    {
        while (ch1 * 256 + ch2 < 128 && isspace(ch2))
        {
            ch1 = fgetc(fp);
            ch2 = fgetc(fp);
        }
        if (ch1 == 0 && ch2 == '<')
            return FMT_UTF16BE;
        else
            return FMT_UNKNOWN;
    }
    else if (ch1 != 0 && ch2 != 0)
    {
        while (isspace(ch1))
        {
            ch1 = ch2;
            ch2 = fgetc(fp);
        }
        if (ch1 == '<')
        {
            ungetc(ch2, fp);
            return FMT_UTF8;
        }
        else
            return FMT_UNKNOWN;
    }
    return FMT_UNKNOWN;
}

static int fileaccess(void *ptr)
{
   FILE *fp = ptr;
   return fgetc(fp);
}

static int utf16accessbe(void *ptr)
{
    struct utf16buff *up = ptr;
    int Nchars;
    int wch;
    int ch;
    
    if (up->rack[up->pos])
        return up->rack[up->pos++];
    else {
        wch = fgetc(up->fp);
        if (wch == EOF)
            return EOF;
        wch *= 256;
        ch = fgetc(up->fp);
        if (ch == EOF)
            return EOF;
        wch |= ch;
        Nchars = bbx_utf8_putch(up->rack, wch);
        up->rack[Nchars] = 0;
        up->pos = 0;
        return up->rack[up->pos++];
    }
    
}

static int utf16accessle(void *ptr)
{
    struct utf16buff *up = ptr;
    int Nchars;
    int wch;
    int ch;
    
    if (up->rack[up->pos])
        return up->rack[up->pos++];
    else {
        wch = fgetc(up->fp);
        if (wch == EOF)
            return EOF;
        ch = fgetc(up->fp);
        if (ch == EOF)
            return EOF;
        wch |= (ch * 256);
        Nchars = bbx_utf8_putch(up->rack, wch);
        up->rack[Nchars] = 0;
        up->pos = 0;
        return up->rack[up->pos++];
    }
    
}

static int bbx_utf8_putch(char *out, int ch)
{
  char *dest = out;
  if (ch < 0x80)
  {
     *dest++ = (char)ch;
  }
  else if (ch < 0x800)
  {
    *dest++ = (ch>>6) | 0xC0;
    *dest++ = (ch & 0x3F) | 0x80;
  }
  else if (ch < 0x10000)
  {
     *dest++ = (ch>>12) | 0xE0;
     *dest++ = ((ch>>6) & 0x3F) | 0x80;
     *dest++ = (ch & 0x3F) | 0x80;
  }
  else if (ch < 0x110000)
  {
     *dest++ = (ch>>18) | 0xF0;
     *dest++ = ((ch>>12) & 0x3F) | 0x80;
     *dest++ = ((ch>>6) & 0x3F) | 0x80;
     *dest++ = (ch & 0x3F) | 0x80;
  }
  else
    return 0;
  return dest - out;
}



XMLDOC *xmldocfromstring(const char *str,char *errormessage, int Nerr)
{
   ERROR error;
   LEXER lexer;
   XMLDOC *answer = 0;
   struct strbuff strbuf;
    int ch;
    
    initerror(&error);

   if (errormessage && Nerr > 0)
      errormessage[0] = 0;

    strbuf.str = str;
    strbuf.pos = 0;
    /* the lexer has been hacked to support non-seekable streams, so
     we need to pull out the first character */
    ch = stringaccess(&strbuf);
    if (ch != '<')
    {
        snprintf(errormessage, Nerr, "string must start with a \'<\' character");
        return 0;
    }
    initlexer(&lexer, &error, stringaccess, &strbuf);
    answer = xmldocument(&lexer, &error);
    if (error.set)
    {
         snprintf(errormessage, Nerr, "%s", error.message);
    }
    return answer;
}

static int stringaccess(void *ptr)
{
    struct strbuff *s = ptr;
    if (s->str[s->pos])
        return s->str[s->pos++];
    else
        return EOF;
}


/*
  document destructor
*/
void killxmldoc(XMLDOC *doc)
{
  if(doc)
  {
      killxmlnode(doc->root);
      free(doc);
  }
}

/*
  get the root node of the document
*/
XMLNODE *xml_getroot(XMLDOC *doc)
{
  return doc->root;
}

/*
  get a node's tag
*/
const char *xml_gettag(XMLNODE *node)
{
    return node->tag;
}

/*
  get a node's data
*/
const char *xml_getdata(XMLNODE *node)
{
    return node->data;
}

/*
  get a node's attributes
*/
const char *xml_getattribute(XMLNODE *node, const char *attr)
{
  XMLATTRIBUTE *next;

  for(next = node->attributes; next; next = next->next)
    if(!strcmp(next->name, attr))
        return next->value;

  return 0;
}

/*
  get the number of direct children of the node
*/
int xml_Nchildren(XMLNODE *node)
{
  XMLNODE *next;
  int answer = 0;

  if(node->child)
  {
    next = node->child;
    while(next)
    {
      answer++;
      next = next->next;
    }
  }

  return answer;
}

/*
  get the number of direct children with a particular tag
  Params: node - the node
          tag - the tag (NULL for all children)
  Returns: numer of children with that tag
*/
int xml_Nchildrenwithtag(XMLNODE *node, const char *tag)
{
  XMLNODE *next;
  int answer = 0;

  if(node->child)
  {
    next = node->child;
    while(next)
    {
      if(tag == 0 || (next->tag && !strcmp(next->tag, tag)))
        answer++;
      next = next->next;
    }
  }

  return answer;
}

/*
  get child with tag and index
  Params: node - the node
          tag - tag of child (NULL for all children)
          index - index number of child to retrieve
  Returns: child, or null on fail
  Notes: slow, only call for nodes with relatively small
    numbers of children. If the child list is very long,
    step through the linked list manually.
*/
XMLNODE *xml_getchild(XMLNODE *node, const char *tag, int index)
{
  XMLNODE *next;
  int count = 0;

  if(node->child)
  {
    next = node->child;
    while(next)
    {
      if(tag == 0 || (next->tag && !strcmp(next->tag, tag)))
      {
        if(count == index)
          return next;
        count++;
      }
      next = next->next;
    }
  }

  return 0;
}

/*
  recursive get descendants
  Params; node the the node
          tag - tag to retrieve
          list = pointer to return list of pointers to matchign nodes
          N - return for number of nodes found, also index of current place to write
  Returns: 0 on success -1 on out of memory
  Notes:
    we are descending the tree, allocating space for a pointer for every
    matching node.

*/
static int getdescendants_r(XMLNODE *node, const char *tag,  XMLNODE ***list, int *N)
{
  XMLNODE **temp;
  XMLNODE *next;
  int err;

  next = node;
  while(next)
  {
    if(tag == 0 || (next->tag && !strcmp(next->tag, tag)))
    {
      temp = realloc(*list, (*N +1) * sizeof(XMLNODE *));
      if(!temp)
        return -1;
      *list = temp;
      (*list)[*N] = next;
      (*N)++;
    }
    if(next->child)
    {
      err = getdescendants_r(next->child, tag, list, N);
      if(err)
        return err;
    }
    next = next->next;
  }

  return 0;
}

/*
 get all descendants that match a particular tag
   Params: node - the root node
           tag - the tag
           N - return for number found
   Returns: 0 on success, -1 on out of memory
   Notes: useful for fishing. Save you are reading the crossword
     tag, but you don't know exactly whther it is root or
     some child element. You also don't know if several of them are
     in the file. Just call to extract a list, then query for
     children so you know that the tag is an actual match.
     Don't call for huge lists as inefficient.
*/
XMLNODE **xml_getdescendants(XMLNODE *node, const char *tag, int *N)
{
  XMLNODE **answer = 0;
  int err;

  *N = 0;
  err = getdescendants_r(node, tag, &answer, N);
  if(err)
  {
    free(answer);
    return 0;
  }

  return answer;
}

static void getnestedata_r(XMLNODE *node, STRING *str, ERROR *err)
{
    XMLNODE *child;
    int i = 0;
    
    for (child = node->child; child; child = child->next)
    {
        while (node->data && i < child->position && node->data[i])
            string_push(str, node->data[i++], err);
        getnestedata_r(child, str, err);
    }
    while (node->data && node->data[i])
        string_push(str, node->data[i++], err);
    
}

char *xml_getnesteddata(XMLNODE *node)
{
    ERROR error;
    STRING str;
    initerror(&error);
    string_init(&str);
    getnestedata_r(node, &str, &error);
    return string_release(&str);
}

/*
    get a node's line number.
    (note this one is null-guarded because it is meant to be called in error conditions)
 */
int xml_getlineno(XMLNODE *node)
{
    if (node)
        return node->lineno;
    return -1;
}
/*
  Report any attributes which are not on a list of knowwn attributes
 
  Params: node the node to test
        ... NULL-terminated list of known node names.
   Returns: a list of unrecognised attribures as a deep copy, 0 if
       there are none.
    Notes:
       Call as
       XMLATTRIBUTE *badattr =  0;
       char *end = 0;
 
     badatttr = xml_unknownattributes(node, "faith", "hope", "charity", end);
     if (badattr)
         // We've got a node with some attributes we don't recognise
    
    Don't pass NULL for the terminator, becuase sometimes it expands to integer 0 and gets put on the argument list as 32 bits instead of 64.
 */
XMLATTRIBUTE *xml_unknownattributes(XMLNODE *node, ...)
{
    int *matches;
    va_list args;
    char *name;
    XMLATTRIBUTE *attr;
    XMLATTRIBUTE *copy = 0;
    XMLATTRIBUTE *answer = 0;
    int Nattributes = 0;
    int i;
    
    for (attr = node->attributes; attr != NULL; attr = attr->next)
        Nattributes++;
    
    if (Nattributes == 0)
        return 0;
    
    matches = malloc(Nattributes * sizeof(int));
    if (!matches)
        return 0;
    
    for (i = 0; i < Nattributes; i++)
        matches[i] = 0;
    
    va_start(args, node);
    name = va_arg(args, char *);

    while (name)
    {
       
        i = 0;
        for (attr = node->attributes; attr != NULL; attr = attr->next)
        {
            if (!strcmp(attr->name, name))
                matches[i]++;
            i++;
        }
        name = va_arg(args, char *);
    }
    va_end(args);
    
    i = 0;
    for (attr = node->attributes; attr != NULL; attr = attr->next)
    {
        if (!matches[i])
        {
            copy = malloc(sizeof(XMLATTRIBUTE));
            if (!copy)
                goto out_of_memory;
            copy->name = mystrdup(attr->name);
            if (!copy->name)
                goto out_of_memory;
            copy->value = mystrdup(attr->value);
            if (!copy->value)
                goto out_of_memory;
            copy->next = answer;
            answer = copy;
            copy = 0;
        }
        i++;
    }
    
    copy = 0;
    while (answer)
    {
        attr = answer;
        answer = answer->next;
        attr->next = copy;
        copy = attr;
    }
     answer = copy;
    
    free(matches);
    
    return answer;
    
out_of_memory:
    free(matches);
    if (copy)
    {
        free(copy->name);
        free(copy->value);
        free(copy);
    }
    killxmlattribute(answer);
    
    return  0;
}


/*
  xml node destructor
  Notes: destroy siblings in a list, chilren recursively
    as children are unlikely to be nested very deep

*/
void killxmlnode(XMLNODE *node)
{
  XMLNODE *next;

  if(node)
  {
    while(node)
    {
      next = node->next;
      if(node->child)
        killxmlnode(node->child);
      killxmlattribute(node->attributes);
      free(node->data);
      free(node->tag);
      free(node);
      node = next;
    }
  }
}

/*
  destroy the attributes list
*/
static void killxmlattribute(XMLATTRIBUTE *attr)
{
  XMLATTRIBUTE *next;
  if(attr)
  {
    while(attr)
    {
       next = attr->next;
       free(attr->name);
       free(attr->value);
       free(attr);
       attr = next;
    }
  }
}

static int is_initidentifier(int ch)
{
   if (isalpha(ch) || ch == '_')
     return 1;
   return 0;
}

static int is_elementnamech(int ch)
{
   if (isalpha(ch) || isdigit(ch) || ch == '_' || ch == '-' || ch == '.' || ch == ':')
     return 1;
   return 0;
}

static int is_attributenamech(int ch)
{
   if (isalpha(ch) || isdigit(ch) || ch == '_' || ch == '-' || ch == '.' || ch == ':')
     return 1;
   return 0;
}

static void trim(char *str)
{
    int i;
    
    for (i = 0; str[i]; i++)
        if (!isspace((unsigned char) str[i]))
            break;
    if (i != 0)
        memmove(str, &str[i], strlen(str) -i + 1);
    if (str[0])
    {
        i = strlen(str) -1;
        while (i >= 0 && isspace((unsigned char)str[i]))
           str[i--] = 0;
    }
        
}

static int string_init(STRING *s)
{
  s->str = 0;
  s->capacity = 0;
  s->N = 0;
    
  return 0;
}

static void string_push(STRING *s, int ch, ERROR *err)
{
    char *temp = 0;
    
   if (s->capacity < s->N * 2 + 2)
   {
     temp = realloc(s->str, s->N * 2 + 2);
     if (!temp)
       goto out_of_memory;
     s->str = temp;
     s->capacity = s->N * 2 + 2; 
   }
   s->str[s->N++] = ch;
   s->str[s->N] = 0;
   return;
out_of_memory:
   reporterror(err, "out of memory");

}

static void string_concat(STRING *s, const char *str, ERROR *err)
{
    int i;
    
    for (i =0; str[i]; i++)
        string_push(s, str[i], err);
}

static char *string_release(STRING *s)
{
   char *answer;

   if (s->str == 0)
   {
       s->str = malloc(1);
       if (s->str)
           s->str[0] = 0;
       else
           return 0;
       
   }
   answer = realloc(s->str, s->N + 1);
   s->str = 0;
   s->N = 0;
   s->capacity = 0;

   return answer;
}

static XMLDOC *xmldocument(LEXER *lex, ERROR *err)
{
    XMLNODE *node;
    XMLDOC *doc;
    int ch;
    int shriek;
    
    doc = malloc(sizeof(XMLDOC));
    if (!doc)
    {
        reporterror(err, "out of memory");
        return 0;
    }
    
    skipbom(lex, err);

    do {
        skipwhitespace(lex, err);
        if (!match(lex, '<'))
            reporterror(err, "can't find opening tag");
        ch = gettoken(lex);
        if (is_initidentifier(ch))
        {
            node = xmlnode(lex, err);
            if (node)
            {
                if (!err->set)
                {
                    doc->root = node;
                    return doc;
                }
                else
                {
                    killxmlnode(node);
                    break;
                }
            }
            else
            {
                reporterror(err, "bad root node");
            }
        }
        else if (ch == '!')
        {
            shriek = shriektype(lex, err);
            if (shriek == COMMENT)
                comment(lex, err);
            else
                skipunknowntag(lex, err);
        }
        else  if (ch == '?')
        {
            char *text;
            text = processinginstruction(lex, err);
            free(text);
        }
        else {
            skipunknowntag(lex, err);
        }
    } while (ch != EOF);
    
    free(doc);
    return 0;
}

static XMLNODE *xmlnode(LEXER *lex, ERROR *err)
{
    int ch;
    char *tag = 0;
    XMLATTRIBUTE *attributes = 0;
    XMLNODE *node = 0;
    XMLNODE *lastchild = 0;
    STRING datastr;
    int shriek;
    int lineno;
    
    if (err->set)
        return 0;
    enterrecursion(err);
    
    string_init(&datastr);
    
    lineno = lex->lineno;
    tag = elementname(lex, err);
    if (!tag)
        goto parse_error;
    attributes = attributelist(lex, err);
    skipwhitespace(lex, err);
    ch = gettoken(lex);
    if (ch == '/')
    {
        match(lex, '/');
        if (!match(lex, '>'))
            goto parse_error;
        node = malloc(sizeof(XMLNODE));
        if (!node)
            goto out_of_memory;
        node->tag = tag;
        node->attributes = attributes;
        node->data = 0;
        node->position = 0;
        node->lineno = lineno;
        node->child = 0;
        node->next = 0;
        endrecursion(err);
        return node;
    }
    else if (ch == '>')
    {
        match(lex, '>');
        node = malloc(sizeof(XMLNODE));
        if (!node)
            goto out_of_memory;
        node->tag = tag;
        node->attributes = attributes;
        node->data = 0;
        node->position = 0;
        node->lineno = lineno;
        node->child = 0;
        node->next = 0;
        tag = 0;
        attributes = 0;
        
        do {
            char *text = textspan(lex, err);
            if (text)
            {
                string_concat(&datastr, text, err);
                free(text);
                text = 0;
            }
            ch = gettoken(lex);
            if (ch == '<')
            {
                match(lex, '<');
                ch = gettoken(lex);
                if (is_initidentifier(ch))
                {
                    XMLNODE *child = xmlnode(lex, err);
                    if (!child)
                        goto parse_error;
                    if (lastchild)
                        lastchild->next = child;
                    else
                        node->child = child;
                    lastchild = child;
                    child->position = datastr.N;
                }
                else if(ch == '/')
                {
                    match(lex, '/');
                    tag = elementname(lex,err);
                    if (tag && !strcmp(tag, node->tag))
                    {
                        node->data = string_release(&datastr);
                        free(tag);
                        match(lex, '>');
                        endrecursion(err);
                        return node;
                    }
                    else
                    {
                        reporterror(err, "bad closing tag %s", tag);
                        goto parse_error;
                    }
                }
                else if (ch == '!')
                {
                    shriek = shriektype(lex, err);
                    if (shriek == COMMENT)
                        comment(lex, err);
                    else if(shriek == CDATA)
                    {
                        text = cdata(lex, err);
                        if (text)
                            string_concat(&datastr, text, err);
                        free(text);
                    }
                }
                else if (ch == '?')
                {
                    text = processinginstruction(lex, err);
                    free (text);
                }
                else
                {
                    goto parse_error;
                }
            }
        } while (ch != EOF);
    }
    else
    {
        goto parse_error;
    }
parse_error:
    reporterror(err, "error parsing element");
    endrecursion(err);
    return 0;
out_of_memory:
    reporterror(err, "out of memory");
    endrecursion(err);
    return 0;
}

static XMLNODE *comment(LEXER *lex, ERROR *err)
{
    char buff[4] = {0};
    int ch;
    int lineno;
    
    lineno = lex->lineno;
    
    while ((ch = gettoken(lex)) != EOF)
    {
        match(lex, ch);
        memmove(buff, buff+1, 3);
        buff[2] = ch;
        if (!strcmp(buff, "-->"))
            return 0;
    }
    
    reporterror(err, "bad comment (starts line");
    return 0;
          
}

static XMLATTRIBUTE *attributelist(LEXER *lex, ERROR *err)
{
    int ch;
    XMLATTRIBUTE *answer = 0;
    XMLATTRIBUTE *last = 0;
    XMLATTRIBUTE *attr = 0;
    
    while(1)
    {
        skipwhitespace(lex, err);
        ch = gettoken(lex);
        if (is_initidentifier(ch))
        {
            attr = xmlattribute(lex, err);
            if (!attr)
                goto parse_error;
            if (last)
            {
                last->next = attr;
                last = attr;
            }
            else
            {
                answer = attr;
                last = answer;
            }
        }
        else
        {
            break;
        }
    }
    
    return answer;
parse_error:
    killxmlattribute(answer);
    
    return 0;
}

static XMLATTRIBUTE *xmlattribute(LEXER *lex, ERROR *err)
{
    char *name = 0;
    char *value = 0;
    XMLATTRIBUTE *answer = 0;
    
    name = attributename(lex, err);
    if (!name)
        goto parse_error;
    skipwhitespace(lex, err);
    if (!match(lex, '='))
        goto parse_error;
    skipwhitespace(lex, err);
    value = quotedstring(lex, err);
    if (!value)
        goto parse_error;
    
    answer = malloc(sizeof(XMLATTRIBUTE));
    if (!answer)
        goto out_of_memory;
    answer->name = name;
    answer->value = value;
    answer->next = 0;
    return answer;
    
parse_error:
    reporterror(err, "error in attribute");
    free(name);
    free(value);
    free(answer);
    return 0;
    
out_of_memory:
    reporterror(err, "out of memory");
    free(name);
    free(value);
    free(answer);
    return 0;
}



static char *quotedstring(LEXER *lex, ERROR *err)
{
    int quotech;
    STRING str;
    int ch;
    
    string_init(&str);
    
    quotech = gettoken(lex);
    if (quotech != '\"' && quotech != '\'')
    {
        goto parse_error;
    }
    match(lex, quotech);
    while ((ch = gettoken(lex)) != EOF)
    {
        if (ch == quotech)
            break;
        else if (ch == '&')
        {
            ch = escapechar(lex, err);
            string_push(&str, ch, err);
        }
        else if (ch == '\n')
            goto parse_error;
        else
        {
            string_push(&str,ch, err);
            match(lex, ch);
        }
    }
    if (!match(lex, quotech))
        goto parse_error;
    return string_release(&str);
parse_error:
    free(string_release(&str));
    reporterror(err, "bad quoted string");
    return 0;
    
}

static char *textspan(LEXER *lex, ERROR *err)
{
   int ch;
   STRING str;

   string_init(&str);

   while ( (ch = gettoken(lex)) != EOF)
   {
      if (ch == '<')
          break;
      if (ch == '&')
        ch = escapechar(lex, err);
      else
         match(lex, ch);
      string_push(&str, ch, err);
   } 
   
    return string_release(&str);
}

static char *cdata(LEXER *lex, ERROR *err)
{
    char buff[4] = {0};
    int ch;
    int i;
    STRING str;
    int lineno;
    
    lineno = lex->lineno;
    
    string_init(&str);
    
    match(lex, '[');
    
    for (i =0; i < 3; i++)
    {
        ch = gettoken(lex);
        buff[i] = ch;
        match(lex, ch);
    }
    
    while ((ch = gettoken(lex)) != EOF)
    {
        string_push(&str, buff[0], err);
        buff[0] = buff[1];
        buff[1] = buff[2];
        buff[2] = ch;
        match(lex, ch);
        if (!strcmp(buff, "]]>"))
            return string_release(&str);
    }
    free (string_release(&str));
    reporterror(err, "unterminated CDATA tag (starts line %d)", lineno);
    
    return 0;
}

static char *processinginstruction(LEXER *lex, ERROR *err)
{
    int ch;
    STRING str;
    int lineno;
    
    string_init(&str);
    lineno = lex->lineno;
    
    match(lex, '?');
    
    while ((ch = gettoken(lex)) != EOF)
    {
        match(lex, ch);
        if (ch == '?')
        {
            if (gettoken(lex) == '>')
            {
                match(lex, '>');
                return string_release(&str);
            }
        }
    }
    reporterror(err, "<? tag not closed (starts line %d)", lineno);
    free (string_release(&str));
    
    return 0;
}

static char *attributename(LEXER *lex, ERROR *err)
{
    int ch;
    STRING str;

    string_init(&str);
    
    ch = gettoken(lex);
    if (!is_initidentifier(ch))
      goto parse_error;
    while (is_attributenamech(ch))
    {
       match(lex, ch);
       string_push(&str, ch, err);
       ch = gettoken(lex);
    }
    return string_release(&str);
 parse_error:
    free(string_release(&str));
    return 0;
}

static char *elementname(LEXER *lex, ERROR *err)
{
   int ch;
   STRING str;
    char *temp = 0;

   string_init(&str);
   
   ch = gettoken(lex);
   if (!is_initidentifier(ch))
     goto parse_error;
   while (is_elementnamech(ch))
   {
      match(lex, ch);
      string_push(&str, ch, err);
      ch = gettoken(lex);
   }
   return string_release(&str);
parse_error:
    temp = string_release(&str);
    free(temp);
   return 0;
}

static int escapechar(LEXER *lex, ERROR *err)
{
    int ch;
    STRING str;
    char *escaped = 0;
    int answer = 0;
    
    string_init(&str);
    
    ch = gettoken(lex);
    if (!match(lex, '&'))
        goto parse_error;
    string_push(&str, ch, err);
    while ( (ch = gettoken(lex)) != EOF)
    {
        string_push(&str, ch, err);
        match(lex, ch);
        if (ch == ';')
            break;
        if (ch == '\n')
            goto parse_error;
    }
    escaped = string_release(&str);
    if (!strcmp(escaped, "&amp;"))
        answer = '&';
    else if (!strcmp(escaped, "&gt;"))
        answer = '>';
    else if (!strcmp(escaped, "&lt;"))
        answer = '<';
    else if (!strcmp(escaped, "&quot;"))
        answer = '\"';
    else if (!strcmp(escaped, "&apos;"))
        answer = '\'';
    if (answer == 0)
        reporterror(err, "Unrecognised escape sequence %s", escaped);
    
    free(escaped);
    return answer;
parse_error:
    free(escaped);
    return 0;
}


/*
<!-- begins a comment, which ends with -->

<![CDATA[ begins a CDATA section, which ends with ]]>

<![INCLUDE[ and <![IGNORE[ begin conditional sections, which end with ]]>.

<!DOCTYPE begins a document type declaration.

<!ELEMENT begins an element type declaration.

<!ATTLIST begins an attribute list declaration.

<!ENTITY begins an entity declaration.

<!NOTATION begins a notation declaration.
*/

static int shriektype(LEXER *lex, ERROR *err)
{
    char buff[32] = {0};
    int ch;
    int i  = 0;
    
    match(lex, '!');
    buff[i++] = '!';
    
    while ((ch = gettoken(lex)) != EOF)
    {
        if (ch == '<' || ch == '>')
            return UNKNOWNSHRIEK;
        if (isspace(ch))
            return UNKNOWNSHRIEK;
        buff[i++] = ch;
        if (i >= sizeof(buff))
            break;
        if (!strcmp(buff, "!--"))
        {
            match(lex, ch);
            return COMMENT;
        }
        if (!strcmp(buff, "![CDATA["))
            return  CDATA;
        if (!strcmp(buff, "![INCLUDE["))
            return INCLUDE;
        if (!strcmp(buff, "![IGNORE["))
            return IGNORE;
        if (!strcmp(buff, "!DOCTYPE"))
        {
            match(lex, ch);
            return DOCTYPE;
        }
        if (!strcmp(buff, "!ELEMENT"))
        {
            match(lex, ch);
            return ELEMENT;
        }
        if (!strcmp(buff, "!ATTLIST"))
        {
            match(lex, ch);
            return ATTLIST;
        }
        if (!strcmp(buff, "NOTATION"))
        {
            match(lex, ch);
            return NOTATION;
        }
        match(lex, ch);
    }
    
    return UNKNOWNSHRIEK;
}

static void skipbom(LEXER *lex, ERROR *err)
{
    int ch;
    ch = gettoken(lex);
    if (ch == 0xEF)
    {
        match(lex, ch);
        if (!match(lex, 0xBB))
            reporterror(err, "input has bad byte order marker");
        if (!match(lex, 0xBF))
            reporterror(err, "input has bad byte order marker");
    }
}
               
static void skipunknowntag(LEXER *lex, ERROR *err)
{
    int ch;
    
    enterrecursion(err);
    while ((ch = gettoken(lex)) != EOF)
    {
        match(lex, ch);
        if (ch == '<')
            skipunknowntag(lex, err);
        if (ch == '>')
            break;
    }
    endrecursion(err);
}

static void skipwhitespace(LEXER *lex, ERROR *err)
{
   int ch = gettoken(lex);
   while (isspace(ch))
   {
     match(lex, ch);
     ch = gettoken(lex);
   }
}
static void initerror(ERROR *err)
{
    err->set = 0;
    err->message[0] = 0;
    err->lexer = 0;
    err->recursiondepth = 0;
}

static void enterrecursion(ERROR *err)
{
    err->recursiondepth++;
    if (err->recursiondepth > MAXRECURSIONLIMIT)
    {
        reporterror(err, "nesting too deep");
        if (err->lexer)
            match(err->lexer, EOF);
    }
}

static void endrecursion(ERROR *err)
{
    err->recursiondepth--;
    if (err->recursiondepth < 0)
        reporterror(err, "nesting problem");
}
           
static void reporterror(ERROR *err, const char *fmt, ...)
{
    char buff[1024];
    va_list args;
    
    if (!err->set)
    {
        va_start(args, fmt);
        vsnprintf(buff, 1024, fmt, args);
        va_end(args);
        if (err->lexer)
            snprintf(err->message, 1024, "Error line %d: %s.", err->lexer->lineno, buff);
        else
            strcpy(err->message, buff);
        err->set = 1;
    }
}

static void initlexer(LEXER *lex, ERROR *err, int (*getch)(void *), void *ptr)
{
  lex->getch = getch;
  lex->ptr = ptr;
  lex->err = err;
  lex->lineno = 0;
  lex->columnno = 0;
  lex->badmatch = 0;
  err->lexer = lex;
  /* hacked. Put a '<' sitting in the token becuase non-seekable UTF-16 streams
   need to read this character to determine data format */
    lex->token = '<'; //(*lex->getch)(lex->ptr);
  if (lex->token != EOF)
  {
    lex->lineno = 1;
    lex->columnno = 1; 
  }
  else
  {
     reporterror(lex->err, "Can't read data\n");
  }

}

static int gettoken(LEXER *lex)
{
    if (lex->badmatch)
        return EOF;
   return lex->token;
}

static int match(LEXER *lex, int token)
{
   if (lex->token == token)
   {
       if (lex->token == '\n')
       {
          lex->lineno++;
          lex->columnno = 1;
       }
      else
      {
        lex->columnno++;
      }
       lex->token = (*lex->getch)(lex->ptr);
      return 1;
   }
   else
   {
       lex->badmatch = 1;
     return 0;
   }
}

static char *mystrdup(const char *str)
{
  char *answer;

  answer = malloc(strlen(str) + 1);
  if(answer)
    strcpy(answer, str);

  return answer;
}


static void printnode_r(XMLNODE *node, int depth)
{
    int i;
    XMLNODE *child;
    
    for (i =0; i < depth; i++)
        printf("\t");
    printf("<%s>\n", node->tag);
    for (child = node->child; child; child = child->next)
        printnode_r(child, depth +1);
    for (i =0; i < depth; i++)
        printf("\t");
    printf("</%s>\n", node->tag);
}

static void printdocument(XMLDOC *doc)
{
    if (!doc)
        printf("Document was null\n");
    else if(!doc->root)
        printf("root null\n");
    else
        printnode_r(doc->root, 0);
}

int xmlparser2main(int argc, char **argv)
{
    XMLDOC *doc;
    
    char error[1024];
    if (argc == 1)
    {
        doc = xmldocfromstring("<!-- --><FRED attr=\"Fred\">Fred<![CDATA[character > data]]><JIM/>Bert<JIM/>Harry</FRED>", error, 1204);
        //printf("%s\n", doc->root->data);
    }else
    {
        doc = loadxmldoc(argv[1], error, 1024);
        if (doc)
            printf("%s\n", xml_getnesteddata(doc->root));
    }
    if (error[0])
        printf("%s\n", error);
    else
        printdocument(doc);
    
    return 0;
}

