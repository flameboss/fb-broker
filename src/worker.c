
#include <worker.h>
#include <common.h>

#include <openssl/err.h>

#include <pthread.h>
#include <sys/select.h>

//#define DEBUG 1
#include <debug.h>


static unsigned int num_workers;

worker_st workers[MAX_NUM_WORKERS];


static void *start_in_new_thread(void *vp);
static void w_run(worker_st *p);
#ifdef HAVE_SELECT
static void w_recv(worker_st *p);
static int w_poll_select(worker_st *p);
#endif
static void w_message_push(worker_st *p, message_st *item);
static message_st *w_message_pop(worker_st *p);


/* server thread functions */

/** start the worker thread */
static void w_start(worker_st *p)
{
    int rv;

    pthread_mutex_init(&p->clients_lock, NULL);
    pthread_mutex_init(&p->msg_q_lock, NULL);

    sprintf(p->msg_q_name, "worker:%d:msg_q", p->index);
    queue_init(&p->msg_q, p->msg_q_name);

    sprintf(p->gc_q_name, "worker:%d:gc_q", p->index);
    queue_init(&p->gc_q, p->gc_q_name);

    p->me = malloc(sizeof(client_st));
    memset(p->me, 0, sizeof(client_st));
    p->me->client_id_len = sprintf((char *)p->me->client_id, "%s:worker:%d", config.name, p->index);

    rv = pthread_create(&p->thread, NULL, start_in_new_thread, p);
    if (rv) {
        log_error("could not create worker thread %d", p->index);
        exit(1);
    }
}

void workers_init(unsigned int nw)
{
    auth_init();
    num_workers = nw;
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers[i].index = i;
        w_start(&workers[i]);
    }
}

/** return the total number of connected clients */
unsigned int workers_num_clients()
{
    unsigned int cnt = 0;
    for (unsigned int i = 0; i < num_workers; ++i) {
        cnt += workers[i].num_clients;
    }
    return cnt;
}

static worker_st *w_next()
{
    static unsigned int rr_index = 0;
    worker_st *rv;
    rv = &workers[rr_index];
    rr_index = (rr_index + 1) % num_workers;
    return rv;
}

static void w_add_client(worker_st *p, client_st *cp)
{
    LOG_DEBUG("w_add_client:%d %d", p->index, cp->sd);

    pthread_mutex_lock(&p->clients_lock);
    cp->clients_node.p = cp;
    dllist_insert_head(&p->clients_head, &cp->clients_node);
    ++p->num_clients;
    pthread_mutex_unlock(&p->clients_lock);
    w_poll_reg_client(p, cp);
}

static SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

void workers_add_client(server_st *sp,
        int sd,
        struct sockaddr_in *peer_addr)
{
    client_st *cp;
    worker_st *wp;

    LOG_DEBUG("workers_add_client %d", sd);

    wp = w_next();
    cp = c_new(wp, sp, sd, peer_addr);
    if (cp) {
        w_add_client(wp, cp);
    }
}

/** called by c_unbind() */
void worker_rm_client(worker_st *p, client_st *rm_cp)
{
    w_poll_unreg_client(p, rm_cp);

    for (int i = 0; i < rm_cp->num_subs; ++i) {
        sub_remove(&p->subs, &rm_cp->subscriptions[i]);
        free(rm_cp->subscriptions[i].filter);
    }
    rm_cp->num_subs = 0;

    auth_client_destroy(rm_cp);

    pthread_mutex_lock(&p->clients_lock);

    LOG_DEBUG("worker_rm_client %d sd %d cnt %d", p->index, rm_cp->sd, dllist_count(p->clients_head));

    --p->num_clients;

    dllist_remove_ref(&p->clients_head, &rm_cp->clients_node);

    pthread_mutex_unlock(&p->clients_lock);
}

/* called from any worker thread */

/** queue message on all workers */
void workers_queue_message(message_st *mp)
{
    LOG_DEBUG("workers_queue_message");
    ASSERT(mp->dup <= 1);
    unsigned int i;
    for (i = 0; i < num_workers; ++i) {
        w_message_push(&workers[i], mp);
    }
}

