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

typedef struct field
{
   char *name;  /* name in CSV header */
   char *tag; /* tag of owning XML element */
   int attribute; /* set if the field is an attribute */
   int datatype; /* type of data it is */
   char *xpath;
   char *ctype; /* type of data in C */
   int structexternal; /* set if the type is an externally defned structure */
   char *format; /* format string to pass to fprintf */
   enum PrintFType printftype;
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
int pointerwithspace(const char *type);
int fillfields_r(XMLDOC *doc, FIELD *field);

int determinefieldtype_r(FIELD *field);
int fieldshavesamenumbernodes_r(FIELD *field);


FIELD *createfield(void);
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
int writeformat(FILE *fp, const char *fmt, enum PrintFType pftype, const char *data);
enum PrintFType getformttype_pf(const char *fmt);
const char *getformattype(const char *fmt);
int Nformatspecifiers(const char *fmt);
static char *mystrconcat(const char *prefix, const char *suffix);
static char *mystrdup(const char *str);

int processdataframenode(FILE *fp, XMLNODE *node, int header)
{
    const char *namestr;
    const char *src;
    const char *xpath;
    const char *ctype;
    int useattributes = 1;
    int usechildren = 1;
    int external = 0;
    int i;
    
    char *ext = 0;
    char *name = 0;
    char *structname = 0;
    char error[1024];
    int err;
    
    XMLNODE *parent = 0;
    XMLNODE *child = 0;
    FIELD *fields =  0;
    XMLNODE **nodes;
    int Nnodes;
    
    
    XMLDOC *doc = 0;
    
    namestr = xml_getattribute(node, "name");
    src = xml_getattribute(node, "src");
    xpath = xml_getattribute(node, "xpath");
    ctype = xml_getattribute(node, "ctype");
    
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
    
    if (xml_getattribute(node, "external"))
    {
        external = parseboolean(xml_getattribute(node, "external"), &err);
        if (err)
        {
            fprintf(stderr, "dataframe element external attribute must be boolean\n");
        }
    }
    
    ext = getextension((char *)src);
    if (ext)
        makelower(ext);

    if (name)
      name = mystrdup(namestr);
    else if(src)
      name = getbasename((char *)src);
    else
    {
        fprintf(stderr, "No dataframe name specified\n");
        goto parse_error;
    }

    if (!strcmp(ext, ".csv"))
    {
        CSV *csv;
        FILE *tempfp;
        
        csv = loadcsv(src);
        if (!csv)
        {
            fprintf(stderr, "Can't load CSV  file %s\n", src);
            goto parse_error;
        }
        tempfp = tmpfile();
        if (!tempfp)
            goto out_of_memory;
        xml_fromCSV(tempfp, csv);
        fseek(tempfp, 0, SEEK_SET);
        doc = floadxmldoc(tempfp, error, 1024);
        if (!doc)
        {
            fprintf(stderr, "dataframe internal error %s\n", error);
            goto parse_error;
        }
        fclose(tempfp);
    }
    else if (!strcmp(ext, ".json"))
    {
        cJSON *json;
        FILE *tempfp;
        char *jsonstr;
        
        tempfp = fopen(src, "r");
        if (!tempfp)
            goto out_of_memory;
        jsonstr = fslurp(tempfp);
        fclose(tempfp);
        if (!jsonstr)
            goto out_of_memory;
       
        
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
        jsonstr = 0;
        tempfp = tmpfile();
        if (!tempfp)
            goto out_of_memory;
        xml_fromJSON(tempfp, json);
        fseek(tempfp, 0, SEEK_SET);
        doc = floadxmldoc(tempfp, error, 1024);
        if (!doc)
        {
            fprintf(stderr, "dataframe internal error %s\n", error);
            goto parse_error;
        }
        fclose(tempfp);
    }
    else if(!strcmp(ext, ".xml"))
    {
        doc = loadxmldoc(src, error, 1024);
        if (!doc)
        {
            fprintf(stderr, "Can't load xml file %s error %s\n", src, error);
            goto parse_error;
        }
    }
    else
    {
        fprintf(fp, "datafrsme unrecognised forat %s\n", ext);
        goto parse_error;
    }
    
  
    if (xml_Nchildrenwithtag(node, "field") > 0)
    {
        fields = fieldsfromnodes_r(node->child, ext);
    }
    else if (xpath)
    {
         if (!xml_xpath_isvalid(xpath, error, 1024))
         {
             fprintf(stderr, "Bad xpath attribute to dataframe element\n");
             fprintf(stderr, "%s\n", error);
         }
        nodes = xml_xpath_select(doc, xpath, &Nnodes, error, 1024);
        if (!nodes)
        {
            fprintf(stderr, "%s\n", error);
        }
        for (i = 1; i < Nnodes; i++)
        {
            if (!nodeshavesamestructure(nodes[0], nodes[i], useattributes, usechildren))
            {
                fprintf(stderr, "Not all nodes selected for dataframe have same structure\n");
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

    if (ctype)
        structname = mystrdup(ctype);
    else
        structname = mystrconcat("struct ", name);
    if (!structname)
        goto out_of_memory;
    
    if (!external)
        writestructuredefinition_r(fp, fields, structname);
    if (!header)
    {
        writedatafreme(fp, doc, fields, name, structname);
    }
    
    killxmldoc(doc);
    free(structname);
    free(ext);
    free(name);
    
    return 0;
    
out_of_memory:
    killxmldoc(doc);
    free(structname);
    free(ext);
    free(name);
    
    fprintf(stderr, "Out of memory");
    
    return -1;
    
parse_error:
    killxmldoc(doc);
    free(structname);
    free(ext);
    free(name);
    
    return  -2;
}

int writestructuredefinition_r(FILE *fp, FIELD *fields, const char *structname)
{
    FIELD *current;
    char *childstructname;
    
    for  (current = fields; current != NULL; current = current->next)
    {
        if (current->child)
        {
            if (current->ctype)
                childstructname = mystrdup(current->ctype);
            else
                childstructname = mystrconcat("struct ", current->name);
            if (!current->structexternal)
                writestructuredefinition_r(fp, current->child, childstructname);
            
            free(childstructname);
        }
    }
    
    fprintf(fp, "%s\n", structname);
    fprintf(fp, "{\n");
    writefieldstruct_r(fp, fields);
    fprintf(fp, "};\n\n");
}


int writedatafreme(FILE *fp, XMLDOC *doc, FIELD *fields, const char *name, const char *structname)
{
    int i;
    int height;

    height = fields->Nnodes;
    fprintf(fp, "%s %s[%d] =\n", structname, name, height);
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
            if (field->format)
            {
                writeformat(fp, field->format, field->printftype, field->attributes[row]->value);
            }
            else if (field->datatype == TYPE_STRING)
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
            if (field->format)
            {
                writeformat(fp, field->format, field->printftype, field->nodes[row]->data);
            }
            else if (field->datatype == TYPE_STRING)
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
          field->attributes = xml_xpath_selectattributes(doc, field->xpath, error, 1024);
          N = 0;
          for (i = 0; field->attributes[i]; i++)
            N++;
        }
        else
        {
            field->nodes = xml_xpath_select(doc, field->xpath, &N, error, 1024);
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
        if (field->ctype)
        {
            if (pointerwithspace(field->ctype))
            {
                fprintf(fp, "  %s%s;\n", field->ctype, field->name);
            }
            else
            {
                fprintf(fp, "  %s %s;\n", field->ctype, field->name);
            }
        }
        else if (field->datatype == TYPE_STRING)
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

int pointerwithspace(const char *type)
{
    char *asterisk;
    
    asterisk = strrchr(type, '*');
    if (!asterisk)
        return 0;
    if (asterisk[1] == 0)
    {
        while (*asterisk == '*' && asterisk != type)
            asterisk--;
        if (isspace((unsigned char) *asterisk))
            return 1;
    }
    
    return  0;
}

FIELD *createfield(void)
{
    FIELD *field;
    
    field = malloc(sizeof(FIELD));
    if (!field)
        return 0;
    field->name = 0;
    field->tag = 0;
    field->xpath = 0;
    field->attribute = 0;
    field->datatype = TYPE_UNKNOWN;
    field->ctype = 0;
    field->format = 0;
    field->printftype = PF_UNKNOWN;
    field->structexternal = 0;
    field->attributes = 0;
    field->nodes = 0;
    field->next = 0;
    field->child = 0;
    
    return field;
}

void killfield(FIELD *field)
{
    if (field)
    {
        free(field->name);
        free(field->tag);
        free(field->xpath);
        free(field->ctype);
        free(field->format);
        free(field->attributes);
        free(field->nodes);
        free(field);
    }
}

void killfields_r(FIELD *field)
{
    FIELD *next;
    while (field)
    {
        next = field->next;
        if (field->child)
            killfields_r(field->child);
        killfield(field);
        
        field = next;
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
          pathtonode = xml_xpath_getnodepath(doc, node);
          pathtonodea = mystrconcat(pathtonode, "@");
      }
      for (attr = node->attributes; attr; attr = attr->next)
      {
          field = createfield();
          field->name = mystrdup(attr->name);
          field->tag = mystrdup(node->tag);
          field->xpath = mystrconcat(pathtonodea, attr->name);
          field->attribute = 1;
          field->datatype = TYPE_UNKNOWN;
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
          field = createfield();
          
         field->name = mystrdup(child->tag);
         field->tag = mystrdup(node->tag);
         field->xpath = xml_xpath_getnodepath(doc, child);
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
out_of_memory:
    killfields_r(answer);
    fprintf(stderr, "Out of nemory\n");
    
    return 0;
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
    const char *ctype = 0;
    const char *formatstr = 0;
    enum PrintFType pftype;
    int err;
    
    field = createfield();
    if (!field)
        goto out_of_memory;
    
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
        if (xml_xpath_selectsattributes(xpath, 0, 0))
            field->attribute = 1;
    }
    else
    {
        field->xpath = mystrconcat("//", field->name);
        if (!field->xpath)
            goto out_of_memory;
    }
    
    ctype = xml_getattribute(node, "ctype");
    if (ctype)
    {
        field->ctype = mystrdup(ctype);
        if (!field->ctype)
            goto out_of_memory;
    }
    
    formatstr = xml_getattribute(node, "format");
    if (formatstr)
    {
        if (Nformatspecifiers(formatstr) != 1)
        {
            fprintf(stderr, "field element format attribute must specify one field\n");
        }
        else
        {
            pftype = getformttype_pf(formatstr);
            if (pftype == PF_UNKNOWN)
            {
                fprintf(stderr, "field element format attribute doesn't specify supported type\n");
            }
            else
            {
                field->format = mystrdup(formatstr);
                if (!field->format)
                    goto out_of_memory;
                field->printftype = pftype;
            }
        }
    }
    
    if (xml_getattribute(node, "external"))
    {
        field->structexternal = parseboolean(xml_getattribute(node, "external"), &err);
        if (err)
        {
            fprintf(stderr, "field element external attribute must be boolean\n");
        }
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

int writeformat(FILE *fp, const char *fmt, enum PrintFType pftype, const char *data)
{
    switch (pftype)
    {
        case PF_UNKNOWN:
            fprintf(fp, "%s", data);
            break;
        case PF_INT:
            fprintf(fp, fmt, (int) strtol(data, 0, 10));
            break;
        case PF_DOUBLE:
            fprintf(fp, fmt, strtod(data, 0));
            break;
        case PF_CHAR_STAR:
            fprintf(fp, fmt, data);
            break;
        case PF_LONG_INT:
            fprintf(fp, fmt, strtol(data, 0, 10));
            break;
        case PF_LONG_LONG_INT:
            fprintf(fp, fmt, strtoll(data, 0, 10));
            break;
        case PF_LONG_DOUBLE:
            fprintf(fp, fmt, strtold(data, 0));
            break;
        case PF_SIZE_T:
            fprintf(fp, fmt, (size_t) strtoll(data, 0, 10));
            break;
    }
    fprintf(fp, ", ");
}

enum PrintFType getformttype_pf(const char *fmt)
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
