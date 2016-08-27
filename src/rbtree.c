#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct rbnode
{
  struct rbnode *left;
  struct rbnode *right;
  struct rbnode *parent;
  char colour;
  void *key;
  void *data;
} RBNODE;

typedef struct
{
  RBNODE sentinel;
  RBNODE *root;
  RBNODE *last;
  int (*comp)(const void *e1, const void *e2);
} RBTREE;

static RBNODE *rbtFind(RBTREE *tree, void * key);
static void rbtDelete(RBTREE *tree, RBNODE *p);
static void insertFixup(RBTREE *tree, RBNODE *x);
static void deleteFixup(RBTREE *tree, RBNODE *x);
static void rotateLeft(RBTREE *tree, RBNODE *x);
static void rotateRight(RBTREE *tree, RBNODE  *x);
static RBNODE *nextnode(RBTREE *tree, RBNODE *x);
static RBNODE *prevnode(RBTREE *tree, RBNODE *x);

/*
  red balck tree constructor
  Params: compfunc - qsort() style comparison function
  Returns: pointer to created red black tree.

 */
RBTREE *rbtree(int (*compfunc)(const void *e1, const void *e2))
{
  RBTREE *answer;

  answer = malloc(sizeof(RBTREE));
  if(!answer)
    return 0;

  answer->comp = compfunc;
  answer->root = &answer->sentinel;
  answer->sentinel.colour = 'B';
  answer->sentinel.left = &answer->sentinel;
  answer->sentinel.right = &answer->sentinel;
  answer->sentinel.parent = 0;
  answer->sentinel.key = 0;
  answer->sentinel.data = 0;
  answer->last = 0;

  return answer; 
}

/*
  destroy a red balck tree
  Params: tree - object to destroy.
 */
void killrbtree(RBTREE *tree)
{
  rbtDelete(tree, tree->root);
  free(tree);
}

/*
 insert new node (no duplicates allowed)
*/
int rbt_add(RBTREE *tree, void *key, void *data) 
{
  RBNODE *current, *parent, *x;

  // allocate node for data and insert in tree

  // find future parent
  current = tree->root;
  parent = 0;
  while (current != &tree->sentinel) {
    if ( (*tree->comp)(key, current->key) == 0) 
      return -1;
    parent = current;
    current = (*tree->comp) (key, current->key) < 0 ?
      current->left : current->right;
  }

  // setup new node
  if ((x = malloc (sizeof(*x))) == 0)
    return -1;
  x->parent = parent;
  x->left = &tree->sentinel;
  x->right = &tree->sentinel;
  x->colour = 'R';
  x->key = key;
  x->data = data;

  // insert node in tree
  if(parent) 
  {
    if((*tree->comp)(key, parent->key) < 0)
      parent->left = x;
    else
      parent->right = x;
  } 
  else 
  {
    tree-> root = x;
  }

  insertFixup(tree, x);

  return 0;
}

/*
  delete a node
  Params: tree - the tree
          key - key of node to delete
  Returns 0 on success, -1 on fail (not found)
 */
int  rbt_del(RBTREE *tree, void *key) 
{
  RBNODE *z;
  RBNODE *x, *y;

  z = rbtFind(tree, key);
  if(!z)
    return -1;
  if(tree->last == z)
    tree->last = 0;

  if (z->left == &tree->sentinel || z->right == &tree->sentinel) 
  {
    // y has a SENTINEL node as a child
    y = z;
  } 
  else 
  {
    // find tree successor with a SENTINEL node as a child
    y = z->right;
    while (y->left != &tree->sentinel) y = y->left;
  }

  // x is y's only child
  if (y->left != &tree->sentinel)
    x = y->left;
    else
      x = y->right;

  // remove y from the parent chain
  x->parent = y->parent;
  if (y->parent)
    if (y == y->parent->left)
      y->parent->left = x;
    else
      y->parent->right = x;
  else
    tree->root = x;

  if (y != z) 
  {
    z->key = y->key;
    z->data = y->data;
  }

  if(y->colour == 'B')
    deleteFixup (tree, x);

  free (y);

  return 0;
}

/*
  retrive an item with a key
  Params: tree - the rb tree
          key - key of item to find
  Returns: pointer to item if found, else NULL.
 */
void *rbt_find(RBTREE *tree, void *key)
{
  RBNODE *node;

  node = rbtFind(tree, key);
  if(node)
    return node->data;
  else
    return 0;
}

/*
  walk the tree in forward direction.
  Params: tree - pointer to the tree
          key - key of previous item (NULL for first)
          dataret - return pointer for data item.
  Returns: key of next item in tree.
 */
