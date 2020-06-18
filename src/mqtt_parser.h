
#ifndef MQTT_PARSER_H_
#define MQTT_PARSER_H_

#include <mqtt_packet.h>


/** max length of client_id, username, and password */
#define MAX_ID_LEN              127

/** max topic length */
#define MAX_TOPIC_LEN           127

/** max payload length */
#define MAX_PAYLOAD_LEN         4096

/** max number of topics in a subscription */
#define MAX_NUM_TOPICS          10


struct message;
typedef struct message message_st;

struct mqtt_parser;
typedef struct mqtt_parser mqtt_parser_t;

struct client;
typedef struct client client_st;

struct connect_s {
    struct flags_s {
        unsigned char will;
        unsigned char will_qos;
        unsigned char will_retain;
        unsigned char cleansession;
        unsigned char username_present;
        unsigned char password_present;
    } flags;
    char protocol[7];
    uint16_t u16;
    uint16_t will_topic_len;
    unsigned char will_topic[MAX_TOPIC_LEN + 1];
    unsigned char password[MAX_ID_LEN + 1];
};
typedef struct connect_s connect_st;

struct subscribe_s {
    uint16_t packet_id;
    uint16_t u16;
    int num_filters;
    unsigned int filter_lengths[MAX_NUM_TOPICS];
    unsigned char *filters[MAX_NUM_TOPICS];
    unsigned int qos[MAX_NUM_TOPICS];
};
typedef struct subscribe_s subscribe_st;

struct publish_s {
    uint16_t topic_len;
    uint32_t payload_len;
    unsigned char topic[MAX_TOPIC_LEN + 1];
    message_st *mp;
};


typedef struct {
    int (*on_recv_connect)(mqtt_parser_t *, connect_st *);
    int (*on_recv_subscribe)(mqtt_parser_t *, subscribe_st *);
    int (*on_recv_unsubscribe)(mqtt_parser_t *, subscribe_st *);
    int (*on_recv_publish)(mqtt_parser_t *, message_st *mp);
    int(*on_recv_ping)(mqtt_parser_t *);
} mqtt_parser_callbacks_t;

struct mqtt_parser {
    client_st *client;
    unsigned int client_id_len;
    unsigned char client_id[MAX_ID_LEN + 1];
    unsigned int username_len;
    unsigned char username[MAX_ID_LEN + 1];
    header_byte_t header;
    uint32_t rem_len;
    uint16_t i;
    int state;
    int multiplier;
    mqtt_parser_callbacks_t *callbacks;
    union {
        struct connect_s connect;
        struct subscribe_s subscribe;
        struct publish_s publish;
    } u;
};
typedef struct mqtt_parser mqtt_parser_t;


void mqtt_parser_init(mqtt_parser_t* parser);

int mqtt_parser_execute(mqtt_parser_t* parser, const unsigned char *buff, unsigned int len);

#endif
