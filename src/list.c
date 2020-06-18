
/*
 * Doubly linked list
 */

#include <list.h>
#include <common.h>
//#define DEBUG 1
#include <debug.h>

void ddlist_init(dllist_node_st **pphead)
{
    *pphead = NULL;
}

int dllist_count(dllist_node_st *head)
{
    int rv = 0;
    dllist_node_st *lp = head;

    while (lp) {
        ++rv;
        lp = lp->next;
    }

    return rv;
}

/** insert element at the head of the list */
void dllist_insert_head(dllist_node_st **head, dllist_node_st *item)
{
    item->prev = NULL;
    if (*head) {
        (*head)->prev = item;
    }
    item->next = *head;
    *head = item;
}

/** insert the element at the tail of the list if it is not already in the list */
void dllist_insert_tail_unique(dllist_node_st **head, dllist_node_st *item)
{
    dllist_node_st *p;

    if (*head == NULL) {
        item->prev = NULL;
        item->next = NULL;
        *head = item;
        return;
    }

    p = *head;
    while (1) {
        if (p == item) {
            return;
        }
        if (p->next == NULL) {
            item->next = NULL;
            item->prev = p;
            p->next = item;
            return;
        }
        p = p->next;
    }
}

/** remove item from list */
void dllist_remove_ref(dllist_node_st **head, dllist_node_st *p)
{
    if (*head == NULL) {
        log_error("dllist_remove list empty");
        return;
    }
    if (*head == p) {
        *head = p->next;
        return;
    }
    if (p->prev) {
        p->prev->next = p->next;
    }
    if (p->next) {
        p->next->prev = p->prev;
    }
}

/** delete element with node.p == vp and return the node * if found and deleted, otherwise NULL */
dllist_node_st *dllist_remove_val(dllist_node_st **head, void *vp)
{
    dllist_node_st *lp = *head;
    int rv;
    rv = 1;
    while (lp) {
        if (lp->p == vp) {
            if (lp == *head) {
                *head = lp->next;
                return lp;
            }
            else {
                lp->prev->next = lp->next;
                return lp;
            }
        }
        lp = lp->next;
    }
    return NULL;
}
