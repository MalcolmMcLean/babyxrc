//
//  dataframes.c
//  babyxrc
//
//  Created by Malcolm McLean on 24/05/2024.
//

#include "dataframes.h"
#include "xpath.h"
#include "xmlconverter.h"
#include "csv.h"
#include "cJSON.h"
#include "asciitostring.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TYPE_UNKNOWN 0
#define TYPE_NUMBER 1
#define TYPE_STRING 2

typedef struct field
{
   char *name;  /* name in CSV header */
   char *tag; /* tag of owning XML element */
   int attribute; /* set if the filed is an attribute */
   int datatype; /* type of data it is */
   char *xpath;
   char format[32];
   XMLNODE **nodes;
   XMLATTRIBUTE **attributes;
   int Nnodes;
   struct field *next;
   struct field * child;
} FIELD;

int writestructuredefinition_r(FILE *fp, FIELD *fields, const char *structname);
int writedatafreme(FILE *fp, XMLDOC *doc, FIELD *fields, const char *name, const char *structname);

int writefield_r(FILE *fp, FIELD *field, int row);
void writefieldstruct_r(FILE *fp, FIELD *field);
int fillfields_r(XMLDOC *doc, FIELD *field);

int determinefieldtype_r(FIELD *field);
int fieldshavesamenumbernodes_r(FIELD *field);

FIELD *getfields_r(XMLNODE *node, XMLDOC *doc, int useattributes, int usechildren);
FIELD *fieldsfromnodes_r(XMLNODE *node, const char *format);
FIELD *processfieldnode(XMLNODE *node, const char *format);
int nodeshavesamestructure(XMLNODE *nodea, XMLNODE *nodeb, int useattributes, int usechildren);
XMLNODE *chooseparentnode(XMLNODE *node, int useattributes, int usechildren);

int parseboolean(const char *boolattribute, int *error);
char *getbasename(char *fname);
char *getextension(char *fname);
void makelower(char *str);

int guessdatatype(XMLNODE **nodes, int N);
int guessdatataattributes(XMLATTRIBUTE **attr, int N);
int getdatatype(const char *str);
static char *mystrconcat(const char *prefix, const char *suffix);
static char *mystrdup(const char *str);

int processdataframenode(FILE *fp, XMLNODE *node, int header)
{
    const char *name;
    const char *src;
    const char *xpath;
    int useattributes = 1;
    int usechildren = 1;
    int i;
    
    char *ext;
    char error[1024];
    int err;
    
    XMLDOC *doc = 0;
    
    name = xml_getattribute(node, "name");
    src = xml_getattribute(node, "src");
    xpath = xml_getattribute(node, "xpath");
    
    if (xml_getattribute(node, "useattributes"))
    {
        useattributes = parseboolean(xml_getattribute(node, "useattributes"), &err);
        if (err)
        {
            fprintf(stderr, "dataframe element useattributes attribute must be boolean\n");
        }
    }
    
    if (xml_getattribute(node, "usechildren"))
    {
        usechildren = parseboolean(xml_getattribute(node, "usechildren"), &err);
        if (err)
        {
            fprintf(stderr, "dataframe element usechildren attribute must be boolean\n");
        }
    }
    
    ext = getextension((char *)src);
    if (ext)
        makelower(ext);
    
    if(name)
      name = mystrdup(name);
    else if(src)
      name = getbasename((char *)src);
    else
    {
        fprintf(stderr, "No dataframe name specified\n");
        return -1;
    }
    
    
    if (!strcmp(ext, ".csv"))
    {
        CSV *csv;
        FILE *tempfp;
        
        csv = loadcsv(src);
        tempfp = tmpfile();
        xml_fromCSV(tempfp, csv);
        fseek(tempfp, 0, SEEK_SET);
        doc = floadxmldoc2(tempfp, error, 1024);
        fclose(tempfp);
    }
    else if (!strcmp(ext, ".json"))
    {
        cJSON *json;
        FILE *tempfp;
        char *jsonstr;
        
        tempfp = fopen(src, "r");
        jsonstr = fslurp(tempfp);
        fclose(tempfp);
        
        json = cJSON_Parse(jsonstr);
        xml_fromJSON(stdout, json);
        
        tempfp = tmpfile();
        xml_fromJSON(tempfp, json);
        fseek(tempfp, 0, SEEK_SET);
        doc = floadxmldoc2(tempfp, error, 1024);
        fclose(tempfp);
        
        printf("Doc %p\n", doc);
        printf("error %s\n", error);
    }
    else if(!strcmp(ext, ".xml"))
    {
        doc = loadxmldoc2(src, error, 1024);
    }
    
    XMLNODE *parent = 0;
    XMLNODE *child = 0;
    FIELD *fields =  0;
    XMLNODE **nodes;
    int Nnodes;
    
    if (xml_Nchildrenwithtag(node, "field") > 0)
    {
        fields = fieldsfromnodes_r(node->child, ext);
    }
    else if (xpath)
    {
         if (!xml_xpathvalid(xpath, error, 1024))
         {
             fprintf(stderr, "Bad xpath attribute to dataframe element\n");
             fprintf(stderr, "%s\n", error);
         }
        nodes = xml_selectxpath(doc, xpath, &Nnodes, error, 1024);
        if (!nodes)
        {
            fprintf(stderr, "%s\n", error);
        }
        for (i = 1; i < Nnodes; i++)
        {
            if (!nodeshavesamestructure(nodes[0], nodes[i], useattributes, usechildren))
            {
                fprintf(stderr, "Not all nodes selected for datframe have same structure\n");
            }
        }
        fields = getfields_r(nodes[0], doc, useattributes, usechildren);
        free (nodes);
    
    }
    else
    {
        parent = chooseparentnode(doc->root, useattributes, usechildren);
        fields = getfields_r(parent->child, doc, useattributes, usechildren);
    }
    fillfields_r(doc, fields);
    determinefieldtype_r(fields);
    if (fieldshavesamenumbernodes_r(fields) == -1)
    {
        fprintf(stderr, "xpath matches different number of data items\n");
        return -1;
    }
    
    writestructuredefinition_r(fp, fields, name);
    if (!header)
    {
        writedatafreme(fp, doc, fields, name, name);
    }
    
    killxmldoc(doc);
    free(ext);
    
    return 0;
}

