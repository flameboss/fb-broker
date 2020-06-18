

#include <keep_alive.h>
#include <common.h>
#include <debug.h>

keep_alive_st ka;

static void server_drop_client(client_st *cp)
{
    static int index = -1;

    log_info("server_drop_client %d", cp->index);
    assert(cp->index > index);
    index = cp->index;
}

void test_keep_alive()
{
    ka_init(&ka);
    ka_set_drop_callback(server_drop_client);
    int n = 20;
    for (int i = 0; i < n; ++i) {
        client_st *cp = malloc(sizeof(client_st));
        memset(cp, 0, sizeof(client_st));
        cp->index = cp->sd = i;
        cp->keep_alive = 10 + i;
        log_info("test_keep_alive %d, cp %p, ka_cnt %d", i, cp, ka.cnt);
        ka_update(&ka, cp, time(NULL));
    }

    time_t t = time(NULL);
    do {
        t = t + 1;
        ka_run(&ka, t);
    }
    while (ka.cnt > 0);

    ka_run(&ka, t + 1);

    ASSERT(ka.cnt + ka.kick_cnt == n);
}

