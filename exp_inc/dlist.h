#ifndef _DLIST_H_
#define _DLIST_H_

typedef struct _DListNode
{
    void *data;
    struct _DListNode *prev;
    struct _DListNode *next;
}DListNode;

typedef struct _DList
{
    DListNode *head;
    DListNode *tail;
}DList;

typedef enum _Ret
{
    RET_OK = 1,
    RET_FAULT,
    RET_OOM,
}Ret;

#define return_val_if_fail(p, val)     if(!(p)) { return val;}
#define dlist_get_data_safe(a,b) (dlist_get(a,b)==NULL?NULL:dlist_get(a,b)->data)
DList *dlist_create();
int dlist_len(DList *dlist);
DListNode *dlist_get(DList *dlist, int index);
Ret dlist_add(DList *dlist, int index,  void *data);
Ret dlist_attach(DList *dlist,void *data);
Ret dlist_delete(DList *dlist, int index);
Ret dlist_destroy(DList *dlist);
Ret dlist_remove(DList *dlist, int index);

#endif /*_APP_LIST_H_*/
