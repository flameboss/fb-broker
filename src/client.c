
/*
 * Manage a client session
 */

#include <client.h>
#include <client_tls.h>
#include <common.h>
#include <mqtt_encode.h>
#include <mqtt_packet.h>
#include <mqtt_parser.h>

//#define DEBUG 1
#include <debug.h>


static const char * const sys_connected = "$SYS/brokers/%s/clients/%s/connected";
static unsigned int sys_connected_len;

static const char * const sys_disconnected = "$SYS/brokers/%s/clients/%s/disconnected";
static unsigned int sys_disconnected_len;

static __thread char l_reason[128];

/** buffer for receiving MQTT protocol data */
__thread unsigned char c_recv_buf[1500];


static void c_pub_connected(client_st *p);
static void c_pub_disconnected(client_st *p, const char *reason);

static int on_recv_connect(mqtt_parser_t *, connect_st *);
static int on_recv_subscribe(mqtt_parser_t *, subscribe_st *);
static int on_recv_unsubscribe(mqtt_parser_t *, subscribe_st *);
static int on_recv_publish(mqtt_parser_t *, message_st *);
static int on_recv_ping(mqtt_parser_t *);

__thread unsigned char c_send_buf[1500];

mqtt_parser_callbacks_t mqtt_parser_callbacks = {
    .on_recv_connect = on_recv_connect,
    .on_recv_subscribe = on_recv_subscribe,
    .on_recv_unsubscribe = on_recv_unsubscribe,
    .on_recv_publish = on_recv_publish,
    .on_recv_ping = on_recv_ping
};

static char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            break;

        default:
            strncpy(s, "Unknown AF", maxlen);
            return NULL;
    }

    return s;
}

/** create a new client */
client_st *c_new(
        worker_st *wp,
        server_st *sp,
        int sd,
        struct sockaddr_in *peer_addr)
{
    client_st *cp;
    cp = malloc(sizeof(client_st));
    if (!cp) {
        log_error("malloc client failed");
        return NULL;
    }
    memset(cp, 0, sizeof(client_st));
    cp->worker = wp;
    cp->server = sp;
    cp->sd = sd;
    cp->is_tls = sp->is_tls;
    cp->is_ws = sp->is_ws;
    get_ip_str((const struct sockaddr *)peer_addr, cp->ip_address, sizeof(cp->ip_address));

    if (cp->is_tls) {
        if (tls_open(wp->ctx, cp)) {
            log_warn("sd %d TLS open failed", sd);
            free(cp);
            return NULL;
        }
    }
    if (cp->is_ws) {
        c_ws_init(cp);
    }

    cp->parser.client = cp;
    cp->parser.callbacks = &mqtt_parser_callbacks;
    mqtt_parser_init(&cp->parser);

    LOG_DEBUG("tcp connect sd %d, ip %s, port %d", cp->sd, cp->ip_address, cp->server->port);
    return cp;
}

static int on_recv_connect(mqtt_parser_t *parser, connect_st *connect_info)
{
    int len;
    client_st *p = parser->client;
    unsigned char ack = 0;

    if (config.allow_anonymous && p->username[0] == '\0') {
        p->is_super = 0;
    }
    else if (auth_auth(p->worker->auth, p, connect_info->password)) {
        ack = 5;
        p->is_super = 0;
    }
    else {
        p->is_super = (auth_super(p->worker->auth, p) == 0);
    }

    len = encode_conack(c_send_buf, sizeof(c_send_buf), 0, ack);
    if (c_send_data(p, c_send_buf, len)) {
        return 1;
    }

    if (ack == 0) {
        p->state = CLIENT_CONNECTED;
        log_info("connect sd %d, client-id %s, keep-alive %d, port %d, ip %s",
                p->sd, p->client_id, p->keep_alive, p->server->port, p->ip_address);
        c_pub_connected(p);
        ka_update(&p->worker->keep_alive, p, p->recv_at);
    }
    else {
        c_disconnect(p, "auth failed");
    }
    return 0;
}

