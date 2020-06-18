
#include <worker.h>
#include <common.h>


#ifdef HAVE_EPOLL

void w_poll_init(worker_st *p)
{
#ifdef HAVE_KQUEUE
    p->kqfd = kqueue();
    if (p->kqfd == -1) {
        perror("w_poll_init kqueue");
        exit(1);
    }
#endif
}

void w_poll_reg_client(worker_st *p, client_st *cp)
{
    int rv;
    struct kevent   event;
    EV_SET(&event, cp->sd, EVFILT_READ, EV_ADD, 0, 0, cp);
    rv = kevent(p->kqfd, &event, 1, NULL, 0, NULL);
    if (rv < 0) {
        perror("w_poll_reg_client kevent");
        exit(1);
    }
}

void w_poll_unreg_client(worker_st *p, client_st *cp)
{
    int rv;
    struct kevent   event;
    EV_SET(&event, cp->sd, EVFILT_READ, EV_DELETE, 0, 0, cp);
    rv = kevent(p->kqfd, &event, 1, NULL, 0, NULL);
    if (rv < 0) {
        perror("w_poll_unreg_client kevent");
        exit(1);
    }
}

void w_poll(worker_st *p)
{
    int rv;
    struct timespec timeout;

    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    rv = kevent(p->kqfd, NULL, 0, p->karray, MAX_EVENTS, &timeout);
    if (rv < 0) {
        perror("w_poll kevent");
        sleep(1);
        return;
    }
    if (rv == 0) {
        sleep(1);
        return;
    }

    struct kevent *ke = p->karray;
    while (rv > 0) {
        switch (ke->filter) {
        case EVFILT_READ:
            {
                client_st *cp = (client_st *)(ke->udata);
                if (ke->flags & EV_EOF) {
                    fprintf(stderr, "w_poll EOF %d", cp->sd);
                    c_disconnect(cp);
                }
                else {
                    fprintf(stderr, "w_poll ready %d", cp->sd);
                    cp->recv_ready = 1;
                    c_recv(cp);
                }
            }
            break;
        default:
            fprintf(stderr, "w_poll ignored kevent %d", ke->filter);
            break;
        }
        --rv;
        ++ke;
    }
}

#endif  /* HAVE_EPOLL */

