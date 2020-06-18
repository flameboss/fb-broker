
#ifndef KEEP_ALIVE_H_
#define KEEP_ALIVE_H_

#include <time.h>
#include <list.h>


#define KA_HASH_TABLE_SIZE  101


struct keep_alive_s {
    /** number of clients being monitored */
    int cnt;

    /** number of clients that have been dropped for keep alive timeout */
    int kick_cnt;

    dllist_node_st *hash_table[KA_HASH_TABLE_SIZE];
};
typedef struct keep_alive_s keep_alive_st;

struct client;
typedef struct client client_st;


void ka_init(keep_alive_st *);

void ka_set_drop_callback(void (*fp)(struct client *));

void ka_update(keep_alive_st *, struct client *, time_t);

void ka_remove(keep_alive_st *, client_st *);

void ka_run(keep_alive_st *, time_t t);

#endif  /* KEEP_ALIVE_H_ */
