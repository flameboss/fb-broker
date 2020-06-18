
#include <common.h>

inline message_st *message_create(client_st *client,
        uint16_t topic_len,
        unsigned int payload_len,
        int dup, int orig_qos, int retain, int packet_id)
{
    message_st *mp;
    mp = malloc(MESSAGE_SIZE(topic_len, payload_len));
    if (mp == NULL) {
        return mp;
    }
    mp->topic_len = topic_len;
    mp->payload_len = payload_len;
    mp->topic = (unsigned char *)mp + sizeof(message_st);
    mp->topic[topic_len] = '\0';
    mp->payload = (unsigned char *)mp + sizeof(message_st) + topic_len + 1;
    mp->payload[payload_len] = '\0';
    mp->client = client;
    mp->dup = dup;
    mp->orig_qos = orig_qos;
    mp->retain = retain;
    mp->packet_id = packet_id;
    mp->state = M_STATE_IN_USE;
    return mp;
}