int writestructuredefinition_r(FILE *fp, FIELD *fields, const char *structname)
{
    FIELD *current;
    
    for  (current = fields; current != NULL; current = current->next)
    {
        if (current->child)
            writestructuredefinition_r(fp, current->child, current->name);
    }
    
    fprintf(fp, "struct %s\n", structname);
    fprintf(fp, "{\n");
    writefieldstruct_r(fp, fields);
    fprintf(fp, "};\n\n");
}


int writedatafreme(FILE *fp, XMLDOC *doc, FIELD *fields, const char *name, const char *structname)
{
    int i;
    int height;

    height = fields->Nnodes;
    fprintf(fp, "struct %s %s[%d] =\n", name, structname, height);
    fprintf(fp, "{\n");
    for (i = 0; i < height; i++)
    {
        fprintf(fp, "  {");
        writefield_r(fp, fields, i);
        fprintf(fp, "},\n");
    }
    fprintf(fp, "};\n\n");
    
    return 0;
}

int writefield_r(FILE *fp, FIELD *field, int row)
{
    char *cstring;
    
    while (field)
    {
        if (field->attribute)
        {
            if (field->datatype == TYPE_STRING)
            {
                cstring = texttostring(field->attributes[row]->value);
                fprintf(fp, "%s, ", cstring);
                free(cstring);
            }
            else if (field->datatype == TYPE_NUMBER)
            {
                fprintf(fp, "%s, ", field->attributes[row]->value);
            }
        }
        else
        {
            if (field->datatype == TYPE_STRING)
            {
                cstring = texttostring(field->nodes[row]->data);
                fprintf(fp, "%s, ", cstring);
                free(cstring);
            }
            else if (field->datatype == TYPE_NUMBER)
            {
                fprintf(fp, "%s, ", field->nodes[row]->data);
            }
        }
        if (field->child)
        {
            fprintf(fp, "{");
            writefield_r(fp, field->child, row);
            fprintf(fp, "},");
        }
        field = field->next;
    }
    
    return 0;
    
out_of_memory:
    fprintf(stderr, "Out of memory\n");
    return 0;
}

