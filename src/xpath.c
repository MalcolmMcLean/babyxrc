//
//  xpath.c
//  babyxrc
//
//  Created by Malcolm McLean on 22/05/2024.
//

#include "xpath.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct hashcell
{
    void *key;
    struct hashcell *next;
    int deleted;
} HASHCELL;

typedef struct {
    HASHCELL *pool;
    int N;
    HASHCELL **table;
    int Nallocated;
} HASHTABLE;

typedef struct
{
    const char *input;
    int tokenpos;
    int pos;
    int token;
    char error[1204];
} LEXER;

#define NUL 0
#define EQNAME 1
#define SLASH 2
#define SLASHSLASH 3
#define ASTERISK 4
#define STRUDEL 5
#define OPENSQUARE 6
#define CLOSESQUARE 7
#define DOTDOT 8

static HASHTABLE *inithashtablefromtree(XMLNODE *root);
static XMLNODE **getselectednodes(XMLNODE *root, HASHTABLE *ht, int *Nret);

static void stepup_r(XMLNODE *node, HASHTABLE *ht);
static void stepdown_r(XMLNODE *node, HASHTABLE *ht);
static void select_r(XMLNODE *node, HASHTABLE *ht, int (*predicate)(XMLNODE *node, void *ptr), void *ptr);
static void fish_r(XMLNODE *node, HASHTABLE *ht, int (*predicate)(XMLNODE *node, void *ptr), void *ptr);
static void markdeleted_r(XMLNODE *node, HASHTABLE *ht);
static int countselectednodes_r(XMLNODE *node, HASHTABLE *ht);
static XMLNODE **getselectednodes_r(XMLNODE *node, HASHTABLE *ht, XMLNODE **out);
static int getnodepath_r(XMLNODE *node, char *path, int len, XMLNODE *reference);
static int pathdepth_r(XMLNODE *node);
static int countnodes_r(XMLNODE *node);
static void fillhashtable_r(XMLNODE *node, HASHTABLE *ht);

static XMLATTRIBUTE **simplexpression(XMLNODE *root, HASHTABLE *ht, LEXER *lex);
static XMLATTRIBUTE **pathexpression(XMLNODE *root, HASHTABLE *ht, LEXER *lex, int level);
static XMLATTRIBUTE **postfixexpression(XMLNODE *root, HASHTABLE *ht, LEXER *lex);
static void predicate(XMLNODE *root, HASHTABLE *ht, LEXER *lex);

static int matchhaschild(XMLNODE *node, void *ptr);
static int matchhasanychild(XMLNODE *node, void *ptr);
static int matchattribute(XMLNODE *node, void *ptr);
static int matchtag(XMLNODE *node, void *ptr);
static int matchall(XMLNODE *node, void *ptr);

static int pickattribute(XMLATTRIBUTE *attr, void *ptr);

static void initlexer(LEXER *lex, const char *xpath);
static int gettoken(LEXER *lex);
static int getvalue(LEXER *lex, char *value, int Nvalue);
static int match(LEXER *lex, int token);
static int haserror(LEXER *lex);
static void writeerror(LEXER *lex, const char *fmt, ...);
static int iselementchar(int ch);

static HASHTABLE *inithashtable(int N);
static void killhashtable(HASHTABLE *ht);
static void ht_addentry(HASHTABLE *ht, void *address);
static HASHCELL *ht_get(HASHTABLE *ht, void *address);
static unsigned int hash(void *address);

static void printnode_r(XMLNODE *node, int depth);
static void printnodewithsibs(XMLNODE *node);

/*
    Call the XPath query.
 
    Params: doc - the xml document
            xpath - the path to query
            Nselected - return for number of selected paths
            errormessage - return buffer for parse errors
            Nerr - length of errormessage buffer.
    Returns: the selected nodes as a list, yerminateed with a NULL
 
    Notes: were the quety returns no nores, u]it will return an empty
    list containing onl the terminal NULL value. When it encouters an error
    it wil return 0.
 */
