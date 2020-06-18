
#ifndef WORKER_H_
#define WORKER_H_

#include <client.h>
#include <mqtt_packet.h>
#include <queue.h>
#include <keep_alive.h>

#include <openssl/ssl.h>

#include <pthread.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <stdint.h>
#include "subscription.h"


struct client;
typedef struct client client_st;

struct keep_alive_s;
typedef struct keep_alive_s keep_alive_st;

struct worker {
    unsigned int    index;

    /** pointer to auth module data */
    void *          auth;

    pthread_mutex_t msg_q_lock;
    char            msg_q_name[32];
    queue_st        msg_q;

    char            gc_q_name[32];
    queue_st        gc_q;

    int             num_clients;
    pthread_mutex_t clients_lock;
    dllist_node_st  *clients_head;

    /** pseudo client for sending $SYS messages from worker */
    client_st       *me;

    pthread_t       thread;

    trie_mult_node_st *subs;

    keep_alive_st   keep_alive;

    SSL_CTX *ctx;

#ifdef HAVE_SELECT
    fd_set          _fd_set;
#endif

#if HAVE_KQUEUE
    enum {
        MAX_EVENTS = 4096
    };
    int kqfd;
    struct kevent karray[MAX_EVENTS];
#endif

};

typedef struct worker worker_st;

struct message;
typedef struct message message_st;


extern worker_st workers[MAX_NUM_WORKERS];


void workers_queue_message(message_st *mp);

void workers_init(unsigned int nw);

unsigned int workers_num_clients(void);

void workers_add_client(server_st *sp, int sd, struct sockaddr_in *peer_addr);

void worker_rm_client(worker_st *p, struct client *rm_cp);


void w_poll_init(worker_st *p);

void w_poll_reg_client(worker_st *p, client_st *cp);

void w_poll_unreg_client(worker_st *p, client_st *cp);

void w_poll(worker_st *p);

#endif /* WORKER_H_ */