int fillfields_r(XMLDOC *doc, FIELD *field)
{
    int N = -1;
    char error[1024];
    int i;
    
    while (field)
    {
        field->Nnodes = 0;
       if (field->child)
           fillfields_r(doc, field->child);
    
       if (field->attribute)
       {
          field->attributes = xml_xpathgetattributes(doc, field->xpath, error, 1024);
          N = 0;
          for (i = 0; field->attributes[i]; i++)
            N++;
        }
        else
        {
            field->nodes = xml_selectxpath(doc, field->xpath, &N, error, 1024);
        }
        
        field->Nnodes = N;

        field = field->next;
    }
    
    return N;
}

int determinefieldtype_r(FIELD *field)
{
    while (field)
    {
        if (field->datatype == TYPE_UNKNOWN)
        {
            if (field->attribute)
                field->datatype = guessdatataattributes(field->attributes, field->Nnodes);
            else
                field->datatype = guessdatatype(field->nodes, field->Nnodes);
        }
        
        if (field->child)
            determinefieldtype_r(field->child);
        
        field = field->next;
    }
}

int fieldshavesamenumbernodes_r(FIELD *field)
{
    int N = -1;
    
    while (field)
    {
        if (!field->child)
        {
            if (N == -1)
                N = field->Nnodes;
            else if (N != field->Nnodes)
                return -1;
        }
        else
        {
            if (N == -1)
                N = fieldshavesamenumbernodes_r(field->child);
            else if (N != fieldshavesamenumbernodes_r(field->child))
                return -1;
        }
                 
        field = field->next;
    }
    
    return N;
}



int xdeterminefieldtype_r(XMLNODE *node, FIELD *fields, int
useattributes, int usechildren)
{
   int i = 0;
    XMLATTRIBUTE *attr;
    XMLNODE *child;
    FIELD *target = 0;
    FIELD *temp;
    int type;

   if (useattributes)
   {
       for (attr = node->attributes; attr; attr = attr->next)
       {
           for (temp = fields; temp !=  NULL; temp = temp->next)
               if (temp->attribute && !strcmp(attr->name, temp->name))
                   break;
           if (temp)
               break;
       }
       if (attr && temp)
       {
         type = getdatatype(attr->value);
         if (temp->datatype == TYPE_UNKNOWN)
           temp->datatype = type;
         else if (temp->datatype == TYPE_NUMBER
            && type == TYPE_STRING)
             temp->datatype = type;
      }
   }
   
   if (usechildren)
   {
      for (child = node->child; child; child = child->next)
      {
          for (temp = fields; temp !=  NULL; temp = temp->next)
              if (!temp->attribute  && !strcmp(child->tag, temp->tag))
                  break;
          if (temp)
              break;
      }
      if (child && temp)
      {
           type = getdatatype(child->data);
         if (temp->datatype == TYPE_UNKNOWN)
           temp->datatype = type;
         else if (temp->datatype == TYPE_NUMBER
            && type == TYPE_STRING)
            temp->datatype = type;

        xdeterminefieldtype_r(child, temp->child, useattributes,
usechildren);
      }
   }
}

void writefieldstruct_r(FILE *fp, FIELD *field)
{
    while (field)
    {
        if (field->datatype == TYPE_STRING)
        {
            fprintf(fp, "  char *%s;\n", field->name);
        }
        else if (field->datatype == TYPE_NUMBER)
        {
            fprintf(fp, "  double %s;\n", field->name);
        }
        
        if (field->child)
        {
            if (field->datatype == TYPE_UNKNOWN)
                fprintf(fp, "  struct %s %s;\n", field->name, field->name);
            else
                fprintf(fp, "  struct %s %s_x;\n", field->name, field->name);
        }
        field = field->next;
    }
}