XMLNODE **xml_xpath_select(XMLDOC *doc, const char *xpath, int *Nselected, char *errormessage, int Nerr)
{
    LEXER lex;
    HASHTABLE *ht = 0;
    XMLNODE **answer = 0;
    XMLATTRIBUTE** selattributes = 0;
    
    ht = inithashtablefromtree(doc->root);
    if (!ht)
    {
        goto  out_of_memory;
    }
    
    initlexer(&lex, xpath);
    selattributes = simplexpression(doc->root, ht, &lex);
    free(selattributes);
    selattributes = 0;
    
    if (haserror(&lex))
    {
        killhashtable(ht);
        if (errormessage)
            strncpy(errormessage, lex.error, Nerr);
        return 0;
    }
    if (errormessage)
        errormessage[0] = 0;
    
    answer = getselectednodes(doc->root, ht, Nselected);
    if (!answer)
        goto  out_of_memory;
    
    killhashtable(ht);
    return answer;
    
out_of_memory:
    free(selattributes);
    killhashtable(ht);
    if (errormessage)
        strncpy(errormessage, "Out of memory", Nerr);
    return  0;
}

/*
    Call the XPath query on an expression which selects attributes
 
    Params: doc - the xml document
            xpath - the path to query
            Nselected - return for number of selected paths
            errormessage - return buffer for parse errors
            Nerr - length of errormessage buffer.
    Returns: the selected attributes as a list, yerminateed with a NULL
 
    Notes: where the query returns no attributes, it will return an empty
    list containing only the terminal NULL value. When it encouters an error
    it will return 0.
 
     An XPath expression like "//book@category" selects all catehory atr]tributes of nodes with the element tad "book". If you call xml_xpath_select you will obtain thne <book> nodesb themselves.
 
 */
XMLATTRIBUTE **xml_xpath_selectattributes(XMLDOC *doc, const char *xpath, char *errormessage, int Nerr)
{
    LEXER lex;
    HASHTABLE *ht = 0;
    XMLATTRIBUTE **answer = 0;
    
    ht = inithashtablefromtree(doc->root);
    if (!ht)
    {
        goto  out_of_memory;
    }
    
    initlexer(&lex, xpath);
    answer = simplexpression(doc->root, ht, &lex);
    
    if (haserror(&lex))
    {
        killhashtable(ht);
        if (errormessage)
            strncpy(errormessage, lex.error, Nerr);
        return 0;
    }
    if (errormessage)
        errormessage[0] = 0;
    
    killhashtable(ht);
    return answer;
    
out_of_memory:
    killhashtable(ht);
    if (errormessage)
        strncpy(errormessage, "Out of memory", Nerr);
    return  0;
}

/*
    Test whether an XPath selects attributes.
 
    Params: xpath - the xapth exression
           errormessage - reutnr buffer for parse error messages
           Nerr - size of the roor meaage buffer
    Returns: 1 if the xpath expression selects attributes, else 0.
 
    Nots: the Xath xpression "//book" selcts all nodes with the element tag "book". Tne expression "//book@category" selects all nodes with the elemnet tag "book" and an attribute "category" in Node context, and the attributes themselves in an attribute cntext. The funtion distinguishes.
s.
 */
int xml_xpath_selectsattributes(const char *xpath, char *errormessage, int Nerr)
{
    LEXER lex;
    XMLATTRIBUTE **attributes;
    int answer = 0;
    initlexer(&lex, xpath);
    attributes = simplexpression(0, 0, &lex);
    if (attributes)
    {
        answer = 1;
        free(attributes);
    }
    
    if (haserror(&lex))
    {
        if (errormessage)
            strncpy(errormessage, lex.error, Nerr);
        return 0;
    }
    if (errormessage)
        errormessage[0] = 0;
    return answer;
}

/*
    Test an XPath expression for errors.
 
    Params: xpath - the xpath expresion
            errormessage - return buffer for error diagnstics
            Nerr - size of the errormeaasge buffer. (256 bytes adequate).
    Returns: 1 id the xoath ahs valid sntax, 0 if not.
 
    Notes: we do not suppprt all of XPath. So some expressions may be valid
    XPath syntax but not accpeted by this XPath engine.
*/
int xml_xpath_isvalid(const char *xpath, char *errormessage, int Nerr)
{
    XMLDOC *doc = 0;
    XMLNODE **nodes = 0;
    int N;
    int answer = 0;
    
    doc = xmldocfromstring("<root attr=\"true\">Fred</root>\n", 0, 0);
    if (!doc)
        goto out_of_memory;
    nodes = xml_xpath_select(doc, xpath, &N, errormessage, Nerr);
    if (!nodes)
        answer = 0;
    else
        answer = 1;
    free(nodes);
    killxmldoc(doc);
    
    return answer;
    
out_of_memory:
    free(nodes);
    killxmldoc(doc);
    return -1;
}

