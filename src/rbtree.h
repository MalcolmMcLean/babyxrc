#ifndef rbtree_h
#define rbtree_h

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

RBTREE *rbtree(int (*compfunc)(const void *e1, const void *e2));
void killrbtree(RBTREE *tree);
int rbt_add(RBTREE *tree, void *key, void *data);
int  rbt_del(RBTREE *tree, void *key);
void *rbt_find(RBTREE *tree, void *key);
void *rbt_next(RBTREE *tree, void *key, void **dataret);
void *rbt_prev(RBTREE *tree, void *key, void **dataret);


#endif

