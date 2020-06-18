
/*
 * Trie with multiple items per node in a doubly linked list
 */

#include <trie_mult.h>
#include <common.h>
//#define DEBUG 1
#include <debug.h>

trie_mult_node_st *new_trie_mult_node()
{
    trie_mult_node_st *node;

    node = (trie_mult_node_st *)malloc(sizeof(trie_mult_node_st));
    memset(node, 0, sizeof(*node));
    return node;
}

/** remove the item from the node list and the node if the list is empty
 *  and return 1 if node was free'd */
static int free_trie_node(trie_mult_node_st **curr)
{
    if ((*curr)->list == NULL) {
        free(*curr);
        (*curr) = NULL;
        return 1;
    }
    return 0;
}

static int have_children(trie_mult_node_st * curr)
{
    for (int i = 0; i < CHAR_SIZE; i++)
        if (curr->children[i])
            return 1;   // child found

    return 0;
}

void trie_m_insert(trie_mult_node_st **pphead, const unsigned char *s, dllist_node_st *p)
{
    trie_mult_node_st *curr;

    if (*pphead == NULL) {
        *pphead = new_trie_mult_node();
    }
    curr = *pphead;
    while (*s) {
        if (curr->children[*s] == NULL)
            curr->children[*s] = new_trie_mult_node();

        curr = curr->children[*s];

        ++s;
    }
    dllist_insert_tail_unique(&curr->list, p);
}

static void found_list(dllist_node_st *p, int *found_count, trie_mult_found_callback_t fp, void *vp)
{
    while (p) {
        ++*found_count;
        fp(p->p, vp);
        p = p->next;
    }
}

static void search_r_mqtt(trie_mult_node_st *curr, const unsigned char *s, int *found_count,
        trie_mult_found_callback_t fp, void *vp)
{
    char last_c;

    if (curr == NULL) {
        return;
    }

    LOG_DEBUG("search_r %c", *s);
    last_c  = '\0';
    while (*s) {
        if (last_c == '\0' || last_c == '/') {
            /* handle wildcard subs */
            if (curr->children['#']) {
                found_list(curr->children['#']->list, found_count, fp, vp);
            }
            if (curr->children['+']) {
                const unsigned char *cp;

                cp = s;
                while (*cp && *cp != '/') {
                    ++cp;
                }
                if (*cp == '\0') {
                    found_list(curr->children['+']->list, found_count, fp, vp);
                }
                else {
                    ASSERT(*cp == '/');
                    search_r_mqtt(curr->children['+']->children['/'], cp + 1, found_count, fp, vp);
                }
            }
        }
        curr = curr->children[*s];

        if (curr == NULL)
            return;

        last_c = *s;
        s++;
    }
    if (curr->list) {
        found_list(curr->list, found_count, fp, vp);
    }
}

void trie_m_search_mqtt(trie_mult_node_st *phead, const unsigned char *s, int *pfound_count,
        trie_mult_found_callback_t fp, void *vp)
{
    LOG_DEBUG("trie_m_search_mqtt %s", s);
    *pfound_count = 0;
    if (s[0] == '$') {
        /* ignore wildcard matches when topic starts with $ */
        if (phead != NULL) {
            search_r_mqtt(phead->children['$'], s + 1, pfound_count, fp, vp);
        }
    }
    else {
        search_r_mqtt(phead, s, pfound_count, fp, vp);
    }
    LOG_DEBUG("trie_m_search_mqtt found %d", *pfound_count);
}

static int delete_r(trie_mult_node_st **curr, const unsigned char *s, dllist_node_st *lp)
{
    if (*curr == NULL)
        return 0;

    if (*s) {
        if (*curr != NULL &&
                (*curr)->children[*s] != NULL &&
                delete_r(&((*curr)->children[*s]), s + 1, lp)) {
            if (!have_children(*curr)) {
                return free_trie_node(curr);
            }
            else {
                return 0;
            }
        }
    }
    else {
        dllist_remove_ref(&(*curr)->list, lp);
        LOG_DEBUG("delete_r rv %d", rv);
        if (!have_children(*curr)) {
            return free_trie_node(curr);
        }
        else {
            return 0;
        }
    }

    return 0;
}

void trie_m_delete(trie_mult_node_st **phead, const unsigned char *s, dllist_node_st *lp)
{
    LOG_DEBUG("trie_m_delete %s", s);
    delete_r(phead, s, lp);
}
