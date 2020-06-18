
#ifndef MQTT_PACKET_H_
#define MQTT_PACKET_H_

#include <config.h>
#include <queue.h>

#include <stdint.h>


#define MESSAGE_SIZE(topic_len, payload_len) \
    (sizeof(message_st) + (topic_len) + (payload_len) + 2)

typedef enum {
    NONE, CONNECT, CONNACK, PUBLISH, PUBACK, PUBREC, PUBREL,
    PUBCOMP, SUBSCRIBE, SUBACK, UNSUBSCRIBE, UNSUBACK,
    PINGREQ, PINGRESP, DISCONNECT, MQTT_NUM_TYPES
} msg_type_t;

typedef struct {
    unsigned char msg_type;
    unsigned char dup;
    unsigned char qos;
    unsigned char retain;
} header_byte_t;

typedef struct {
    unsigned int len;
    char pad[4];
    const unsigned char *data;
} lenstring_st;

struct client;
typedef struct client client_st;

struct message;
typedef struct message message_st;

typedef enum {
    M_STATE_UNKNOWN,
    M_STATE_IN_USE,
    M_STATE_FREE
} message_state_et;

struct message {
    message_state_et state;
    uint8_t dup;
    uint8_t orig_qos;
    uint8_t retain;
    uint16_t packet_id;
    uint16_t topic_len;
    unsigned int payload_len;
    unsigned char *topic;
    unsigned char *payload;
    client_st *client;
    queue_item_st gc_node;
    queue_item_st nodes[MAX_NUM_WORKERS];
    int not_done[MAX_NUM_WORKERS];
};


message_st * message_create(client_st *p,
        uint16_t topic_len,
        unsigned int payload_len,
        int dup, int qos, int retain, int packet_id);

char *mqtt_msg_type_name(msg_type_t type);

char mqtt_topic_matches(const unsigned char *topic_filter, const unsigned char *topic_name, int topic_len);

#endif  /* MQTT_PACKET_H_ */
