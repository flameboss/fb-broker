
#include <keep_alive.h>
#include <common.h>
//#define DEBUG 1
#include <debug.h>


#define HASH_TABLE_SIZE     101

#define HASH_VALUE(t)    ((t) % HASH_TABLE_SIZE)

static void (*drop_callback)(client_st *);

static void default_drop_callback(client_st *p)
{
    c_disconnect(p, "keep-alive timeout");
}

void ka_init(keep_alive_st *ka)
{
    ka->cnt = 0;
    ka->kick_cnt = 0;
    memset(ka->hash_table, 0, sizeof(ka->hash_table));
    ka_set_drop_callback(default_drop_callback);
}

void ka_set_drop_callback(void (*fp)(client_st *))
{
    drop_callback = fp;
}

/** refresh timeout for client (new or already monitored) */
void ka_update(keep_alive_st *ka, client_st *pc, time_t now)
{
    int rv;
    int interval;
    int hash_value_1, hash_value_2;

    LOG_DEBUG("ka_update sd %d, ka %d", pc->sd, pc->keep_alive);

    if (pc->keep_alive == 0)
        return;

    /* optimize for bursty pattern */
    if (pc->last_ka_update_at == now) {
        return;
    }

    if (pc->ka_node.p) {
        hash_value_1 = HASH_VALUE(pc->expires_at);
        dllist_remove_ref(&ka->hash_table[hash_value_1], &pc->ka_node);
    }
    else {
        hash_value_1 = -1;
        pc->ka_node.p = pc;
        ++ka->cnt;
    }
    interval = pc->keep_alive + (pc->keep_alive + 1) / 2;
    pc->expires_at = now + interval;

    hash_value_2 = HASH_VALUE(pc->expires_at);
    dllist_insert_head(&ka->hash_table[hash_value_2], &pc->ka_node);

    pc->last_ka_update_at = now;

    LOG_DEBUG("ka_update sd %d, h1 %d %d, h2 %d %d, cnt %d, interval %d",
            pc->sd,
            hash_value_1, hash_value_1 == -1 ? 0 : dllist_count(ka->hash_table[hash_value_1]),
            hash_value_2, dllist_count(ka->hash_table[hash_value_2]),
            ka->cnt, interval);
}

/** stop monitoring the client */
void ka_remove(keep_alive_st *ka, client_st *pc)
{
    int hash_value;

    if (pc->keep_alive && pc->ka_node.p) {
        hash_value = HASH_VALUE(pc->expires_at);
        dllist_remove_ref(&ka->hash_table[hash_value], &pc->ka_node);
        --ka->cnt;
        LOG_DEBUG("ka_remove sd %d, hash %d, cnt %d", pc->sd, hash_value, ka->cnt);
        ASSERT(ka->cnt >= 0);
    }
}

/** expire inactive connections */
void ka_run(keep_alive_st *ka, time_t t)
{
    static time_t last_t = 0;

    int hash_value;
    time_t i;

    ASSERT(t > 0);

    if (last_t == t) return;

    LOG_DEBUG("ka_run t %d, cnt %d", t, ka->cnt);

    i = last_t + 1;
    if (last_t == 0) {
        i = t;
    }
    while (i <= t) {
        dllist_node_st *lp;

        hash_value = HASH_VALUE(i);
        lp = ka->hash_table[hash_value];
        while (lp) {
            client_st *p = lp->p;
            if (p->expires_at <= t) {
                ka_remove(ka, p);
                if (drop_callback) {
                    (*drop_callback)(p);
                }
                ++ka->kick_cnt;
            }
            lp = lp->next;
        }
        ++i;
    }
    last_t = t;
}
