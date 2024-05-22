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
#define ASTERISK 3

static XMLNODE *simplexpression(XMLNODE *root, LEXER *lex);
static XMLNODE *pathexpression(XMLNODE *root, LEXER *lex, int level);
static XMLNODE *postfixexpression(XMLNODE *root, LEXER *lex);
static int matchtag(XMLNODE *node, void *ptr);
static int matchall(XMLNODE *node, void *ptr);

static XMLNODE *fish_r(XMLNODE *node, int (*predicate)(XMLNODE *node, void *ptr), void *ptr);
static XMLNODE *select_r(XMLNODE *node, int level, int (*predicate)(XMLNODE *node, void *ptr), void *ptr);

static XMLNODE *clonexmlnode_r(XMLNODE *node);
static XMLNODE *clonexmlnode(XMLNODE *node);
static XMLATTRIBUTE *clonexmlattribute_r(XMLATTRIBUTE *attr);
static XMLATTRIBUTE *clonexmlattribute(XMLATTRIBUTE *attr);

static void initlexer(LEXER *lex, const char *xpath);
static int gettoken(LEXER *lex);
static int getvalue(LEXER *lex, char *value, int Nvalue);
static int match(LEXER *lex, int token);
static int haserror(LEXER *lex);
static void writeerror(LEXER *lex, const char *fmt, ...);
static char *mystrdup(const char *str);

static void printnode_r(XMLNODE *node, int depth);
static void printnodewithsibs(XMLNODE *node);

XMLDOC *xml_selectxpath(XMLDOC *doc, const char *xpath, char *errormessage, int Nerr)
{
    LEXER lex;
    XMLNODE *result = 0;
    XMLNODE *root = 0;
    XMLDOC *answer = 0;
    
    root = clonexmlnode_r(doc->root);
    if (!root)
    {
        goto  out_of_memory;
    }
    printnode_r(root, 0);
    
    initlexer(&lex, xpath);
    result = simplexpression(root, &lex);
    
    if (haserror(&lex))
    {
        killxmlnode(result);
        if (errormessage)
            strncpy(errormessage, lex.error, Nerr);
        return 0;
    }
    if (errormessage)
        errormessage[0] = 0;
    
    answer = malloc(sizeof(XMLDOC));
    if (!answer)
    {
        goto out_of_memory;
    }
    answer->root = result;
    return answer;
    
out_of_memory:
    killxmlnode(result);
    if (errormessage)
        strncpy(errormessage, "Out of memory", Nerr);
    return  0;
}

static XMLNODE *simplexpression(XMLNODE *root, LEXER *lex)
{
    int token = gettoken(lex);
    XMLNODE *answer = root;
    
    if (token == SLASH)
    {
        answer = pathexpression(root, lex, 0);
    }
    else if (token == EQNAME)
    {
        answer = postfixexpression(root, lex);
    }
    else
    {
        writeerror(lex, "Can't recognise path");
    }
    
    match(lex, NUL);
    
    return  answer;
}

static XMLNODE *pathexpression(XMLNODE *root, LEXER *lex, int level)
{
    int token;
    char eqname[1024];
    XMLNODE *answer;
    
    if (match(lex, SLASH) == 0)
        return root;
    
    answer = root;
    
    token = gettoken(lex);
    if (token == EQNAME)
    {
        getvalue(lex, eqname , 1024);
        match(lex, EQNAME);
        answer = select_r(answer, level, matchtag, eqname);
    }
    else if (token == ASTERISK)
    {
        match(lex, ASTERISK);
        answer = select_r(answer, level, matchall, eqname);
    }
    
    token = gettoken(lex);
    if (token == SLASH)
        answer = pathexpression(answer, lex, 1);
        
    return answer;
}


