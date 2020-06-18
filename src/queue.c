
#include <queue.h>
#include <common.h>

//#define DEBUG 1
#include <debug.h>


void queue_init(queue_st *q, char *name)
{
    q->name = name;
    q->head = NULL;
    q->tail = NULL;
}

void queue_push(queue_st *q, queue_item_st *item)
{
    LOG_DEBUG("queue_push %s %p", q->name, item->p);

    item->next = NULL;

    if (q->head == NULL) {
        q->head = item;
        q->tail = item;
        return;
    }

    q->tail->next = item;
    q->tail = item;
}

queue_item_st *queue_pop(queue_st *q)
{
    queue_item_st *rv;

    if (q->head == NULL) {
        return NULL;
    }

    rv = q->head;
    if (q->head->next) {
        q->head = q->head->next;
    }
    else {
        q->head = q->tail = NULL;
    }

    LOG_DEBUG("queue_pop %s %p", q->name, rv);
    return rv;
}


queue_item_st *queue_peek(queue_st *q)
{
    return q->head;
}