FIELD *getfields_r(XMLNODE *node, XMLDOC *doc, int useattributes, int usechildren)
{
    FIELD *answer = 0;
    FIELD *field;
    FIELD *temp;
    FIELD *reverse = 0;
    
    XMLATTRIBUTE *attr;
    XMLNODE *child;
    int type;
    
    char *pathtonode;
    char *pathtonodea;
    
   if (useattributes)
   {
      if (node->attributes)
      {
          pathtonode = xml_getnodepath(doc, node);
          pathtonodea = mystrconcat(pathtonode, "@");
      }
      for (attr = node->attributes; attr; attr = attr->next)
      {
          field = malloc(sizeof(FIELD));
          field->name = mystrdup(attr->name);
          field->tag = mystrdup(node->tag);
          field->xpath = mystrconcat(pathtonodea, attr->name);
          field->attribute = 1;
          field->datatype = TYPE_UNKNOWN;
          field->attributes = 0;
          field->nodes = 0;
          field->next = answer;
          field->child = 0;
          answer = field;
      }
       if (node->attributes)
       {
           free(pathtonode);
           free(pathtonodea);
           pathtonode = 0;
           pathtonodea = 0;
       }
   }
   if (usechildren)
   {
      for (child = node->child; child; child = child->next)
      {
          field = malloc(sizeof(FIELD));
          
         field->name = mystrdup(child->tag);
         field->tag = mystrdup(node->tag);
         field->xpath = xml_getnodepath(doc, child);
         field->attribute = 0;
         field->datatype = TYPE_UNKNOWN;
          field->attributes = 0;
          field->nodes = 0;
          field->next = answer;
          field->child = getfields_r(child, doc, useattributes, usechildren);;
          answer = field;
      }
   }
    
   while (answer)
   {
         temp = answer;
       answer = answer->next;
       temp->next = reverse;
       reverse = temp;
   }
    answer = reverse;
    
    return answer;
}

FIELD *fieldsfromnodes_r(XMLNODE *node, const char *format)
{
    FIELD *answer = 0;
    FIELD *temp;
    FIELD *reverse = 0;
    
    while (node)
    {
        if (!strcmp(node->tag, "field"))
        {
            temp = processfieldnode(node, format);
            if (temp)
            {
                if (node->child)
                    temp->child = fieldsfromnodes_r(node->child, format);
                temp->next = answer;
                answer = temp;
            }
        }
        node = node->next;
    }
    
    while (answer)
    {
        temp = answer;
        answer = answer->next;
        temp->next = reverse;
        reverse = temp;
    }
     answer = reverse;
    return answer;

}

FIELD *processfieldnode(XMLNODE *node, const char *format)
{
    FIELD *field;
    const char *name = 0;
    const char *xpath = 0;
    
    field = malloc(sizeof(FIELD));
    if (!field)
        goto out_of_memory;
    
    field->name = 0;  /* name in CSV header */
    field->tag = 0; /* tag of owning XML element */
    field->attribute = 0; /* set if the filed is an attribute */
    field->datatype = TYPE_UNKNOWN; /* type of data it is */
    field->xpath = 0;
    field->format[0] = 0;
    field->nodes = 0;
    field->attributes =0;
    field->next = 0;
    field->child = 0;
    
    name = xml_getattribute(node, "name");
    if (!name)
    {
        fprintf(stderr, "No field name specified\n");
        goto error_exit;
    }
    field->name = mystrdup(name);
    if (!field->name)
        goto out_of_memory;
    
    xpath = xml_getattribute(node, "xpath");
    if (xpath)
    {
        field->xpath = mystrdup(xpath);
        if (!field->xpath)
            goto out_of_memory;
        if (xml_xpathselectsattributes(xpath, 0, 0))
            field->attribute = 1;
    }
    else
    {
        field->xpath = mystrconcat("//", field->name);
        if (!field->xpath)
            goto out_of_memory;
    }
    
    return field;
out_of_memory:
    fprintf(stderr, "Out of memory\n");
    return 0;
error_exit:
    return 0;
}


/*
  test if two nodes have the same structure. Do the attributes and the hierarchy
    under them match, and are the element tag names the same?
  Params:
     nodea- the first node
     nodeb - the second node
     useattributes - if set, check that the attribute lists match
     usechildren - if set, check that the children match
Returns 1 if the nodes have the same struture, else 0
 */
int nodeshavesamestructure(XMLNODE *nodea, XMLNODE *nodeb, int useattributes, int usechildren)
{
   XMLATTRIBUTE *attra, *attrb;
   XMLNODE *childa, *childb;

   if (strcmp(nodea->tag, nodeb->tag))
      return 0;

   if (useattributes)
   {
      attra = nodea->attributes;
      attrb = nodeb->attributes;
      while (attra && attrb)
      {
         if (strcmp(attra->name, attrb->name))
           return 0;
         attra = attra->next;
         attrb = attrb->next;
      }
      if (attra != NULL || attrb != NULL)
         return 0;
   }
   if (usechildren)
   {
      childa = nodea->child;
      childb = nodeb->child;
      while (childa && childb)
      {
        if (!nodeshavesamestructure(childa, childb, useattributes, usechildren))
          return 0;
        childa = childa->next;
        childb = childb->next;
     }
     if (childa != NULL || childb != NULL)
        return 0;
   }
   
   return 1;
}