static XMLNODE *postfixexpression(XMLNODE *root, LEXER *lex)
{
    int token;
    char eqname[1024];
    XMLNODE * answer = root;
    
    token = gettoken(lex);
    if (token == EQNAME)
    {
        getvalue(lex, eqname, 1024);
        match(lex, EQNAME);
        answer = fish_r(root, matchtag, eqname);
    }
    token = gettoken(lex);
    if (token == SLASH)
        answer = pathexpression(answer, lex, 0);
    
    return  answer;
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

static XMLNODE *fish_r(XMLNODE *node, int (*predicate)(XMLNODE *node, void *ptr), void *ptr)
{
    XMLNODE *answer = NULL;
    XMLNODE *nextnode;
    XMLNODE *lastnode = NULL;
    XMLNODE *fished;
    
    nextnode = node->next;
    while (node)
    {
        if ((*predicate)(node, ptr))
        {
            if (answer == NULL)
                answer = node;
            lastnode = node;
            
        }
        else
        {
            fished = fish_r(node->child, predicate, ptr);
            if (lastnode)
                lastnode->next = fished;
            else
            {
                answer = fished;
                lastnode = answer;
            }
            while (lastnode->next)
                lastnode = lastnode->next;
            node->next = NULL;
            node->child = NULL;
            killxmlnode(node);
        }
        node = nextnode;
        nextnode = node ? node->next : 0;
    }
    
    return  answer;
}

static XMLNODE *select_r(XMLNODE *node, int level, int (*predicate)(XMLNODE *node, void *ptr), void *ptr)
{
    XMLNODE *answer = NULL;
    XMLNODE *nextnode;
    XMLNODE *lastnode = NULL;
    XMLNODE *fished;
    
    if (level < 0)
    {
        killxmlnode(node);
        return 0;
    }
    
    nextnode = node->next;
    while (node)
    {
        if (level == 0 && (*predicate)(node, ptr))
        {
            if (answer == NULL)
                answer = node;
            lastnode = node;
            
        }
        else
        {
            fished = select_r(node->child, level - 1, predicate, ptr);
            if (lastnode)
                lastnode->next = fished;
            else
            {
                answer = fished;
                lastnode = answer;
            }
            while (lastnode->next)
                lastnode = lastnode->next;
            node->next = NULL;
            node->child = NULL;
            killxmlnode(node);
        }
        node = nextnode;
        nextnode = node ? node->next : 0;
    }
    
    
    return  answer;
}

static XMLNODE *clonexmlnode_r(XMLNODE *node)
{
    XMLNODE *answer;
    XMLNODE *sib;
    XMLNODE *answersib;
    
    answer = clonexmlnode(node);
    if (!answer)
        goto out_of_memory;
    
    if (node->child)
    {
        answer->child = clonexmlnode_r(node->child);
        if (!answer->child)
            goto out_of_memory;
    }
    
    answersib = answer;
    for (sib = node->next; sib != NULL; sib = sib->next)
    {
        answersib->next = clonexmlnode_r(sib);
        if (!answersib->next)
            goto out_of_memory;
        answersib = answersib->next;
    }
    
    return answer;
    
out_of_memory:
    return 0;
    
}


static XMLNODE *clonexmlnode(XMLNODE *node)
{
    XMLNODE *answer;
    
    answer = malloc(sizeof(XMLNODE));
    if (!answer)
        goto out_of_memory;
    
    answer->tag = 0;
    answer->attributes = 0;
    answer->data = 0;
    answer->position = node->position;
    answer->next = 0;
    answer->child = 0;
    
    if (node->tag)
    {
        answer->tag = mystrdup(node->tag);
        if (!answer->tag)
            goto out_of_memory;
    }
    if (node->data)
    {
        answer->data = mystrdup(node->data);
        if (!answer->data)
            goto out_of_memory;
    }
    if (node->attributes)
    {
        answer->attributes = clonexmlattribute_r(node->attributes);
        if (!answer->attributes)
            goto out_of_memory;
    }
    
    return answer;
    
out_of_memory:
    if (answer)
    {
        free(answer->tag);
        free(answer->data);
        free(answer);
    }
    
    return 0;
}

static XMLATTRIBUTE *clonexmlattribute_r(XMLATTRIBUTE *attr)
{
    XMLATTRIBUTE *answer;
    
    answer = clonexmlattribute(attr);
    if (!answer)
        goto out_of_memory;
    if (attr->next)
    {
        answer->next = clonexmlattribute_r(attr->next);
        if (!answer->next)
            goto  out_of_memory;
    }
    
    return answer;
    
out_of_memory:
    return 0;
}

static XMLATTRIBUTE *clonexmlattribute(XMLATTRIBUTE *attr)
{
    XMLATTRIBUTE *answer;
    
    answer = malloc(sizeof(XMLATTRIBUTE));
    if (!answer)
        goto out_of_memory;
    
    answer->name = mystrdup(attr->name);
    if (!answer->name)
        goto  out_of_memory;
    answer->value = mystrdup(attr->value);
    if (!answer->value)
        goto out_of_memory;
    answer->next = 0;
    
    return answer;
out_of_memory:
    if (answer)
    {
        free (answer);
    }
    
    return 0;
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
        else if (isalpha(lex->input[lex->pos]))
        {
            lex->tokenpos = lex->pos;
            while (isalnum(lex->input[lex->pos]) || lex->input[lex->pos] == ':')
            {
                lex->pos++;
            }
            lex->token = EQNAME;
        }
        else if (lex->input[lex->pos] == '/')
        {
            lex->token = SLASH;
            lex->pos++;
        }
        else if (lex->input[lex->pos] == '*')
        {
            lex->token = ASTERISK;
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
        return 0;
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

static char *mystrdup(const char *str)
{
    char *answer;
    
    answer = malloc(strlen(str) +1);
    if (answer)
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

int main(int argc, char **argv)
{
    char error[1024];
    
    if (argc == 3)
    {
        XMLDOC *doc = 0;
        XMLDOC *filtered = 0;
        doc = loadxmldoc2(argv[1], error, 1024);
        if (!doc)
            fprintf(stderr, "%s\n", error);
        filtered = xml_selectxpath(doc, argv[2], error, 1024);
        if (!filtered)
            fprintf(stderr, "%s\n", error);
        
        printdocument(filtered);
        
        killxmldoc(filtered);
        killxmldoc(doc);
    }
    
    
    return 0;
}
