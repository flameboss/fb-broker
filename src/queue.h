
#ifndef QUEUE_H_
#define QUEUE_H_

struct queue_item;
typedef struct queue_item queue_item_st;
struct queue_item {
    void *p;
    queue_item_st *next;
};

typedef struct {
    queue_item_st *head;
    queue_item_st *tail;
    char *name;
} queue_st;


void queue_init(queue_st *q, char *name);

void queue_push(queue_st *q, queue_item_st *item);

queue_item_st *queue_pop(queue_st *q);

queue_item_st *queue_peek(queue_st *q);

#endif  /* QUEUE_H_ */