/*
   Get the number of children with the same structure
   Params:
      node - the node to test
      useattributes - if set, consider attributes of children
      usechildren - if set, consider descendants of children
   Returns: 0 if the node has mixed childrem otherwise the number of children
 */
int Nchildrenwithsamestructure(XMLNODE *node, int useattributes, int usechildren)
{
   int answer = 0;
   XMLNODE *child;

    for (child = node->child; child; child = child->next)
   {
      if (nodeshavesamestructure(node->child, child, useattributes, usechildren))
        answer++;
      else
        return 0;
   }
    
    return answer;
}

/*
   choose  a parent node (recursive)
     Params:
        node - the node to test
        useattributes - consider attributes
        usechildren - consider children
        best  the best node found so far
     Returns: the best descendant to use as the parent node.
     Notes: it goes throuhg the tree looking for the node with the largest number of identical structure children.
 */
XMLNODE *chooseparentnode_r(XMLNODE *node, int useattributes, int usechildren, int best)
{
    XMLNODE *bestnode = 0;
    XMLNODE *testnode;
    XMLNODE *child;
    int Ngoodchildren;
    

    Ngoodchildren = Nchildrenwithsamestructure(node, useattributes, usechildren);
    
    if (Ngoodchildren > best)
    {
        bestnode = node;
        best = Ngoodchildren;
    }
    for (child = node->child; child; child = child->next)
    {
        testnode = chooseparentnode_r(child, useattributes, usechildren, best);
        if (testnode)
        {
            Ngoodchildren = Nchildrenwithsamestructure(testnode, useattributes, usechildren);
            if (Ngoodchildren > best)
            {
                bestnode = testnode;
                best = Ngoodchildren;
            }
        }
    }
    
    return bestnode;
}

/*
    Fish for a parent node for the CSV data
    Params: node - the node
            useattributes - consder attributes
            usehildren - consider children
    Returns: the node to use for th parent, NULL i dthere isnt a suitable one.
    Note: the parent node is th enode with most children of identical structure.
 */
XMLNODE *chooseparentnode(XMLNODE *node, int useattributes, int usechildren)
{
    return chooseparentnode_r(node, useattributes, usechildren, 0);
}

int guessdatataattributes(XMLATTRIBUTE **attr, int N)
{
    int answer = TYPE_UNKNOWN;
    int type;
    int i;
    
    for (i = 0; i < N; i++)
    {
        type = getdatatype(attr[i]->value);
        if (answer == TYPE_UNKNOWN)
            answer = type;
        if (answer == TYPE_NUMBER && type == TYPE_STRING)
            answer = TYPE_STRING;
    }
        
    return answer;
}

int guessdatatype(XMLNODE **nodes, int N)
{
    int answer = TYPE_UNKNOWN;
    int type;
    int i;
    
    for (i = 0; i < N; i++)
    {
        type = getdatatype(nodes[i]->data);
        if (answer == TYPE_UNKNOWN)
            answer = type;
        if (answer == TYPE_NUMBER && type == TYPE_STRING)
            answer = TYPE_STRING;
    }
        
    
    return answer;
}

int getdatatype(const char *str)
{
    char *end;
    int i;
    
    if (str == NULL)
        return TYPE_UNKNOWN;
    
    for (i =0; str[i]; i++)
        if (!isspace((unsigned char)str[i]))
            break;
    if (!str[i])
        return TYPE_UNKNOWN;
    
    strtod(str, &end);
    if (*end == 0)
        return TYPE_NUMBER;
    return TYPE_STRING;
}

static char *mystrconcat(const char *prefix, const char *suffix)
{
    int lena, lenb;
    char *answer;
    
    lena = (int) strlen(prefix);
    lenb = (int) strlen(suffix);
    answer = malloc(lena + lenb + 1);
    if (answer)
    {
        strcpy(answer, prefix);
        strcpy(answer + lena, suffix);
    }
    return  answer;
}

static char *mystrdup(const char *str)
{
    char *answer = malloc(strlen(str) +1);
    if (answer)
        strcpy(answer, str);
    return answer;
}
