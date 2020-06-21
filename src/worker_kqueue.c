
#include <worker.h>
#include <common.h>
//#define DEBUG 1
#include <debug.h>


#if defined(HAVE_KQUEUE)

void w_poll_init(worker_st *p)
{
#ifdef HAVE_KQUEUE
    p->kqfd = kqueue();
    if (p->kqfd == -1) {
        perror("w_poll_init kqueue");
        exit(1);
    }
    LOG_DEBUG("w_poll_init:%d kqfd %d", p->index, p->kqfd);
#endif
}

void w_poll_reg_client(worker_st *p, client_st *cp)
{
    int rv;
    struct kevent event;
    LOG_DEBUG("w_poll_reg_client:%d sd %d", p->index, cp->sd);
    EV_SET(&event, cp->sd, EVFILT_READ, EV_ADD, 0, 0, cp);
    rv = kevent(p->kqfd, &event, 1, NULL, 0, NULL);
    if (rv < 0) {
        perror("w_poll_reg_client kevent");
    }
}

void w_poll_unreg_client(worker_st *p, client_st *cp)
{
    struct kevent event;
    LOG_DEBUG("w_poll_unreg_client:%d sd %d", p->index, cp->sd);
    EV_SET(&event, cp->sd, EVFILT_READ, EV_DELETE, 0, 0, cp);
    kevent(p->kqfd, &event, 1, NULL, 0, NULL);
}

void w_poll(worker_st *p)
{
    int rv;
    struct timespec timeout;

    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    rv = kevent(p->kqfd, NULL, 0, p->karray, W_MAX_EVENTS, &timeout);
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
                // ignore (ke->flags & EV_EOF), recv will catch it

                LOG_DEBUG("w_poll:%d ready %d, eof %d", p->index, cp->sd, ke->flags & EV_EOF);
                cp->recv_ready = 1;
                c_recv(cp);
            }
            break;
        default:
            LOG_DEBUG("w_poll:%d ignored kevent x%x", p->index, ke->filter);
            break;
        }
        --rv;
        ++ke;
    }
}
#endif