void *rbt_next(RBTREE *tree, void *key, void **dataret)
{
  RBNODE *node = 0;

  if(key == 0)
  {
    node = tree->root;
    while(node->left != &tree->sentinel)
      node = node->left;
  }
  else 
  {
    if(tree->last)
    {
      if(tree->last->key == key || (*tree->comp)(tree->last->key, key) == 0)
        node = tree->last;
    }
    if(node == 0)
      node = rbtFind(tree, key);
    if(node == 0)
    {
      return 0;
    }
    node = nextnode(tree, node);
  }
  if(node != &tree->sentinel && node)
  {
    tree->last = node;
    if(dataret)
      *dataret = node->data;
    return node->key;
  }
  if(dataret)
    *dataret = 0;
  return 0;
}

/*
  walk the tree in reverse direction.
  Params: tree - pointer to the tree
          key - key of follwing item
          dataret - return pointer for data item.
  Returns: key of previous item in tree.
 */
void *rbt_prev(RBTREE *tree, void *key, void **dataret)
{
  RBNODE *node = 0;

  if(key == 0)
  {
    node = tree->root;
    while(node->right != &tree->sentinel)
      node = node->right;
  }
  else
  { 
    if(tree->last)
    {
      if(tree->last->key == key || (*tree->comp)(tree->last->key, key) == 0)
        node = tree->last;
    }
    if(node == 0)
      node = rbtFind(tree, key);
    if(node == 0)
    {
      return 0;
    }
    node = prevnode(tree, node);
  }
  if(node != &tree->sentinel && node)
  {
    tree->last = node;
    if(dataret)
      *dataret = node->data;
    return node->key;
  }
  if(dataret)
    *dataret = 0;
  return 0;
}


/*
  find a node matching a key
  Params: tree - the tree
          key - key
  Returns: pointer to the node, NULL on fail.
 */
static RBNODE *rbtFind(RBTREE *tree, void * key) 
{
  RBNODE *current;
  current = tree->root;
  while(current != &tree->sentinel) 
  {
    if((*tree->comp)(key, current->key) == 0)
      return current;
    else 
    {
      current = (*tree->comp) (key, current->key) < 0 ?
	current->left : current->right;
    }
  }
  return NULL;
}

/*
  recursive node destructor
  Params: tree - pointer tot he tree
          p - node to destroy
 */
static void rbtDelete(RBTREE *tree, RBNODE *p) 
{
  if(p == &tree->sentinel) 
     return;
  rbtDelete(tree, p->left);
  rbtDelete(tree, p->right);
  free(p);
}

/*
  rebalance the tree after an insertion
  Params: tree - pointer to the tree
          x - pointer to the node just inserted.
 */
static void insertFixup(RBTREE *tree, RBNODE *x) 
{

  // maintain red-black tree balance
  // after inserting node x

  // check red-black properties
  while (x != tree->root && x->parent->colour == 'R') 
  {
    // we have a violation
    if (x->parent == x->parent->parent->left) 
    {
      RBNODE *y = x->parent->parent->right;
      if (y->colour == 'R') 
      {
	// uncle is RED
	x->parent->colour = 'B';
	y->colour = 'B';
	x->parent->parent->colour = 'R';
	x = x->parent->parent;
      } 
      else 
      {
	// uncle is BLACK
	if (x == x->parent->right) 
        {
	  // make x a left child
	  x = x->parent;
	  rotateLeft(tree, x);
	}

	// recolor and rotate
	x->parent->colour = 'B';
	x->parent->parent->colour = 'R';
	rotateRight(tree, x->parent->parent);
      }
    } 
    else 
    {
      // mirror image of above code
      RBNODE *y = x->parent->parent->left;
      if (y->colour == 'R') {

	// uncle is RED
	x->parent->colour = 'B';
	y->colour = 'B';
	x->parent->parent->colour = 'R';
	x = x->parent->parent;
      } 
      else 
      {
	// uncle is BLACK
	if (x == x->parent->left) 
        {
	  x = x->parent;
	  rotateRight(tree, x);
	}
	x->parent->colour = 'B';
	x->parent->parent->colour = 'R';
	rotateLeft(tree, x->parent->parent);
      }
    }
  }
  tree->root->colour = 'B';
}

/*
  rebalance the tree after a deletion.
  Params: tree - the tree
          x - node just deleted
 */