/* worker thread 0 only functions (garbage collection of messages */

static int message_done(message_st *mp)
{
    int not_done = 0;
    for (unsigned int i = 0; i < config.num_servers; ++i) {
        not_done |= mp->not_done[i];
    }
    return !not_done;
}

/** garbage collect delivered messages */
void w_gc(worker_st *wp)
{
    queue_item_st *item;
    while ((item = queue_peek(&wp->gc_q)) && message_done(item->p)) {
        message_st *mp = item->p;
        log_debug("w %d gc free mp %p", wp->index, mp);
        queue_pop(&wp->gc_q);
        ASSERT(mp->state == M_STATE_IN_USE);
        mp->state = M_STATE_FREE;
        free(item->p);
    }
}

/* worker thread functions */

/** deliver queued messages to our clients */
void w_deliver(worker_st *p)
{
    message_st *mp;
    while ((mp = w_message_pop(p))) {
        int count;
        sub_search(p->subs, mp, &count, c_deliver);
        mp->not_done[p->index] = 0;
        if (mp->client->worker == p) {
            mp->gc_node.p = mp;
            ASSERT(mp->state == M_STATE_IN_USE);
            queue_push(&p->gc_q, &mp->gc_node);
        }
    }
}

static void *start_in_new_thread(void *vp)
{
    w_run((worker_st *)vp);

    return NULL;
}

static void w_run(worker_st *p)
{
    LOG_DEBUG("w_run:%d", p->index);

    p->ctx = tls_init();

    p->auth = auth_thread_init();

    w_poll_init(p);

    while (1) {
        w_poll(p);
        w_deliver(p);
        w_gc(p);
        ka_run(&p->keep_alive, time(NULL));
    }
}

#if defined(HAVE_SELECT)

static void w_recv(worker_st *p)
{
    client_st *cp;
    for (cp = p->clients; cp != NULL; cp = cp->next) {
        c_recv(cp);
    }
}

static int w_poll(worker_st *p)
{
    client_st *cp;
    int rv;
    int max_sd;
    struct timeval timeout;

    max_sd = 0;
    FD_ZERO(&p->_fd_set);
    for (cp = p->clients; cp != NULL; cp = cp->next) {
        if (cp->sd > max_sd) {
            max_sd = cp->sd;
        }
        FD_SET(cp->sd, &p->_fd_set);
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    rv = select(max_sd + 1, &p->_fd_set, NULL, NULL, &timeout);
    if (rv < 0) {
        if (errno == EINTR) {
            sleep(1);
        }
        return 0;
    }
    if (rv == 0) {
        sleep(1);
        return 0;
    }

    for (cp = p->clients; cp != NULL; cp = cp->next) {
        if (FD_ISSET(cp->sd, &p->_fd_set)) {
            LOG_DEBUG("w_poll_select sd %d ready", cp->sd);
            cp->recv_ready = 1;
        }
    }

    w_recv(p);
    return 1;
}
#endif

static void w_message_push(worker_st *p, message_st *mp)
{
    queue_item_st *item;

    LOG_DEBUG("w_message_push:%d %s %s", p->index, mp->topic, mp->payload);
    mp->not_done[p->index] = 1;
    item = &mp->nodes[p->index];
    item->p = mp;
    pthread_mutex_lock(&p->msg_q_lock);
    queue_push(&p->msg_q, item);
    pthread_mutex_unlock(&p->msg_q_lock);
}

static message_st *w_message_pop(worker_st *p)
{
    queue_st *q;
    queue_item_st *item;
    message_st *rv;

    q = &p->msg_q;
    pthread_mutex_lock(&p->msg_q_lock);
    item = queue_pop(q);
    pthread_mutex_unlock(&p->msg_q_lock);
    if (item == NULL) {
        return NULL;
    }
    rv = item->p;
    ASSERT(rv->state == M_STATE_IN_USE);

    LOG_DEBUG("w_message_pop:%d %s %s", p->index, rv->topic, rv->payload);
    return rv;
}