static int on_recv_subscribe(mqtt_parser_t *parser, subscribe_st *sub)
{
    int len;
    client_st *p = parser->client;
    int granted_qos[MAX_SUBS];

    ASSERT(sub->num_filters <= MAX_SUBS);

    for (int i = 0; i < sub->num_filters; ++i) {
        if (!config.allow_anonymous &&
                !p->is_super &&
                auth_acl(p->worker->auth, p, AUTH_SUB, sub->filters[i], sub->filter_lengths[i])) {
            log_warn("sd %d sub ACL FAILED for %s", p->sd, sub->filters[i]);
            c_disconnect(p, "auth sub failed");
            return 1;
        }
    }

    for (int i = 0; i < sub->num_filters; ++i) {
        int j;
        for (j = 0; j < p->num_subs; ++j) {
            if (strcmp((char *)p->subscriptions[j].filter, (char *)sub->filters[i]) == 0) {
                granted_qos[i] = MIN(sub->qos[i], 1);
                p->subscriptions[i].qos = granted_qos[i];
                break;
            }
        }
        if (j < p->num_subs) {
            continue;
        }
        p->subscriptions[p->num_subs].filter = sub->filters[i];
        p->subscriptions[p->num_subs].client = p;

        /* TODO: support qos 2 */
        granted_qos[i] = MIN(sub->qos[i], 1);
        p->subscriptions[p->num_subs].qos = granted_qos[i];
        sub_insert(&p->worker->subs, &p->subscriptions[p->num_subs]);
        ++p->num_subs;
        log_debug("subscribe sd %d %s", p->sd, p->subscriptions[i].filter);
    }

    len = encode_suback(c_send_buf, sizeof(c_send_buf), sub->packet_id, sub->num_filters, granted_qos);
    if (len == 0) { return 0; }
    if (c_send_data(p, c_send_buf, len)) {
        return 1;
    }
    ka_update(&p->worker->keep_alive, p, p->recv_at);
    return 0;
}

static int on_recv_unsubscribe(mqtt_parser_t *parser, subscribe_st *sub)
{
    int len;
    client_st *p = parser->client;
    int granted_qos[MAX_SUBS];

    ASSERT(sub->num_filters <= MAX_SUBS);

    for (int i = 0; i < sub->num_filters; ++i) {
        log_debug("unsubscribe sd %d %s", p->sd, sub->filters[i]);
        for (int j = 0; j < p->num_subs; ++j) {
            if (strcmp((char *)p->subscriptions[j].filter, (char *)sub->filters[i]) == 0) {
                sub_remove(&p->worker->subs, &p->subscriptions[j]);
                for (int k = j; k < p->num_subs; ++k) {
                    /* shallow structure copy */
                    p->subscriptions[k] = p->subscriptions[k+1];
                }
                --p->num_subs;
            }
        }
    }

    len = encode_unsuback(c_send_buf, sizeof(c_send_buf), sub->packet_id);
    if (len == 0) { return 0; }
    if (c_send_data(p, c_send_buf, len)) {
        return 1;
    }
    ka_update(&p->worker->keep_alive, p, p->recv_at);
    return 0;
}

static int on_recv_publish(mqtt_parser_t *parser, message_st *mp)
{
    int len;
    client_st *p = parser->client;
    int rv;

    log_debug("publish sd %d %d %s %.*s", p->sd, mp->packet_id, mp->topic, 32, mp->payload);

    if (!config.allow_anonymous &&
            !p->is_super &&
            auth_acl(p->worker->auth, p, AUTH_PUB, mp->topic, mp->topic_len)) {
        log_warn("sd %d publish ACL failed %s", p->sd, mp->topic);
        return 0;
    }

    mp->client = p;

    workers_queue_message(mp);

    if (mp->orig_qos == 1) {
        len = encode_ack(c_send_buf, sizeof(c_send_buf), PUBACK, 0, mp->packet_id);
        if (c_send_data(p, c_send_buf, len)) {
            return 1;
        }
    }
    else if (mp->orig_qos == 2) {
        len = encode_ack(c_send_buf, sizeof(c_send_buf), PUBREC, 0, mp->packet_id);
        if (c_send_data(p, c_send_buf, len)) {
            return 1;
        }
    }

    ka_update(&p->worker->keep_alive, p, p->recv_at);
    return 0;
}