static void deleteFixup(RBTREE *tree, RBNODE *x) 
{

  // maintain red-black tree balance
  // after deleting node x

  while (x != tree->root && x->colour == 'B') 
  {
    if (x == x->parent->left) 
    {
      RBNODE *w = x->parent->right;
      if (w->colour == 'R') 
      {
	w->colour = 'B';
	x->parent->colour = 'R';
	rotateLeft (tree, x->parent);
	w = x->parent->right;
      }
      if (w->left->colour == 'B' && w->right->colour == 'B') 
      {
	w->colour = 'R';
	x = x->parent;
      } 
      else 
      {
	if (w->right->colour == 'B') 
        {
	  w->left->colour = 'B';
	  w->colour = 'R';
	  rotateRight (tree, w);
	  w = x->parent->right;
	}
	w->colour = x->parent->colour;
	x->parent->colour = 'B';
	w->right->colour = 'B';
	rotateLeft (tree, x->parent);
	x = tree->root;
      }
    } 
    else 
    {
      RBNODE *w = x->parent->left;
      if (w->colour == 'R') 
      {
	w->colour = 'B';
	x->parent->colour = 'R';
	rotateRight (tree, x->parent);
	w = x->parent->left;
      }
      if (w->right->colour == 'B' && w->left->colour == 'B') 
      {
	w->colour = 'R';
	x = x->parent;
      } 
      else 
      {
	if (w->left->colour == 'B') 
        {
	  w->right->colour = 'B';
	  w->colour = 'R';
	  rotateLeft (tree, w);
	  w = x->parent->left;
	}
	w->colour = x->parent->colour;
	x->parent->colour = 'B';
	w->left->colour = 'B';
	rotateRight (tree, x->parent);
	x = tree->root;
      }
    }
  }
  x->colour = 'B';
}

static void rotateLeft(RBTREE *tree, RBNODE *x) 
{

  // rotate node x to left

  RBNODE  *y = x->right;

  // establish x->right link
  x->right = y->left;
  if (y->left != &tree->sentinel ) 
    y->left->parent = x;

  // establish y->parent link
  if (y != &tree->sentinel) 
     y->parent = x->parent;
  if (x->parent) 
  {
    if (x == x->parent->left)
      x->parent->left = y;
    else
      x->parent->right = y;
  } 
  else 
   tree->root = y;

  // link x and y
  y->left = x;
  if (x != &tree->sentinel)
    x->parent = y;
}

static void rotateRight(RBTREE *tree, RBNODE  *x) {

    // rotate node x to right

    RBNODE *y = x->left;

    // establish x->left link
    x->left = y->right;
    if (y->right != &tree->sentinel ) 
      y->right->parent = x;

    // establish y->parent link
    if (y != &tree->sentinel ) 
       y->parent = x->parent;
    if (x->parent) 
    {
       if (x == x->parent->right)
          x->parent->right = y;
        else
          x->parent->left = y;
    } 
    else
      tree->root = y;

    // link x and y
    y->right = x;
    if(x != &tree->sentinel ) 
      x->parent = y;
}

/*
  get the next node in the tree
  Params: tree - the tree.
          x - previous node.
  Returns: node following x, or NULL if x is last node.
 */
static RBNODE *nextnode(RBTREE *tree, RBNODE *x)
{
  if(x->right != &tree->sentinel)
    return x->right;
  if(x->parent == 0)
    return 0;
  if(x->parent->left == x)
    return x->parent;

  while(x->parent && x->parent->right == x)
    x = x->parent;
  if(x->parent)
    return  x->parent;
  else
    return 0;
}

/*
  get the previous node in the tree.
  Params: tree - the tree
          x - following node.
  Returns: node immediately before x, or NULL if x is first.
 */
static RBNODE *prevnode(RBTREE *tree, RBNODE *x)
{
  if(x->left != &tree->sentinel)
    return x->left;
  if(x->parent == 0)
    return 0;
  if(x->parent->right == x)
    return x->parent;
  
  while(x->parent && x->parent->left == x)
    x = x->parent;
  if(x->parent)
    return x->parent;
  else
    return 0;
}


typedef struct
{
  char data[32];
} DATAITEM;

int compare(const void *e1, const void *e2)
{
  const DATAITEM *ptr1 = e1;
  const DATAITEM *ptr2 = e2;

  return strcmp(ptr1->data, ptr2->data);
}

int rbtreemain(void)
{
  DATAITEM data[100];
  int i;
  RBTREE *tree;
  DATAITEM *ptr;

  for(i=0;i<100;i++)
    sprintf(data[i].data, "item%d", i);

  tree = rbtree(compare);

  for(i=0;i<100;i++)
    rbt_add(tree, &data[i], &data[i]);

  rbt_del(tree, &data[50]);
  rbt_del(tree, &data[60]);

  for(i=0;i<100;i+=10)
    {
      ptr = rbt_find(tree, &data[i]);
      if(ptr)
        printf("%s\n", ptr->data);
    }
  
  ptr = 0;
  while(ptr = rbt_prev(tree, ptr, 0))
    printf("*%s*\n", ptr->data);
  killrbtree(tree);

  printf("OK\n");

  return 0;
}
