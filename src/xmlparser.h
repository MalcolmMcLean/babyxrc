#ifndef xmlparser_h
#define xmlparser_h

#include <stdio.h>

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
  struct xmlnode *next;      /* sibling node */
  struct xmlnode *child;     /* first child node */
} XMLNODE;

typedef struct
{
  XMLNODE *root;             /* the root node */
} XMLDOC;


XMLDOC *loadxmldoc(char *fname, int *err);
XMLDOC *floadxmldoc(FILE *fp, int *err);
void killxmldoc(XMLDOC *doc);

XMLNODE *xml_getroot(XMLDOC *doc);
const char *xml_gettag(XMLNODE *node);
const char *xml_getdata(XMLNODE *node);
const char *xml_getattribute(XMLNODE *node, const char *attr);
int xml_Nchildren(XMLNODE *node);
int xml_Nchildrenwithtag(XMLNODE *node, const char *tag);
XMLNODE *xml_getchild(XMLNODE *node, const char *tag, int index);
XMLNODE **xml_getdescendants(XMLNODE *node, const char *tag, int *N);

#endif
