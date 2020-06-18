
/*
 * Manage subscription list per client in a trie with multiple items per node (trie_mult)
 */

#include <common.h>
#include <subscription.h>
//#define DEBUG 1
#include <debug.h>


void sub_insert(trie_mult_node_st **pphead, subscription_st *psub)
{
    unsigned char *filter = psub->filter;

    if (strncmp((char *)filter, "$local/", 7) == 0) {
        filter += 7;
    }

    psub->sub_node.p = psub;
    trie_m_insert(pphead, filter, &psub->sub_node);
}

struct message_and_callback {
    message_st *mp;
    sub_found_callback_t fp;
};

static void callback(void *item, void *pv)
{
    subscription_st *sp = item;
    struct message_and_callback *mcp = pv;
    (*mcp->fp)(sp, mcp->mp);
}

void sub_search(trie_mult_node_st *phead,
        message_st *mp,
        int *pfound_count,
        sub_found_callback_t fp)
{
    struct message_and_callback mc = { mp, fp };
    LOG_DEBUG("sub_search %s", mp->topic);
    trie_m_search_mqtt(phead, mp->topic, pfound_count, callback, &mc);
    LOG_DEBUG("sub_search found %d", *pfound_count);
}

/** delete subscription (filter and client match) */
void sub_remove(trie_mult_node_st **phead, subscription_st *sp)
{
    unsigned char *used_filter = sp->filter;

    /* ignore $local/ prefix to be compatible with emqttd */

    if (strncmp((char *)sp->filter, "$local/", 7) == 0) {
        used_filter += 7;
    }

    LOG_DEBUG("sub_delete %s", used_filter);

    trie_m_delete(phead, used_filter, &sp->sub_node);
}
