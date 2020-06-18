
#ifndef SERVER_H_
#define SERVER_H_

typedef struct {
    int port;
    int is_tls;
    int is_ws;
    int backlog;
    int sd;
} server_st;

void servers_init(void);
void servers_run(void);

#endif /* SERVER_H_ */
