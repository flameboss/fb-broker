
#include <worker.h>
#include <common.h>
#define DEBUG 1
#include <debug.h>


#ifdef HAVE_EPOLL

void w_poll_init(worker_st *p)
{
    p->epoll_fd = epoll_create1(0);

    LOG_DEBUG("w_poll_init:%d fd %d", p->index, p->epoll_fd);

    if (p->epoll_fd == -1) {
        log_error("w_poll_init:%d epoll_create1 failed errno %d", p->index, errno);
    }
}

void w_poll_reg_client(worker_st *p, client_st *cp)
{
    struct epoll_event event;

    LOG_DEBUG("w_poll_reg_client sd %d", cp->sd);

    event.events = EPOLLIN | EPOLLRDHUP | EPOLLPRI;
    event.data.ptr = cp;

    if (epoll_ctl(p->epoll_fd, EPOLL_CTL_ADD, cp->sd, &event)) {
        log_error("w_poll_reg_client:%d sd %d failed, errno %d", p->index, cp->sd, errno);
    }
}

void w_poll_unreg_client(worker_st *p, client_st *cp)
{
    LOG_DEBUG("w_poll_unreg_client sd %d", cp->sd);

    if (epoll_ctl(p->epoll_fd, EPOLL_CTL_DEL, 0, NULL)) {
        log_error("w_poll_unreg_client:%d sd %d failed, errno %d", p->index, cp->sd, errno);
    }
}

void w_poll(worker_st *p)
{
    int cnt;
    int i;

    cnt = epoll_wait(p->epoll_fd, p->events, W_MAX_EVENTS, 1000);
    for (i = 0; i < cnt; i++) {
        struct epoll_event *event;
        event = &p->events[i];
        client_st *cp = (client_st *)(event->data.ptr);
        LOG_DEBUG("w_poll:%d ready sd %d", p->index, cp->sd);
        cp->recv_ready = 1;
        c_recv(cp);
    }
}

#endif  /* HAVE_EPOLL */