static int on_recv_ping(mqtt_parser_t *parser)
{
    int len;
    client_st *p = parser->client;

    log_debug("ping sd %d", p->sd);

    if (c_send_data(p, mqtt_ping_response, sizeof(mqtt_ping_response))) {
        return 1;
    }

    ka_update(&p->worker->keep_alive, p, p->recv_at);
    return 0;
}

void c_unbind(client_st *p, const char *reason)
{
    LOG_DEBUG("c_unbind %d %s", p->sd, reason);

    if (p->state == CLIENT_CONNECTED) {
        c_pub_disconnected(p, reason);
    }

    ASSERT(p->state != CLIENT_CLOSING);
    p->state = CLIENT_CLOSING;

    worker_rm_client(p->worker, p);
    ka_remove(&p->worker->keep_alive, p);

    if (p->ws) {
        free(p->ws);
    }
    free(p);
}

/** send data on wire and return number of bytes sent */
int c_send_data_wire(client_st *p, const unsigned char *buf, unsigned int len_in)
{
    int rv;
    int off;
    unsigned int len = len_in;

    ASSERT(p->state != CLIENT_CLOSING);
    off = 0;
    while (len) {
        rv = send(p->sd, buf + off, len, 0);
        if (rv < 0) {
            snprintf(l_reason, sizeof(l_reason),
                    "send error rv %d, errno %d (%s)", rv, errno, strerror(errno));
            c_disconnect(p, l_reason);
            return 0;
        }
        else if (rv == 0) {
            break;
        }
        len -= rv;
        off += rv;
    }
    LOG_DEBUG("c_send_data_wire sd %d, len %d", p->sd, len_in);
    return off;
}

int c_send_data_ws(client_st *p, const unsigned char *buf, unsigned int len)
{
    unsigned char ws_hdr[10];  // Max websocket header size if there is no mask
    unsigned char *cp = ws_hdr;

    LOG_DEBUG("c_send_data_ws %d", len);

    // Set up the websocket header
    *cp++ = 0x82;  // FIN_BIT | BINARY_OPCODE

    if (len < 126) {
        *cp++ = len & 0x7F;
    }
    else if (len < (1 << 16)) {
        *cp++ = 126;
        *cp++ = (len >> 8) & 0xFF;
        *cp++ = len & 0xFF;
    }
    else {
        c_disconnect(p, "c_send_data_ws too big");
        return 0;
    }
    c_send_data_wire(p, ws_hdr, cp - ws_hdr);
    c_send_data_wire(p, buf, len);
    return len;
}

/** Send all the clear text data and return 0 on success */
int c_send_data(client_st *p, const unsigned char *buf, unsigned int in_len)
{
    unsigned int len = in_len;
    int rv;

    ASSERT(p->state != CLIENT_CLOSING);
    while (len) {
        if (p->is_tls) {
            rv = tls_write(p, buf, len);
        }
        else if (p->is_ws) {
            rv = c_send_data_ws(p, buf, len);
        }
        else {
            rv = c_send_data_wire(p, buf, len);
        }
        if (rv <= 0) {
            return 1;
        }
        len -= rv;
    }
    return 0;
}

void c_disconnect_normal(client_st *p, const char *reason)
{
    int rv;
    log_info("closing sd %d %s", p->sd, reason);
    rv = close(p->sd);
    c_unbind(p, reason);
}

/** disconnect if not already */
void c_disconnect(client_st *p, const char *reason)
{
    int rv;
    if (p->state == CLIENT_CLOSING) {
        return;
    }
    log_warn("closing sd %d %s", p->sd, reason);
    rv = close(p->sd);
    c_unbind(p, reason);
}

/** read from socket into buf up to len,
 * return -1 on error, otherwise number of bytes read
 */