/*
    Given an node, get the XPath expression which selects it.
    
    Params: doc - the XML document
            node - the node to test (Must be from the document)
    Returns: the ath to the node
 
    Notes: it returns the path as "/bookstore/book/title", so the XPath expression
        will select all siblings at the same level. It is aso quite an
        expensive function as it requires a full traversal of the document tree.
 */
char *xml_xpath_getnodepath(XMLDOC *doc, XMLNODE *node)
{
    int maxlen;
    int len;
    char *answer = 0;
    char *temp = 0;
    
    maxlen = pathdepth_r(doc->root);
    answer = malloc(maxlen + 1);
    if (!answer)
        goto out_of_memory;
    getnodepath_r(doc->root, answer, 0, node);
    len = (int) strlen(answer);
    temp = realloc(answer, len +1);
    if (!temp)
        goto out_of_memory;
    answer = temp;
    
    return answer;
    
out_of_memory:
    free(answer);
    return 0;
}



static HASHTABLE *inithashtablefromtree(XMLNODE *root)
{
    HASHTABLE *ht;
    int N;

    N = countnodes_r(root);
    ht = inithashtable(N);
    if (!ht)
        goto out_of_memory;
    fillhashtable_r(root, ht);
    return ht;
    
out_of_memory:
    return 0;
}

static XMLATTRIBUTE **getselectedattributes(XMLNODE *root, HASHTABLE *ht, int (*predicate)(XMLATTRIBUTE *attr, void *ptr), void *ptr)
{
    XMLATTRIBUTE *attr;
    XMLATTRIBUTE **answer = 0;
    XMLNODE **selnodes = 0;
    int N;
    int i;
    
    selnodes = getselectednodes(root, ht, &N);
    if (!selnodes)
        goto out_of_memory;
    answer = malloc((N+1) * sizeof(XMLATTRIBUTE *));
    if(!answer)
        goto out_of_memory;
    
    for (i = 0; selnodes[i]; i++)
    {
        attr = selnodes[i]->attributes;
        while (attr)
        {
            if ((*predicate)(attr, ptr))
            {
                answer[i] = attr;
                break;
            }
            attr = attr->next;
        }
    }
    answer[N] = 0;
    
    free(selnodes);
    return  answer;
    
out_of_memory:
    free(selnodes);
    free(answer);
    return 0;
}

static XMLNODE **getselectednodes(XMLNODE *root, HASHTABLE *ht, int *Nret)
{
    int N;
    XMLNODE **answer;
    
    N = countselectednodes_r(root, ht);
    answer = malloc((N+1) * sizeof(XMLNODE *));
    if (!answer)
        goto out_of_memory;
    getselectednodes_r(root, ht, answer);
    answer[N] = 0;
    if (Nret)
        *Nret = N;
    return answer;
    
out_of_memory:
    return 0;
}

static void stepup_r(XMLNODE *node, HASHTABLE *ht)
{
    HASHCELL *cell;
    XMLNODE *child;
    HASHCELL *childcell;
    
    while (node)
    {
        cell = ht_get(ht, node);
    
        if (cell->deleted == 1)
        {
            if (node->child)
            {
                child = node->child;
                while (child)
                {
                    childcell = ht_get(ht, child);
                    if (childcell->deleted == 0)
                    {
                        cell->deleted = 0;
                        break;
                    }
                    child = child->next;
                }
            }
                
            if (cell->deleted == 1 && node->child)
                stepup_r(node->child, ht);
        }
        
        node = node->next;
    }
}

static void stepdown_r(XMLNODE *node, HASHTABLE *ht)
{
    HASHCELL *cell;
    
    while (node)
    {
        cell = ht_get(ht, node);
        
        if (cell->deleted == 0)
            cell->deleted = 1;
        else
        {
            if (node->child)
                stepdown_r(node->child, ht);
        }
        
        node = node->next;
    }
}

