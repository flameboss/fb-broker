
#include <client.h>

#ifndef SUBSCRIPTION_H_
#define SUBSCRIPTION_H_

#include <trie_mult.h>
#include <stdint.h>

struct message;
typedef struct message message_st;

struct client;
typedef struct client client_st;

struct subscription {
    client_st *client;
    uint8_t qos;
    unsigned char *filter;
    dllist_node_st sub_node;
};
typedef struct subscription subscription_st;

typedef void (*sub_found_callback_t)(subscription_st *, message_st *);


void sub_insert(trie_mult_node_st **pphead, subscription_st *psub);

void sub_search(trie_mult_node_st *phead, message_st *mp, int *pfound_count, sub_found_callback_t fp);

void sub_remove(trie_mult_node_st **pphead, subscription_st *sp);

#endif  /* SUBSCRIPTION_H_ */
