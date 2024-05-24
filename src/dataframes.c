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

typedef struct field
{
   char *name;  /* name in CSV header */
   char *tag; /* tag of owning XML element */
   int attribute; /* set if the filed is an attribute */
   int datatype; /* type of data it is */
   char format[32];
   XMLNODE **nodes;
   XMLATTRIBUTE **attributes;
   struct field *next;
   struct field * child;
} FIELD;


char *getextension(char *fname);
void makelower(char *str);


int processdataframenode(FILE *fp, XMLNODE *node, int header)
{
    const char *name;
    const char *src;
    const char *xpath;
    
    char *ext;
    char error[1024];
    
    XMLDOC *doc = 0;
    
    name = xml_getattribute(node, "name");
    src = xml_getattribute(node, "src");
    xpath = xml_getattribute(node, "xpath");
    
    ext = getextension((char *)src);
    if (ext)
        makelower(ext);
    
    printf("ext ***%s***\n", ext);
    
    
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
    
    int Nselected = 0;
    XMLNODE **selnodes;
    int i;
    
    selnodes = xml_selectxpath(doc, "//init-param", &Nselected, error, 1024);
    printf("%d selected\n", Nselected);
    for (i = 0; selnodes[i];i++)
    {
        printf("%d %s\n", i, selnodes[i]->tag);
        char *path = xml_getnodepath(doc, NULL);
        printf("%p\n", path);
        printf("***%s***\n", path);
    }
    
    free(ext);
}

/*
int writedatafreme(FILE *fp, XMLDOC *doc, FIELD *fields)
{
    int i;
    int height;

    height = fillfields_r(doc, fields);
    for (i = 0; i < height)
    {
        fprintf(fp, "{");
        writefield_r(fields, i);
        fprintf(fp, "};\n");
    }
    
    return 0;
}

int writefield_r(FIELD *field, int row)
{
    while (field)
    {
        fprintf(fp, "%s, ", field->nodes[row]->data);
        field = field->next;
    }
}

int fillfields_r(XMLDOC *doc, FIELD *field)
{
    int N;
    char error[1024];
    
    while (field)
    {
       if (field->child)
           fillfields_r(root, field->child);
        else
        {
            field->nodes = xml_selectxpath(doc, field->tag, &N, error, 1024);
        }
        
        field = field->next;
    }
    
    return N;
}
 */

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