static void select_r(XMLNODE *node, HASHTABLE *ht, int (*predicate)(XMLNODE *node, void *ptr), void *ptr)
{
    HASHCELL *cell;
    
    while (node)
    {
        cell = ht_get(ht, node);
        if (cell->deleted == 0)
        {
            if (!(*predicate)(node, ptr))
            {
                cell->deleted = 1;
                markdeleted_r(node->child, ht);
            }
        }
        else
        {
            if (node->child)
                select_r(node->child, ht, predicate, ptr);
        }
        node = node->next;
    }
}

static void fish_r(XMLNODE *node, HASHTABLE *ht, int (*predicate)(XMLNODE *node, void *ptr), void *ptr)
{
    HASHCELL *cell;
    
    while (node)
    {
        cell = ht_get(ht, node);
        if (cell->deleted == 0)
        {
            if (!(*predicate)(node, ptr))
            {
                cell->deleted = 1;
            }
        }
        if (cell->deleted)
        {
            if (node->child)
                fish_r(node->child, ht, predicate, ptr);
        }
        node = node->next;
    }
}

static void markdeleted_r(XMLNODE *node, HASHTABLE *ht)
{
    HASHCELL *cell;
    
    while (node)
    {
        cell = ht_get(ht, node);
        cell->deleted = 1;
        if (node->child)
            markdeleted_r(node->child, ht);
        node = node->next;
    }
}

static int countselectednodes_r(XMLNODE *node, HASHTABLE *ht)
{
    int answer = 0;
    HASHCELL *cell;
    
    while (node)
    {
        cell = ht_get(ht, node);
        if (!cell->deleted)
            answer++;
        else if (node->child)
            answer += countselectednodes_r(node->child, ht);
        node = node->next;
    }
    
    return answer;
}

static XMLNODE **getselectednodes_r(XMLNODE *node, HASHTABLE *ht, XMLNODE **out)
{
    HASHCELL *cell;
    
    while (node)
    {
        cell = ht_get(ht, node);
        if (!cell->deleted)
            *out++ = node;
        else if (node->child)
            out = getselectednodes_r(node->child, ht, out);
        node = node->next;
    }
    
    return out;
}

static int getnodepath_r(XMLNODE *node, char *path, int len, XMLNODE *reference)
{
    int taglen;
    
    while (node)
    {
        if (node == reference)
        {
            path[len++] = '/';
            strcpy(path + len, node->tag);
            return 1;
        }
        
        if (node->child)
        {
            path[len] = '/';
            strcpy(path + len + 1, node->tag);
            taglen = (int) strlen(node->tag);
            if (getnodepath_r(node->child, path, len + taglen +1, reference))
               return 1;
            path[len] = 0;
        }
        
        node = node->next;
    }
    
    return 0;
}

static int pathdepth_r(XMLNODE *node)
{
    int answer = 0;
    int path;
    
    while (node)
    {
        if (node->tag)
            path = (int) strlen(node->tag) + 1;
        if (node->child)
            path += pathdepth_r(node->child);
        if (path > answer)
            answer = path;
        node = node->next;
    }
    
    return answer;
}


static int countnodes_r(XMLNODE *node)
{
    int answer = 0;
    
    while (node)
    {
        if (node->child)
            answer += countnodes_r(node->child);
        answer++;
        node = node->next;
    }
    
    return answer;
}

static void fillhashtable_r(XMLNODE *node, HASHTABLE *ht)
{
    while (node)
    {
        ht_addentry(ht, node);
        if (node->child)
            fillhashtable_r(node->child, ht);
        node = node->next;
    }
}


static XMLATTRIBUTE **simplexpression(XMLNODE *root, HASHTABLE *ht, LEXER *lex)
{
    XMLATTRIBUTE **answer = 0;
    int token = gettoken(lex);
    
    while (token != NUL)
    {
        if (token == SLASH)
        {
            if (answer)
                free(answer);
            answer = pathexpression(root, ht, lex, 0);
        }
        else if (token == SLASHSLASH)
        {
            if (answer)
                free(answer);
            answer = postfixexpression(root, ht, lex);
        }
        else if (token == OPENSQUARE)
        {
            predicate(root, ht, lex);
        }
        else
        {
            writeerror(lex, "Can't recognise path");
            break;
        }
        
        token = gettoken(lex);
    }
    
    match(lex, NUL);
    
    return answer;
}

