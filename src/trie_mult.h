
#ifndef TRIE_MULT_H_
#define TRIE_MULT_H_

#include <list.h>

#define CHAR_SIZE   256

struct trie_mult_node {
    struct trie_mult_node * children[CHAR_SIZE];
    dllist_node_st *list;
};
typedef struct trie_mult_node trie_mult_node_st;

typedef void (*trie_mult_found_callback_t)(void *, void *);

/** get a new head node */
trie_mult_node_st *new_trie_mult_node(void);

void trie_m_insert(trie_mult_node_st **pphead, const unsigned char *s, dllist_node_st *p);

void trie_m_search_mqtt(trie_mult_node_st *phead,
        const unsigned char *str, int *pfound_count, trie_mult_found_callback_t fp, void *vp);

void trie_m_delete(trie_mult_node_st **phead, const unsigned char *s, dllist_node_st *p);

#endif  /* TRIE_MULT_H_ */
