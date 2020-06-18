
#include <server.h>
#include <common.h>
#include <debug.h>


static int open_server_socket(server_st *);
static int poll_server_socket(server_st *);


static void sig_usr1_handler(int sig)
{
    (void)sig;
    printf("%d clients\n", workers_num_clients());
}

void servers_init()
{
    unsigned int i;
    int sd;

    for (i = 0; i < config.num_servers; ++i) {
        server_st *sp = &config.servers[i];
        sp->sd = open_server_socket(sp);
        log_info("listening on port %d, tls %d, ws %d, backlog %d",
                sp->port, sp->is_tls, sp->is_ws, sp->backlog);
    }
    workers_init(config.num_workers);

    signal(SIGUSR1, sig_usr1_handler);
}

void servers_run()
{
    while (1) {
        int busy = 0;
        for (unsigned int i = 0; i < config.num_servers; ++i) {
            busy |= poll_server_socket(&config.servers[i]);
        }
        if (!busy) {
            usleep(100000);
        }
    }
}


static int open_server_socket(server_st *sp)
{
    int rv;
    int sd;
    struct kevent event;

    sd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sd >= 0);

    int one = 1;
    setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(sp->port);
    rv = bind(sd, (struct sockaddr *)&addr, sizeof(addr));
    if (rv != 0) {
        log_error("server: error: could not bind to port %d", sp->port);
        exit(1);
    }

    rv = listen(sd, sp->backlog);
    assert(rv == 0);

    rv = fcntl(sd, F_GETFL, 0);
    rv = fcntl(sd, F_SETFL, rv | O_NONBLOCK);
    assert(rv == 0);

    return sd;
}

static int poll_server_socket(server_st *sp)
{
    worker_st *pw;
    client_st *cp;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    int csd = accept(sp->sd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    if (csd == -1) {
        if (errno == EAGAIN) {
            return 0;
        }
        perror("accept");
        exit(1);
    }
    workers_add_client(sp, csd, &peer_addr);
    return 1;
}