static XMLATTRIBUTE **pathexpression(XMLNODE *root, HASHTABLE *ht, LEXER *lex, int level)
{
    int token;
    char eqname[1024];
    XMLATTRIBUTE **answer = 0;
    
    if (match(lex, SLASH) == 0)
        return;

    if (level)
        stepdown_r(root, ht);
    
    token = gettoken(lex);
    if (token == EQNAME)
    {
        getvalue(lex, eqname , 1024);
        match(lex, EQNAME);
        select_r(root, ht, matchtag, eqname);
    }
    else if (token == ASTERISK)
    {
        match(lex, ASTERISK);
        select_r(root, ht, matchall, eqname);
    }
    else if (token == DOTDOT)
    {
        match(lex, DOTDOT);
        if (level)
            stepup_r(root, ht);
        stepup_r(root, ht);
    }
    token = gettoken(lex);
    if (token == SLASH)
    {
        answer = pathexpression(root, ht, lex, 1);
    }
    else if (token == STRUDEL)
    {
        match(lex, STRUDEL);
        token = gettoken(lex);
        if (token == EQNAME)
        {
            getvalue(lex, eqname, 1024);
            match(lex, EQNAME);
            select_r(root, ht, matchattribute, eqname);
            answer = getselectedattributes(root, ht, pickattribute, eqname);
        }
    }
    
    return answer;
}

static XMLATTRIBUTE **postfixexpression(XMLNODE *root, HASHTABLE *ht, LEXER *lex)
{
    int token;
    char eqname[1024];
    XMLATTRIBUTE **answer = 0;

    
    if (match(lex, SLASHSLASH) == 0)
        return answer;
    
    token = gettoken(lex);
    if (token == EQNAME)
    {
        getvalue(lex, eqname, 1024);
        match(lex, EQNAME);
        fish_r(root, ht, matchtag, eqname);
    }
    else if(token == STRUDEL)
    {
        match(lex, STRUDEL);
        token = gettoken(lex);
        if (token == EQNAME)
        {
            getvalue(lex, eqname, 1024);
            match(lex, EQNAME);
            fish_r(root, ht, matchattribute, eqname);
            answer = getselectedattributes(root, ht, pickattribute, eqname);
        }
        
    }
    
    token = gettoken(lex);
    if (token == SLASH)
    {
        if (answer)
            free(answer);
        answer = pathexpression(root, ht, lex, 1);
    }
    else if (token == STRUDEL)
    {
        match(lex, STRUDEL);
        token = gettoken(lex);
        if (token == EQNAME)
        {
            getvalue(lex, eqname, 1024);
            match(lex, EQNAME);
            select_r(root, ht, matchattribute, eqname);
            if (answer)
                free(answer);
            answer = getselectedattributes(root, ht, pickattribute, eqname);
        }
    }
    
    return answer;
}

static void predicate(XMLNODE *root, HASHTABLE *ht, LEXER *lex)
{
    
    int token;
    char eqname[1024];
    
    match(lex, OPENSQUARE);
    
    token = gettoken(lex);
    
    if (token == EQNAME)
    {
        getvalue(lex, eqname, 1024);
        match(lex, EQNAME);
        select_r(root, ht, matchhaschild, eqname);
    }
    else if (token == ASTERISK)
    {
        match(lex, ASTERISK);
        select_r(root, ht, matchhasanychild, 0);
    }
    
    match(lex, CLOSESQUARE);
}
   


static int matchhaschild(XMLNODE *node, void *ptr)
{
    XMLNODE *child;
    
    for (child = node->child; child != NULL; child = child->next)\
    {
        if (matchtag(child, ptr))
            return  1;
    }
    
    return  0;
}

static int matchhasanychild(XMLNODE *node, void *ptr)
{
    return node->child ? 1 : 0;
}

static int matchattribute(XMLNODE *node, void *ptr)
{
    char *name = ptr;
    XMLATTRIBUTE *attr = node->attributes;
    
    while (attr)
    {
        if (!strcmp(attr->name, name))
            return 1;
        attr = attr->next;
    }
    return 0;
}

static int matchtag(XMLNODE *node, void *ptr)
{
    char *tag = ptr;
    return strcmp(node->tag, tag) ? 0 : 1;
}