int c_recv_wire(client_st *p, unsigned char *buf, int len)
{
    int rv;
    rv = recv(p->sd, buf, len, 0);
    if (rv == 0 || (rv < 0 && errno != EAGAIN)) {
        snprintf(l_reason, sizeof(l_reason), "recv failed rv %d, errno %d (%s)",
                rv, errno, strerror(errno));
        c_disconnect(p, l_reason);
        return -1;
    }
    return rv;
}

/** called by worker when sd is ready to read */
void c_recv(client_st *p)
{
    if (!p->recv_ready) { return; }

    p->recv_ready = 0;
    int rv;
    unsigned char *buf;
    unsigned int len;

    if (p->is_tls) {
        buf = tls_recv_buf;
        len = sizeof(tls_recv_buf);
    }
    else {
        buf = c_recv_buf;
        len = sizeof(c_recv_buf);
    }

    rv = c_recv_wire(p, buf, len);
    if (rv <= 0) {
        return;
    }

    if (p->is_tls) {
        tls_recv(p, buf, rv);
        return;
    }
    else if (p->is_ws) {
        c_ws_recv(p, buf, rv);
    }
    else {
        c_parse(p, buf, rv);
    }
}

void c_parse(client_st *p, const unsigned char *buf, unsigned int len)
{
    if (mqtt_parser_execute(&p->parser, buf, len)) {
        c_disconnect(p, "mqtt parse error");
    }
}

void c_deliver(subscription_st *sp, message_st *mp)
{
    int len;
    client_st *p = sp->client;
    uint8_t qos;
    uint16_t msg_id = 0;

    if (p->state != CLIENT_CONNECTED) {
        return;
    }

    qos = MIN(mp->orig_qos, sp->qos);
    if (qos > 0) {
        msg_id = ++p->msg_id;
    }
    LOG_DEBUG("c_deliver sd %d topic %s, payload %.*s", p->sd, mp->topic, mp->payload_len, mp->payload);
    len = encode_publish(c_send_buf, sizeof(c_send_buf), mp, qos, msg_id);
    c_send_data(p, c_send_buf, len);
}

static __thread char payload[256];

static void c_pub_connected(client_st *p)
{
    message_st *mp;
    int topic_len;
    int payload_len;

    if (sys_connected_len == 0) {
        sys_connected_len = strlen(sys_connected) - 4 + strlen(config.name);
    }
    topic_len = sys_connected_len + p->client_id_len;
    payload_len = snprintf(payload, sizeof(payload), "{\"client_id\":\"%s\",\"username\":\"%s\",\"ip_address\":\"%s\",\"ts\":%ld}",
            p->client_id, p->username, p->ip_address, p->recv_at);

    LOG_DEBUG("connected: %s", payload);

    mp = message_create(p->worker->me, topic_len, payload_len, 0, 0, 0, 0);
    if (mp == NULL) {
        return;
    }
    sprintf((char *)mp->topic, sys_connected, config.name, p->client_id);
    memcpy(mp->payload, payload, payload_len);

    workers_queue_message(mp);
}

static void c_pub_disconnected(client_st *p, const char *reason)
{
    message_st *mp;
    int topic_len;
    int payload_len;

    LOG_DEBUG("c_pub_disconnected");
    if (sys_disconnected_len == 0) {
        sys_disconnected_len = strlen(sys_disconnected) - 4 + strlen(config.name);
    }
    topic_len = sys_disconnected_len + p->client_id_len;

    payload_len = snprintf(payload, sizeof(payload), "{\"client_id\":\"%s\",\"reason\":\"%s\",\"ts\":%ld}",
            p->client_id, reason, p->recv_at);

    LOG_DEBUG("disconnected: %s", payload);

    mp = message_create(p->worker->me, topic_len, payload_len, 0, 0, 0, 0);
    if (mp == NULL) {
        return;
    }
    sprintf((char *)mp->topic, sys_disconnected, config.name, p->client_id);
    memcpy(mp->payload, payload, payload_len);

    workers_queue_message(mp);
}
