
#ifndef LIST_H_
#define LIST_H_

struct dllist_node;
typedef struct dllist_node dllist_node_st;

struct dllist_node {
    void *p;
    dllist_node_st *prev;
    dllist_node_st *next;
};

void ddlist_init(dllist_node_st **pphead);

int dllist_count(dllist_node_st *head);

void dllist_insert_head(dllist_node_st **head, dllist_node_st *item);

void dllist_insert_tail_unique(dllist_node_st **head, dllist_node_st *item);

dllist_node_st *dllist_remove_val(dllist_node_st **head, void *vp);

void dllist_remove_ref(dllist_node_st **head, dllist_node_st *p);

#endif  /* LIST_H_ */