static int matchall(XMLNODE *node, void *ptr)
{
    return 1;
}


static int pickattribute(XMLATTRIBUTE *attr, void *ptr)
{
    char *name = ptr;
    
    if (!attr)
        return 0;
    
    if (!strcmp(attr->name, name))
        return 1;
    return  0;
}



static void initlexer(LEXER *lex, const char *xpath)
{
    lex->input = xpath;
    lex->pos = 0;
    lex->token = 0;
    lex->error[0] = 0;
    
    match(lex, 0);
}

static int gettoken(LEXER *lex)
{
    return lex->token;
}

static int getvalue(LEXER *lex, char *value, int Nvalue)
{
    int i;
    
    for (i = 0; i < lex->pos - lex->tokenpos; i++)
    {
        value[i] = lex->input[i +lex->tokenpos];
        if ( i == Nvalue - 1)
        {
            writeerror(lex, "Name too long.");
            break;
        }
    }
    value[i++] = 0;
    
    return 1;
}

static int match(LEXER *lex, int token)
{
    if (lex->token == token)
    {
        if (lex->input[lex->pos] == 0)
            lex->token = NUL;
        else if (isalpha(lex->input[lex->pos]) || lex->input[lex->pos] == '_')
        {
            lex->tokenpos = lex->pos;
            while (iselementchar(lex->input[lex->pos]) || lex->input[lex->pos] == ':')
            {
                lex->pos++;
            }
            lex->token = EQNAME;
        }
        else if (lex->input[lex->pos] == '/')
        {
            lex->token = SLASH;
            lex->pos++;
            if (lex->input[lex->pos] == '/')
            {
                lex->token = SLASHSLASH;
                lex->pos++;
            }
        }
        else if (lex->input[lex->pos] == '.' && lex->input[lex->pos+1] == '.')
        {
            lex->token = DOTDOT;
            lex->pos += 2;
        }
        else if (lex->input[lex->pos] == '*')
        {
            lex->token = ASTERISK;
            lex->pos++;
        }
        else if (lex->input[lex->pos] == '@')
        {
            lex->token = STRUDEL;
            lex->pos++;
        }
        else if (lex->input[lex->pos] == '[')
        {
            lex->token = OPENSQUARE;
            lex->pos++;
        }
        else if (lex->input[lex->pos] == ']')
        {
            lex->token = CLOSESQUARE;
            lex->pos++;
        }
        else
        {
            writeerror(lex, "Unrecognised character[%c]", isgraph(lex->input[lex->pos]) ?
                       lex->input[lex->pos] : '?');
            lex->token = NUL;
        }
        
        return 1;
    }
    else
    {
        if (lex->token == NUL)
        {
            writeerror(lex, "Unexpected end of xpath");
        }
        else if (lex->token == EQNAME)
        {
            char eqname[1024];
            getvalue(lex, eqname, 1024);
            writeerror(lex, "Unexpected name[%s]", eqname);
        }
        else if (lex->token == SLASHSLASH)
        {
            writeerror(lex, "Unexpected symbol[//]");
        }
        else if (lex->token == DOTDOT)
        {
            writeerror(lex, "Unexpected symbol[//]");
        }
        else if (lex->pos > 0)
        {
            int ch = lex->input[lex->pos-1];
            writeerror(lex, "Unexpected symbol[%c]", isgraph(ch) ?
                       ch : '?');
            
        }
        return 0;
    }
}

static int haserror(LEXER *lex)
{
    return lex->error[0] ? 1 : 0;
}

static void writeerror(LEXER *lex, const char *fmt, ...)
{
    va_list valist;
    
    va_start(valist, fmt);
    if (lex->error[0] == 0)
        vsnprintf(lex->error, 1024, fmt, valist);
    va_end(valist);
    
}

static int iselementchar(int ch)
{
    if (isalnum(ch))
        return 1;
    if (ch == '-'|| ch == '_' || ch == '.')
        return 1;
    return 0;
}

