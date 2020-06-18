
#ifndef CLIENT_WS_H_
#define CLIENT_WS_H_

#include <client_ws.h>
#include <common.h>
#include <http_parser.h>
#include <ws_parser.h>

#define MAX_HEADER_NAME_LEN     100
#define MAX_HEADER_VALUE_LEN    100

struct client_ws;
typedef struct client_ws client_ws_st;

struct client_ws {
    int on_header_value;
    int got_sec_ws_key_val;
    int http_header_name_len;
    char http_header_name[MAX_HEADER_NAME_LEN];
    int http_sec_ws_key_len;
    char http_sec_ws_key[MAX_HEADER_VALUE_LEN];
    int http_sec_ws_protocol_len;
    char http_sec_ws_protocol[MAX_HEADER_VALUE_LEN];
    http_parser http_parser;
    ws_parser_t ws_parser;
};


int c_ws_init(client_st *cp);

void c_ws_recv(client_st *cp, const unsigned char *data, int len);


#endif  /* CLIENT_WS_H_ */
