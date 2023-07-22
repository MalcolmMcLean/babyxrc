/*
  XML Vanilla Parser, by Malcolm McLean

  version 1.0

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "xmlparser.h"

XMLNODE *floadxmlnode(FILE *fp, char *tag, int *err);
void killxmlnode(XMLNODE *node);

static XMLATTRIBUTE *floadattributes(FILE *fp, int *err);
static void killxmlattribute(XMLATTRIBUTE *attr);
static char *getquotedstring(FILE *fp, int *err);
static char *getdata(FILE *fp, int *err);
static char *getidentifier(FILE *fp, int *err);
static char *unescapedata(char *data);
static char *mystrdup(const char *str);

#include <assert.h>

/*
  load an XML documents, given a filename
*/
XMLDOC *loadxmldoc(char *fname, int *err)
{
  int errplaceholder;
  XMLDOC *answer;
  FILE *fp;

  if(err == 0)
    err = &errplaceholder;
  *err = 0;
  fp = fopen(fname, "r");
  if(!fp)
  {
    *err = -3;
	return 0;
  }
  answer = floadxmldoc(fp, err);
  fclose(fp);

  return answer;

}

/*
  load document from an open stream
*/
XMLDOC *floadxmldoc(FILE *fp, int *err)
{
  XMLDOC *answer;
  int ch;
  int ch1, ch2;
  XMLNODE *rootnode;
  char *tag;
  char *closetag;
  long pos;

  pos = ftell(fp);
  ch1 = fgetc(fp);
  ch2 = fgetc(fp);
  if(ch1 == '<' && ch2 == '?')
  {
    while( (ch = fgetc(fp)) != EOF)
	  if(ch == '>')
	    break;
  }
  else if(ch1 == '<' && isalpha(ch2))
  {
	  fseek(fp, pos, SEEK_SET);
  }
  else
  {
    fseek(fp, pos, SEEK_SET);
	*err = -4;
	return 0;
  }
 

  answer = malloc(sizeof(XMLDOC));
  if(!answer)
    goto out_of_memory;

  answer->root = 0;
  

  while( (ch = fgetc(fp)) != EOF)
  {
    if(ch == '<')
	{
	  ch = fgetc(fp);
	  if(isalpha(ch))
	  {
	    ungetc(ch, fp);
        tag = getidentifier(fp, err);
		if(tag)
		{
		  rootnode = floadxmlnode(fp, tag, err);
		  if(rootnode == 0)
		    goto parse_error;
		  answer->root = rootnode;
		  closetag = getidentifier(fp, err);
		  if(strcmp(tag, closetag))
		    goto parse_error;
		  ch = fgetc(fp);
		  if(ch != '>')
		    goto parse_error;
		  free(tag);
		  free(closetag);
		  break;
		}
	  }
	  else
	  {
	    while( (ch = fgetc(fp)) != EOF)
		  if(ch == '>')
		    break;
	  }
	}
  }
  while( (ch = fgetc(fp)) != EOF)
    if(!isspace(ch))
	{
	  ungetc(ch, fp);
	  break;
	}
  return answer;
parse_error:
  if(*err == 0)
    *err = -2;
  return 0;
out_of_memory:
  if(*err == 0)
    *err = -1;
  return 0;
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


/*
  load a node
    err = 0 = Ok, -1 = out of memory, -2 = parse error
  Notes: recursive
*/
XMLNODE *floadxmlnode(FILE *fp, char *tag, int *err)
{
  XMLNODE *node = 0;
  XMLNODE *child = 0;
  XMLNODE *lastchild = 0;
  char *newtag = 0;
  char *closetag = 0;
  char *data = 0;
  int ch;

  node = malloc(sizeof(XMLNODE));
  if(!node)
    goto out_of_memory;
  node->tag = 0;
  node->data =0;
  node->attributes = 0;
  node->child = 0;
  node->next = 0;
  node->position = 0;

  node->tag = mystrdup(tag);
  if(!node->tag)
    goto out_of_memory;

  while( (ch = fgetc(fp)) != EOF )
    if(!isspace( (unsigned char) ch))
	  break;
  if(ch != '>')
  {
    if(ch == '/')
	  return node;
	ungetc(ch, fp);
    node->attributes = floadattributes(fp, err);
	if(!node->attributes)
      goto parse_error;
	ch = fgetc(fp);
	if(ch == '/')
		return node;
  }
  data = getdata(fp, err);
  if(*err)
    goto parse_error;
  if(!node->data)
    node->data = data;
  else
  {
	  node->data = realloc(node->data, strlen(node->data) + strlen(data) + 1);
	  strcat(node->data, data);
  }
  while(ch != EOF)
  {
    while( (ch = fgetc(fp)) != EOF)
      if(!isspace( (unsigned char) ch))
	    break;
    if(ch == '<')
    {
      ch = fgetc(fp);
	  if(ch == '/')
	  {
	    return node;
	  }
	  else if(isalpha(ch))
	  {
	    ungetc(ch, fp);
	    newtag = getidentifier(fp, err);
        
	    child = floadxmlnode(fp, newtag, err);
	    if(!child)
	      goto parse_error;
		if(node->data)
		  child->position = strlen(node->data);
	    closetag = getidentifier(fp, err);
		ch = fgetc(fp);
		if(ch != '>')
		  goto parse_error;
		if(closetag == 0 && *err == 0)
			;
	    else if(strcmp(newtag, closetag))
          goto parse_error;	    
		if(lastchild)
		  lastchild->next = child;
		else
			node->child = child;
		lastchild = child;
		child = 0;
		free(newtag);
		free(closetag);
		newtag = 0;
		closetag = 0;
	  }
	  else
	  {
	    while( (ch = fgetc(fp)) != EOF)
		  if(ch == '>')
		    break;
	  }
	}
  }
parse_error:
  killxmlnode(node);
  killxmlnode(child);
  if(*err == 0)
    *err = -2;
  return 0;
out_of_memory:
  killxmlnode(node);
  killxmlnode(child);
  *err = -1;
  return 0;
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
  parse attributes. they come in the form

   <TAG firstname = "Fred" lastname = 'Bloggs'>
   ot <TAG quote = '"' age="11"/>
*/
static XMLATTRIBUTE *floadattributes(FILE *fp, int *err)
{
  int ch;
  XMLATTRIBUTE *answer = 0;
  XMLATTRIBUTE *nextattr = 0;
  XMLATTRIBUTE *tail = 0;
  char *name = 0;
  char *value = 0;

  while( (ch = fgetc(fp)) != EOF )
  {
    if(isspace(ch))
	  continue;
    if(isalpha(ch))
	{
	  ungetc(ch, fp);
	  if(nextattr == 0)
	  {
	    nextattr = malloc(sizeof(XMLATTRIBUTE));
		if(!nextattr)
		  goto out_of_memory;
		nextattr->name = 0;
		nextattr->next = 0;
		nextattr->value = 0;
	    name = getidentifier(fp, err);
		if(*err)
		  goto parse_error;
		nextattr->name = name;
		name = 0;
	  }
	  else
	    goto parse_error;
	}
	if(ch == '=')
	{
	  if(nextattr == 0)
	    goto parse_error;
	  value = getquotedstring(fp, err);
	  if(*err)
		  goto parse_error;
	  nextattr->value = value;
	  value = 0;
	  if(answer == 0)
	    answer = nextattr;
	  else
		  tail->next = nextattr;
	  tail = nextattr;
	  nextattr = 0;
	}
	if(ch == '>' || ch == '/')
	{
	  ungetc(ch, fp);
	  return answer;
	}
  }
parse_error:
  free(value);
  free(name);
  killxmlattribute(answer);
  killxmlattribute(nextattr);
  if(*err == 0)
    *err = -2;
  return 0;
out_of_memory:
  free(value);
  free(name);
  killxmlattribute(answer);
  killxmlattribute(nextattr);
  *err = -1;
  return 0;
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

/*
  get a quote string
  Notes: fp must poit to first quote character.
  Both double and single quote allowed
*/
static char *getquotedstring(FILE *fp, int *err)
{
  char *answer;
  char *temp;
  int buffsize = 256;
  char quote;
  int ch;
  int N = 0;

  answer = malloc(buffsize);
  if(!answer)
    goto out_of_memory;

  while( (ch = fgetc(fp)) != EOF )
    if(!isspace(ch))
	  break;

  quote = ch;
  if(quote != '\"' && quote != '\'')
    goto parse_error;

  while( (ch = fgetc(fp)) != EOF)
  {
    if(ch == quote)
	{
		answer[N] = 0;
		temp = unescapedata(answer);
		free(answer);
		return temp;
	}
	else
	{
	  answer[N++] = ch;
	  if(N > buffsize -1)
	  {
	    temp = realloc(answer, buffsize * 2);
		if(!temp)
		  goto out_of_memory;
		answer = temp;
		buffsize = buffsize * 2;
	  }
	}
  }
parse_error:
  free(answer);
  *err = -2;
  return 0;
out_of_memory:
  free(answer);
  *err = -1;
  return 0;
}

/*
  get data field for an xml node
  Notes: the specifications for whitespace are
    a bit loose, so we retain it 
*/
static char *getdata(FILE *fp, int *err)
{
  char *answer;
  char *temp;
  int buffsize = 256;
  int ch;
  int N = 0;

  answer = malloc(buffsize);
  if(!answer)
    goto out_of_memory;
  while( (ch = fgetc(fp)) != EOF)
  {
    if(ch == '<')
	{
		ungetc('<', fp);
		answer[N] = 0;
		temp = unescapedata(answer);
		free(answer);
		return temp;
	}
	else
	{
	  answer[N++] = ch;
	  if(N > buffsize -1)
	  {
	    temp = realloc(answer, buffsize * 2);
		if(!temp)
		  goto out_of_memory;
		answer = temp;
		buffsize = buffsize * 2;
	  }
	}
  }
parse_error:
  free(answer);
  *err = -2;
  return 0;
out_of_memory:
  free(answer);
  *err = -1;
  return 0;
}

/*
  get an identifer for the stream
  Notes: return null if not a valid
    identifier but doesn't set err.
  This is because of the closing tag problem
*/
static char *getidentifier(FILE *fp, int *err)
{
  char *answer;
  char *temp;
  int buffsize = 32;
  int N = 0;
  int ch;

  answer = malloc(buffsize);
  if(!answer)
    goto out_of_memory;

  while( (ch = fgetc(fp)) != EOF )
  {
    if(N == 0 && !isalpha(ch))
	{
	  ungetc(ch, fp);
	  free(answer);
	  return 0;
	}
	if(!isalpha(ch) && !isdigit(ch) && ch != '-')
	{
	  ungetc(ch, fp);
	  answer[N] = 0;
	  temp = realloc(answer, N +1);
	  return temp;
	}
	answer[N++] = ch;
	if(N > buffsize -1)
	{
	  temp = realloc(answer, buffsize + 32);
	  if(!temp)
	    goto out_of_memory;
	  answer = temp;
	  buffsize = buffsize + 32;
	}
  }
  
  if(*err == 0)
    *err = -2;
  free(answer);
  return 0;
out_of_memory:
  free(answer);
  *err = -1;
  return 0;
}

/*
  unescape an xml string
  Params: data - the data string
  Returns: escaped string
*/
static char *unescapedata(char *data)
{
   size_t len;
   int i = 0, j = 0;
   char *answer = 0;

   len = strlen(data);
   answer = malloc(len +1);
   if(!answer)
     goto out_of_memory;
   for(i=0;data[i];i++)
   {
     if(data[i] != '&')
	   answer[j++] = data[i];
	 else
	 {
	   if(!strncmp(data +i, "&amp;", 5))
	   {
	     answer[j++] = '&';
		 i += 4;
	   }
	   else if(!strncmp(data +i, "&lt;", 4))
	   {
	     answer[j++] = '<';
		 i += 3;
	   }
	   else if(!strncmp(data+i, "&gt;", 4))
	   {
	     answer[j++] = '>';
		 i += 3;
	   }
	   else if(!strncmp(data+i, "&quot;", 6))
	   {
	     answer[j++] = '\"';
		 i += 5;
	   }
	   else if(!strncmp(data+i, "&apos;", 6))
	   {
	     answer[j++] = '\'';
		 i += 5;
	   }
	   else
	     answer[j++] = i;
	 }
   }
   answer[j] = 0;
   return answer;
out_of_memory:
   return 0;
}

/*
  strdup drop-in replacement
*/
static char *mystrdup(const char *str)
{
  char *answer;

  answer = malloc(strlen(str) + 1);
  if(!answer)
    return 0;
  strcpy(answer, str);
  return answer;
}

int xmlparsermain(void)
{
  FILE *fp;
  int ch;
  XMLDOC *doc;
  XMLNODE *node;
  XMLNODE **desc;
  int N;
  int err;
  int Nchildren;
  int i;

  fp = fopen("C:/Users/Malcolm/Downloads/crosswordtag.xml", "r");
  if(fp)
  {
    while( (ch = fgetc(fp)) != EOF)
	  putchar(ch);
    fclose(fp);
  }
  doc = loadxmldoc("C:/Users/Malcolm/Downloads/crosswordtag.xml", &err);
  if(!doc)
  {
    printf("Can't load file error %d\n", err);
	return 0;
  }
  printf("root tag %s\n", xml_gettag(doc->root) );
  Nchildren = xml_Nchildren(doc->root);
  for(i=0;i<Nchildren;i++)
  {
    node = xml_getchild(doc->root, 0, i);
	printf("child tag %s data %s\n", xml_gettag(node), node->data);
  }
  desc = xml_getdescendants(doc->root, "down", &N);
  printf("N %d\n", N);
  killxmldoc(doc);
  return 0;
}