static HASHTABLE *inithashtable(int N)
{
    HASHTABLE *ht;
    int i;
    
    ht = malloc(sizeof(HASHTABLE));
    ht->pool = 0;
    ht->table = 0;
    ht->N = 0;
    ht->Nallocated = 0;
    
    ht->pool = malloc(N * sizeof(HASHCELL));
    if (!ht->pool)
        goto out_of_memory;
    
    ht->table = malloc(N * sizeof(HASHCELL *));
    if (!ht->table)
        goto out_of_memory;
    ht->N = N;
    
    for (i = 0 ; i <N; i++)
        ht->table[i] = 0;
    
    return ht;
out_of_memory:
    killhashtable(ht);
    return 0;
}

static void killhashtable(HASHTABLE *ht)
{
    if (ht)
    {
        free(ht->table);
        free(ht->pool);
        free(ht);
    }
}

static void ht_addentry(HASHTABLE *ht, void *address)
{
    HASHCELL *cell;
    unsigned int index;
    
    cell = &ht->pool[ht->Nallocated++];
    cell->key = address;
    index = hash(address) % ht->N;
    cell->next = ht->table[index];
    ht->table[index] = cell;
    
    cell->deleted = 0;
}

static HASHCELL *ht_get(HASHTABLE *ht, void *address)
{
    HASHCELL *cell;
    unsigned int index;
    
    index = hash(address) % ht->N;
    cell = ht->table[index];
    while (cell)
    {
        if (cell->key == address)
            return cell;
        cell = cell->next;
    }
    
    return 0;
}

static unsigned int xhash(void *address)
{
    return (unsigned int) (unsigned long)(address) >> 4;
}

static unsigned int hash(void *address)
{
    int i;
    unsigned long answer = 2166136261;
    unsigned char *byte = (unsigned char *) &address;
    
    for (i =0; i < sizeof(void *); i++)
    {
        answer *= 16777619;
        answer ^= byte[i];
    }
    return (unsigned int) (answer & 0xFFFFFFFF);
}



static void printattributes(XMLATTRIBUTE *attr)
{
    while (attr)
    {
        printf("%s=\"%s\"", attr->name, attr->value);
        if (attr->next)
        {
            printf(" ");
        }
        attr = attr->next;
    }
}

static void printnode_r(XMLNODE *node, int depth)
{
    int i;
    XMLNODE *child;
    
    for (i =0; i < depth; i++)
        printf("\t");
    printf("<%s", node->tag);
    if (node->attributes)
    {
        printf(" ");
        printattributes(node->attributes);
    }
    printf(">\n");
    for (child = node->child; child; child = child->next)
        printnode_r(child, depth +1);
    for (i =0; i < depth; i++)
        printf("\t");
    printf("</%s>\n", node->tag);
}

static void printnodewithsibs(XMLNODE *node)
{
    while (node)
    {
        printnode_r(node, 0);
        node = node->next;
    }
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

int xpathmain(int argc, char **argv)
{
    char error[1024];
    XMLNODE **selected;
    XMLATTRIBUTE **attributes;
    int Nselected;
    int hasattributes;
    int i;
    
    if (argc == 3)
    {
        XMLDOC *doc = 0;
        XMLDOC *filtered = 0;
        doc = loadxmldoc(argv[1], error, 1024);
        if (!doc)
            fprintf(stderr, "%s\n", error);
        
        hasattributes = xml_xpath_selectsattributes(argv[2], error, 1024);
        if (hasattributes)
            printf("xpath selects attributes\n");
        else
            printf("xpath selects nodes\n");
        
        
        selected = xml_xpath_select(doc, argv[2], &Nselected, error, 1024);
        if (!selected)
        {
            fprintf(stderr, "%s\n", error);
        }
        printf("%d nodes selected\n", Nselected);
        for (i = 0; selected[i]; i++)
        {
            printf("node %d\n", i);
            printnode_r(selected[i], 0);
        }
        if (hasattributes)
        {
            attributes = xml_xpath_selectattributes(doc, argv[2], error, 1024);
            for (i = 0; attributes[i]; i++)
            {
                printf("%s=\"%s\"\n", attributes[i]->name, attributes[i]->value);
            }
        }
        
        /*
        filtered = xml_selectxpath(doc, argv[2], error, 1024);
        if (!filtered)
            fprintf(stderr, "%s\n", error);
        else
            printnodewithsibs(filtered->root);
        
        killxmldoc(filtered);
         */
        killxmldoc(doc);
    }
    
    
    return 0;
}
