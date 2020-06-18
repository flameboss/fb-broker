
#include <client_ws.h>
#include <common.h>
//#define DEBUG 1
#include <debug.h>


static const char switching_response[] = "HTTP/1.1 101 Switching Protocols\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Protocol: %s\r\n"
        "Sec-WebSocket-Accept: ";

static const char key_suffix[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static __thread char l_buf[MAX_HEADER_VALUE_LEN + sizeof(key_suffix)];


static int http_on_url(http_parser*, const char *at, size_t length);
static int http_on_header_field(http_parser*, const char *at, size_t length);
static int http_on_header_value(http_parser*, const char *at, size_t length);
static int http_on_message_complete(http_parser*);

const http_parser_settings http_settings = {
        .on_url = http_on_url,
        .on_header_field = http_on_header_field,
        .on_header_value = http_on_header_value,
        .on_message_complete = http_on_message_complete
};


int ws_on_data_begin(void *vp, ws_frame_type_t frame);
static int ws_on_data_payload(void* vp, const char *data, size_t len);
int ws_on_data_end(void *vp);
int ws_on_control_begin(void *vp, ws_frame_type_t frame);
int ws_on_control_payload(void *vp, const char *data, size_t len);
int ws_on_control_end(void *vp);

const ws_parser_callbacks_t ws_callbacks = {
        .on_data_begin = ws_on_data_begin,
        .on_data_payload = ws_on_data_payload,
        .on_data_end = ws_on_data_end,
        .on_control_begin = ws_on_control_begin,
        .on_control_payload = ws_on_control_payload,
        .on_control_end = ws_on_control_end
};

/** initialize a websocket client and return 0 on success */
int c_ws_init(client_st *cp)
{
    LOG_DEBUG("c_ws_init sd %d", cp->sd);
    client_ws_st *ws;

    ws = malloc(sizeof(client_ws_st));
    if (ws == NULL) {
        return 1;
    }
    memset(ws, 0, sizeof(client_ws_st));

    http_parser_init(&ws->http_parser, HTTP_REQUEST);
    ws->http_parser.data = cp;
    ws_parser_init(&ws->ws_parser);
    cp->ws = ws;
    return 0;
}

void c_ws_recv(client_st *p, const unsigned char *data, int len)
{
    int nparsed;
    client_ws_st *ws = p->ws;

    nparsed = 0;
    if (! ws->http_parser.upgrade) {
        nparsed = http_parser_execute(&ws->http_parser, &http_settings, (char *)data, len);
        LOG_DEBUG("c_ws_recv nparsed %d", nparsed);
    }
    if (ws->http_parser.upgrade) {
        int ws_len = len - nparsed;
        if (ws_len) {
            ws_parser_execute(&ws->ws_parser, &ws_callbacks, p, (char *)data + nparsed, ws_len);
        }
    }
    else if (nparsed != len) {
        c_disconnect(p, "http_parser_execute");
    }
}


/*
 * HTTP
 */

static int http_on_url(http_parser *p, const char *at, size_t length)
{
    LOG_DEBUG("http_on_url %.*s", length, at);
    /* TODO: check url */
    (void)p;
    (void)at;
    (void)length;
    return 0;
}

static int http_on_header_field(http_parser *parser, const char *at, size_t length)
{
    LOG_DEBUG("http_on_header_field %.*s", length, at);
    client_st *cp;
    cp = parser->data;
    if (cp->ws->on_header_value) {
        cp->ws->http_header_name_len = 0;
        cp->ws->on_header_value = 0;
    }
    length = MIN(length, sizeof(cp->ws->http_header_name) - cp->ws->http_header_name_len);
    memcpy(cp->ws->http_header_name + cp->ws->http_header_name_len, at, length);
    cp->ws->http_header_name_len += length;
    cp->ws->http_header_name[length] = '\0';
    return 0;
}

static int http_on_header_value(http_parser *parser, const char *at, size_t length)
{
    LOG_DEBUG("http_on_header_value %.*s", length, at);
    client_st *cp;
    cp = parser->data;
    cp->ws->on_header_value = 1;
    if (strcmp(cp->ws->http_header_name, "Sec-WebSocket-Key") == 0) {
        length = MIN(length, sizeof(cp->ws->http_sec_ws_key) - cp->ws->http_sec_ws_key_len - 1);
        memcpy(cp->ws->http_sec_ws_key + cp->ws->http_sec_ws_key_len, at, length);
        cp->ws->http_sec_ws_key_len += length;
        cp->ws->http_sec_ws_key[cp->ws->http_sec_ws_key_len] = '\0';
        return 0;
    }
    else if (strcmp(cp->ws->http_header_name, "Sec-WebSocket-Protocol") == 0) {
        length = MIN(length, sizeof(cp->ws->http_sec_ws_protocol) - cp->ws->http_sec_ws_protocol_len - 1);
        memcpy(cp->ws->http_sec_ws_protocol + cp->ws->http_sec_ws_protocol_len, at, length);
        cp->ws->http_sec_ws_protocol_len += length;
        cp->ws->http_sec_ws_protocol[cp->ws->http_sec_ws_protocol_len] = '\0';
        return 0;
    }
    return 0;
}

static int http_on_message_complete(http_parser *parser)
{
    int rv;
    client_st *cp;
    LOG_DEBUG("on_message_complete");
    cp = parser->data;
    if (parser->upgrade) {
        int rv;
        uint8_t *psend;
        uint8_t hash_buf[20];
        const int b64_hash_len = BASE64_OUTPUT_LEN(20);
        const uint32_t response_len = sizeof(switching_response) + b64_hash_len + 3;

        psend = c_send_buf;

        rv = sprintf((char *)psend, switching_response, cp->ws->http_sec_ws_protocol);
        psend += rv;

        /* stage key + suffix in l_buf */
#if 0
        unsigned int key_len;
        key_len = sizeof(l_buf);
        rv = b64_decode(cp->http_header_val, cp->http_header_val_len, (uint8_t *)l_buf, &key_len);
        if (rv != 0) {
            c_disconnect(cp, "b64-decode-key");
            return 0;
        }
        memcpy(l_buf + key_len, key_suffix, sizeof(key_suffix) - 1);
#else
        memcpy(l_buf, cp->ws->http_sec_ws_key, cp->ws->http_sec_ws_key_len);
        memcpy(l_buf + cp->ws->http_sec_ws_key_len, key_suffix, sizeof(key_suffix) - 1);
#endif

        SHA1((unsigned char *)l_buf, cp->ws->http_sec_ws_key_len + sizeof(key_suffix) - 1,
                (unsigned char *)hash_buf);

        b64_encode(hash_buf, 20, (char *)psend);
        psend += b64_hash_len;

        *psend++ = '\r';
        *psend++ = '\n';

        /* end headers */
        *psend++ = '\r';
        *psend++ = '\n';

        c_send_data_wire(cp, c_send_buf, psend - c_send_buf);
    }
    else {
        c_disconnect(cp, "non-upgrade http request");
    }
    return 0;
}

/*
 * WebSocket
 */

int ws_on_data_begin(void *vp, ws_frame_type_t frame)
{
    (void)vp;
    (void)frame;
    return 0;
}

static int ws_on_data_payload(void* vp, const char *data, size_t len)
{
    int rv;
    client_st *p = vp;

    LOG_DEBUG("on_data_payload len %d", len);
    c_parse(p, (const unsigned char *)data, len);
    return 0;
}
int ws_on_data_end(void *vp)
{
    (void)vp;
    return 0;
}
int ws_on_control_begin(void *vp, ws_frame_type_t frame)
{
    (void)vp;
    (void)frame;
    return 0;
}
int ws_on_control_payload(void *vp, const char *data, size_t len)
{
    (void)vp;
    (void)data;
    (void)len;
    return 0;
}
int ws_on_control_end(void *vp)
{
    (void)vp;
    return 0;
}
