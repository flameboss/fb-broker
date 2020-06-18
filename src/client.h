
#ifndef CLIENT_H_
#define CLIENT_H_

#include <client_ws.h>
#include <keep_alive.h>
#include <list.h>
#include <mqtt_packet.h>
#include <mqtt_parser.h>
#include <time.h>
#include <worker.h>

#include <openssl/ssl.h>

#include <stdint.h>
#include "subscription.h"

/** max number of subscriptions */
#define MAX_SUBS    12

typedef enum {
    CLIENT_OPEN,
    CLIENT_CONNECTED,
    CLIENT_CLOSING
} client_state_et;

struct worker;
typedef struct worker worker_st;

struct client;
typedef struct client client_st;

struct client_ws;
typedef struct client_ws client_ws_st;

struct message;
typedef struct message message_st;

struct keep_alive_node;
typedef struct keep_alive_node keep_alive_node_t;

struct client {
    int index;
    client_state_et state;
    int sd;
    uint16_t keep_alive;
    message_st *will_mp;
    unsigned char will_topic[MAX_TOPIC_LEN + 1];
    unsigned char will_payload[MAX_TOPIC_LEN + 1];

    int is_tls;
    SSL *ssl;
    BIO *rbio; /* SSL reads from, we write to. */
    BIO *wbio; /* SSL writes to, we read from. */

    int is_ws;
    client_ws_st *ws;

    mqtt_parser_t parser;

    int is_super;
    uint16_t msg_id;
    char recv_ready;
    char ip_address[20];
    char user_id_str[20];
    unsigned int username_len;
    unsigned char username[MAX_ID_LEN + 1];
    int num_subs;
    subscription_st subscriptions[MAX_SUBS];
    unsigned char client_id[MAX_ID_LEN + 1];

    void *auth_info;

    int client_id_len;
    time_t recv_at;
    worker_st *worker;
    server_st *server;

    /** node for storing this client in the worker->clients_head list */
    dllist_node_st clients_node;

    client_st *next;

    time_t last_ka_update_at;
    time_t expires_at;
    dllist_node_st ka_node;
};


extern __thread unsigned char c_recv_buf[1500];

extern __thread unsigned char c_send_buf[1500];


client_st *c_new(
        worker_st *wp,
        server_st *sp,
        int sd,
        struct sockaddr_in *peer_addr);

void c_recv(client_st *p);

int c_send_data(client_st *p, const unsigned char *buf, unsigned int len);

void c_parse(client_st *p, const unsigned char *buf, unsigned int len);

void c_disconnect_normal(client_st *p, const char *reason);

void c_disconnect(client_st *p, const char *reason);

void c_deliver(subscription_st *p, message_st *mp);

int c_send_data_wire(client_st *p, const unsigned char *buf, unsigned int len);

void c_unbind(client_st *p, const char *reason);

#endif  /* CLIENT_H_ */
