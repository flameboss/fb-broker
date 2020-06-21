
#include <mqtt_parser.h>
#include <common.h>

//#define DEBUG 1
#include <debug.h>

enum {
    S_HEADER,
    S_REM_LEN,

    S_CONNECT_PACKET_ID_0,
    S_CONNECT_PACKET_ID_1,

    S_CONNECT_PROTOCOL_0,
    S_CONNECT_PROTOCOL_1,
    S_CONNECT_PROTOCOL_2,

    S_CONNECT_VERSION,
    S_CONNECT_FLAGS,

    S_CONNECT_KEEP_ALIVE_0,
    S_CONNECT_KEEP_ALIVE_1,

    S_CONNECT_CLIENT_ID_0,
    S_CONNECT_CLIENT_ID_1,
    S_CONNECT_CLIENT_ID_2,

    S_CONNECT_WILL_TOPIC_0,
    S_CONNECT_WILL_TOPIC_1,
    S_CONNECT_WILL_TOPIC_2,

    S_CONNECT_WILL_PAYLOAD_0,
    S_CONNECT_WILL_PAYLOAD_1,
    S_CONNECT_WILL_PAYLOAD_2,

    S_CONNECT_USERNAME_0,
    S_CONNECT_USERNAME_1,
    S_CONNECT_USERNAME_2,

    S_CONNECT_PASSWORD_0,
    S_CONNECT_PASSWORD_1,
    S_CONNECT_PASSWORD_2,

    S_SUBSCRIBE_PACKET_ID_0,
    S_SUBSCRIBE_PACKET_ID_1,

    S_SUBSCRIBE_TOPIC_0,
    S_SUBSCRIBE_TOPIC_1,
    S_SUBSCRIBE_TOPIC_2,
    S_SUBSCRIBE_TOPIC_QOS,

    S_UNSUBSCRIBE_PACKET_ID_0,
    S_UNSUBSCRIBE_PACKET_ID_1,

    S_UNSUBSCRIBE_TOPIC_0,
    S_UNSUBSCRIBE_TOPIC_1,
    S_UNSUBSCRIBE_TOPIC_2,

    S_PUBLISH_TOPIC_0,
    S_PUBLISH_TOPIC_1,
    S_PUBLISH_TOPIC_2,

    S_PUBLISH_PACKET_ID_0,
    S_PUBLISH_PACKET_ID_1,

    S_PUBLISH_PAYLOAD
};

void mqtt_parser_init(mqtt_parser_t* parser)
{
    parser->state = S_HEADER;
}

/** parser mqtt message and return 0 on success */
int mqtt_parser_execute(mqtt_parser_t *parser, const unsigned char *buf, unsigned int len)
{
    int j;
    int rv;
    void *vp;
    client_st *p = parser->client;
    message_st *mp;

    LOG_DEBUG("mqtt_parser_execute %d", len);

    while (len) {
        //LOG_DEBUG("mqtt state %d x%x (%d), len %d", parser->state, *buf, *buf, len);
        switch (parser->state) {
        case S_HEADER:
            parser->header.msg_type = *buf >> 4;
            parser->header.dup = (*buf & (1 << 3)) != 0;
            parser->header.qos = (*buf >> 1) & 0b11;
            parser->header.retain = ((*buf & 1) == 1);

            LOG_DEBUG("header %s", mqtt_msg_type_name(parser->header.msg_type));
            if (parser->header.msg_type == NONE ||
                    parser->header.msg_type >= MQTT_NUM_TYPES) {
                log_error("invalid msg type %d", parser->header.msg_type);
                return 1;
            }
            if (parser->header.retain) {
                log_warn("retain not supported");
                return 1;
            }

            parser->rem_len = 0;
            parser->multiplier = 1;
            ++parser->state;
            ++buf; --len;
            break;

        case S_REM_LEN:
            parser->rem_len += (*buf & 127) * parser->multiplier;
            parser->multiplier *= 128;
            LOG_DEBUG("rem_len x%x x%x (%d)", *buf, parser->rem_len, parser->rem_len);
            if ((*buf & 128) == 0) {
                int rv;
                switch (parser->header.msg_type) {
                case CONNECT:
                    parser->state = S_CONNECT_PROTOCOL_0;
                    break;
                case DISCONNECT:
                    c_disconnect_normal(p, "disconnect");
                    parser->state = S_HEADER;
                    return 0;
                case PINGREQ:
                    if (parser->callbacks->on_recv_ping(parser)) {
                        return 1;
                    }
                    parser->state = S_HEADER;
                    break;
                case SUBSCRIBE:
                    parser->state = S_SUBSCRIBE_PACKET_ID_0;
                    break;
                case UNSUBSCRIBE:
                    parser->state = S_UNSUBSCRIBE_PACKET_ID_0;
                    break;
                case PUBLISH:
                    parser->state = S_PUBLISH_TOPIC_0;
                    break;
                default:
                    log_error("unsupported msg type %d", parser->header.msg_type);
                    return 1;
                }
            }
            ++buf; --len;
            break;

        /*
         * CONNECT
         */

        case S_CONNECT_PROTOCOL_0:
            parser->u.connect.u16 = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_CONNECT_PROTOCOL_1:
            parser->u.connect.u16 |= *buf;
            if (parser->u.connect.u16 > sizeof(parser->u.connect.protocol)-1) {
                log_error("connect protocol len %d", parser->u.connect.u16);
                return 1;
            }
            parser->i = 0;
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_PROTOCOL_2:
            parser->u.connect.protocol[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == parser->u.connect.u16) {
                ++parser->state;
            }
            break;
        case S_CONNECT_VERSION:
            if (parser->u.connect.u16 == 4 &&
                    *buf == 4 &&
                    memcmp(parser->u.connect.protocol, "MQTT", 4) == 0) {
                /* ok - do nothing */
            }
            else if (parser->u.connect.u16 == 6 &&
                    *buf == 3 &&
                    memcmp(parser->u.connect.protocol, "MQIsdp", 6) == 0) {
                /* ok - do nothing */
            }
            else {
                log_error("protocol/version %.*s/%d", parser->u.connect.u16, parser->u.connect.protocol, *buf);
                return 1;
            }
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_FLAGS:
            parser->u.connect.flags.cleansession = (*buf >> 1) & 1;
            parser->u.connect.flags.will = (*buf >> 2) & 1;
            parser->u.connect.flags.will_qos = (*buf >> 3) & 3;
            parser->u.connect.flags.will_retain = (*buf >> 5) & 1;
            parser->u.connect.flags.password_present = (*buf >> 6) & 1;
            parser->u.connect.flags.username_present = *buf >> 7;
            LOG_DEBUG("connect will %d, will_qos %d, will_retain %d, pass %d, un %d",
                    parser->u.connect.flags.will, parser->u.connect.flags.will_qos, parser->u.connect.flags.will_retain,
                    parser->u.connect.flags.password_present,
                    parser->u.connect.flags.username_present);
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_KEEP_ALIVE_0:
            p->keep_alive = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_CONNECT_KEEP_ALIVE_1:
            p->keep_alive |= *buf;
            ++parser->state;
            ++buf; --len;
            break;

        case S_CONNECT_CLIENT_ID_0:
            p->client_id_len = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_CONNECT_CLIENT_ID_1:
            p->client_id_len |= *buf;
            if (p->client_id_len == 0 || p->client_id_len > MAX_ID_LEN) {
                log_error("client_id len %d", parser->client_id_len);
                return 1;
            }
            parser->i = 0;
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_CLIENT_ID_2:
            p->client_id[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == p->client_id_len) {
                p->client_id[parser->i] = '\0';
                LOG_DEBUG("client_id %s", p->client_id);
                if (parser->u.connect.flags.will) {
                    ++parser->state;
                }
                else if (parser->u.connect.flags.username_present) {
                    parser->state = S_CONNECT_USERNAME_0;
                }
                else if (parser->u.connect.flags.password_present) {
                    log_error("password but not username");
                    return 1;
                }
                else if (!config.allow_anonymous) {
                    log_error("anonymous connection not allowed");
                    return 1;
                }
                else {
                    if (parser->callbacks->on_recv_connect(parser, &parser->u.connect)) {
                        return 1;
                    }
                    parser->state = S_HEADER;
                }
            }
            break;

        case S_CONNECT_WILL_TOPIC_0:
            parser->u.connect.will_topic_len = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_CONNECT_WILL_TOPIC_1:
            parser->u.connect.will_topic_len |= *buf;
            if (parser->u.connect.will_topic_len == 0 || parser->u.connect.will_topic_len > sizeof(p->will_topic)) {
                log_error("client_id len %d", parser->u.connect.will_topic_len);
                return 1;
            }
            parser->i = 0;
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_WILL_TOPIC_2:
            p->will_topic[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == parser->u.connect.will_topic_len) {
                ++parser->state;
            }
            break;

        case S_CONNECT_WILL_PAYLOAD_0:
            parser->u.connect.u16 = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_CONNECT_WILL_PAYLOAD_1:
            parser->u.connect.u16 |= *buf;
            if (parser->u.connect.u16 == 0 || parser->u.connect.u16 > MAX_PAYLOAD_LEN) {
                log_error("connect will payload len %d", parser->u.connect.u16);
                return -1;
            }
            LOG_DEBUG("connect will topic_len %d, payload_len %d", parser->u.connect.will_topic_len, parser->u.connect.u16);
            parser->i = 0;
            mp = message_create(p, parser->u.connect.will_topic_len, parser->u.connect.u16,
                                0, parser->header.qos, parser->header.retain, 0);
            if (mp == NULL) {
                return 1;
            }
            memcpy(mp->topic, parser->u.connect.will_topic, parser->u.connect.will_topic_len);
            p->will_mp = mp;
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_WILL_PAYLOAD_2:
            p->will_mp->payload[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == parser->u.connect.u16) {
                if (parser->u.connect.flags.username_present) {
                    ++parser->state;
                }
                else {
                    log_error("anonymous connection");
                    return -1;
                }
            }
            break;

        case S_CONNECT_USERNAME_0:
            p->username_len = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_CONNECT_USERNAME_1:
            p->username_len |= *buf;
            if (p->username_len == 0 || p->username_len > MAX_ID_LEN) {
                log_error("username len %d", p->username_len);
                return 1;
            }
            parser->i = 0;
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_USERNAME_2:
            p->username[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == p->username_len) {
                p->username[parser->i] = '\0';
                LOG_DEBUG("username %s", p->username);
                ++parser->state;
            }
            break;

        case S_CONNECT_PASSWORD_0:
            parser->u.connect.u16 = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_CONNECT_PASSWORD_1:
            parser->u.connect.u16 |= *buf;
            if (parser->u.connect.u16 == 0 || parser->u.connect.u16 > MAX_ID_LEN) {
                log_error("mqtt_parser_execute connect protocol len %d", parser->u.connect.u16);
                return 1;
            }
            parser->i = 0;
            ++buf; --len;
            ++parser->state;
            break;
        case S_CONNECT_PASSWORD_2:
            parser->u.connect.password[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == parser->u.connect.u16) {
                parser->u.connect.password[parser->i] = '\0';
                if (parser->callbacks->on_recv_connect(parser, &parser->u.connect)) {
                    return 1;
                }
                parser->state = S_HEADER;
            }
            break;

        /*
         * SUBSCRIBE
         */

        case S_SUBSCRIBE_PACKET_ID_0:
            LOG_DEBUG("sub start");
            parser->u.subscribe.packet_id = *buf << 8;
            ++buf; --len;
            ++parser->state;
            break;
        case S_SUBSCRIBE_PACKET_ID_1:
            parser->u.subscribe.packet_id |= *buf;
            ++buf; --len;
            ++parser->state;
            LOG_DEBUG("sub packet_id %d", parser->u.subscribe.packet_id);
            parser->u.subscribe.num_filters = 0;
            parser->rem_len -= 2;
            break;

        case S_SUBSCRIBE_TOPIC_0:
            parser->u.subscribe.u16 = *buf << 8;
            ++buf; --len;
            ++parser->state;
            break;
        case S_SUBSCRIBE_TOPIC_1:
            parser->u.subscribe.u16 |= *buf;
            LOG_DEBUG("sub len %d / %d", parser->u.subscribe.u16, parser->rem_len);
            if (parser->u.subscribe.u16 + 3u > parser->rem_len) {
                log_error("subcribe length error %d", parser->u.subscribe.u16);
                return 1;
            }
            ++buf; --len;
            vp = malloc(parser->u.subscribe.u16 + 1);
            if (vp == NULL) {
                log_error("subscribe malloc %d failed", parser->u.subscribe.u16 + 1);
                return 1;
            }
            parser->u.subscribe.filter_lengths[parser->u.subscribe.num_filters] = parser->u.subscribe.u16;
            parser->u.subscribe.filters[parser->u.subscribe.num_filters] = vp;
            parser->u.subscribe.filters[parser->u.subscribe.num_filters][parser->u.subscribe.u16] = '\0';
            ++parser->state;
            parser->u.subscribe.u16 = 0;
            break;
        case S_SUBSCRIBE_TOPIC_2:
            parser->u.subscribe.filters[parser->u.subscribe.num_filters][parser->u.subscribe.u16] = *buf;
            //LOG_DEBUG("%.*s", parser->u16, parser->u.subscribe.filters[parser->u.subscribe.num_filters]);
            ++buf; --len;
            ++parser->u.subscribe.u16;
            if (parser->u.subscribe.u16 == parser->u.subscribe.filter_lengths[parser->u.subscribe.num_filters]) {
                parser->state = S_SUBSCRIBE_TOPIC_QOS;
            }
            break;
        case S_SUBSCRIBE_TOPIC_QOS:
            if (*buf > 2) {
                log_error("subcribe used reserved bits");
                return 1;
            }
            parser->u.subscribe.qos[parser->u.subscribe.num_filters] = *buf;
            ++buf; --len;
            parser->rem_len -= (parser->u.subscribe.filter_lengths[parser->u.subscribe.num_filters] + 3);

            LOG_DEBUG("sub %s", parser->u.subscribe.filters[parser->u.subscribe.num_filters]);

            ++parser->u.subscribe.num_filters;
            if (parser->rem_len > 0) {
                parser->state = S_SUBSCRIBE_TOPIC_0;
            }
            else {
                if (parser->callbacks->on_recv_subscribe(parser, &parser->u.subscribe)) {
                    return 1;
                }
                parser->state = S_HEADER;
            }
            break;

            /*
             * UNSUBSCRIBE
             */

            case S_UNSUBSCRIBE_PACKET_ID_0:
                LOG_DEBUG("sub start");
                parser->u.subscribe.packet_id = *buf << 8;
                ++buf; --len;
                ++parser->state;
                break;
            case S_UNSUBSCRIBE_PACKET_ID_1:
                parser->u.subscribe.packet_id |= *buf;
                ++buf; --len;
                ++parser->state;
                LOG_DEBUG("sub packet_id %d", parser->u.subscribe.packet_id);
                parser->u.subscribe.num_filters = 0;
                parser->rem_len -= 2;
                break;

            case S_UNSUBSCRIBE_TOPIC_0:
                parser->u.subscribe.u16 = *buf << 8;
                ++buf; --len;
                ++parser->state;
                break;
            case S_UNSUBSCRIBE_TOPIC_1:
                parser->u.subscribe.u16 |= *buf;
                LOG_DEBUG("sub len %d / %d", parser->u.subscribe.u16, parser->rem_len);
                if (parser->u.subscribe.u16 + 3u > parser->rem_len) {
                    log_error("unsub length error %d", parser->u.subscribe.u16);
                    return 1;
                }
                ++buf; --len;
                vp = malloc(parser->u.subscribe.u16 + 1);
                if (vp == NULL) {
                    log_error("unsub malloc %d failed", parser->u.subscribe.u16 + 1);
                    return 1;
                }
                parser->u.subscribe.filter_lengths[parser->u.subscribe.num_filters] = parser->u.subscribe.u16;
                parser->u.subscribe.filters[parser->u.subscribe.num_filters] = vp;
                parser->u.subscribe.filters[parser->u.subscribe.num_filters][parser->u.subscribe.u16] = '\0';
                ++parser->state;
                parser->u.subscribe.u16 = 0;
                break;
            case S_UNSUBSCRIBE_TOPIC_2:
                parser->u.subscribe.filters[parser->u.subscribe.num_filters][parser->u.subscribe.u16] = *buf;
                //LOG_DEBUG("%.*s", parser->u16, parser->u.subscribe.filters[parser->u.subscribe.num_filters]);
                ++buf; --len;
                ++parser->u.subscribe.u16;
                if (parser->u.subscribe.u16 == parser->u.subscribe.filter_lengths[parser->u.subscribe.num_filters]) {
                    parser->rem_len -= (parser->u.subscribe.filter_lengths[parser->u.subscribe.num_filters] + 2);
                    LOG_DEBUG("unsub %s", parser->u.subscribe.filters[parser->u.subscribe.num_filters]);
                    ++parser->u.subscribe.num_filters;

                    if (parser->rem_len > 0) {
                        parser->state = S_UNSUBSCRIBE_TOPIC_0;
                    }
                    else {
                        if (parser->callbacks->on_recv_unsubscribe(parser, &parser->u.subscribe)) {
                            return 1;
                        }
                        parser->state = S_HEADER;
                    }
                }
                break;

        /*
         * PUBLISH
         */

        case S_PUBLISH_TOPIC_0:
            parser->u.publish.topic_len = *buf << 8;
            ++parser->state;
            ++buf; --len;
            break;
        case S_PUBLISH_TOPIC_1:
            parser->u.publish.topic_len |= *buf;
            if (parser->u.publish.topic_len == 0 || parser->u.publish.topic_len > MAX_TOPIC_LEN) {
                log_error("publish topic len %d", parser->u.publish.topic_len);
                return 1;
            }
            parser->i = 0;
            LOG_DEBUG("pub topic_len %d", parser->u.publish.topic_len);
            ++buf; --len;
            ++parser->state;
            break;
        case S_PUBLISH_TOPIC_2:
            parser->u.publish.topic[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == parser->u.publish.topic_len) {
                parser->u.publish.topic[parser->i] = '\0';
                LOG_DEBUG("pub topic %s", parser->u.publish.topic);
                if (parser->header.qos) {
                    parser->u.publish.payload_len = parser->rem_len - parser->u.publish.topic_len - 4;
                    ++parser->state;
                }
                else {
                    parser->u.publish.payload_len = parser->rem_len - parser->u.publish.topic_len - 2;
                    parser->state = S_PUBLISH_PAYLOAD;
                }
                if (parser->u.publish.payload_len == 0) {
                    log_warn("pub empty payload");
                    return 1;
                }
                LOG_DEBUG("pub topic_len %d, payload_len %d",
                        parser->u.publish.topic_len, parser->u.publish.payload_len);

                parser->i = 0;

                mp = message_create(p, parser->u.publish.topic_len, parser->u.publish.payload_len,
                                    parser->header.dup, parser->header.qos, parser->header.retain, 0);
                if (mp == NULL) {
                    log_error("publish msg malloc");
                    return 1;
                }
                memcpy(mp->topic, parser->u.publish.topic, parser->u.publish.topic_len);
                parser->u.publish.mp = mp;
            }
            break;

        case S_PUBLISH_PACKET_ID_0:
            LOG_DEBUG("sub start");
            parser->u.publish.mp->packet_id = *buf << 8;
            ++buf; --len;
            ++parser->state;
            break;
        case S_PUBLISH_PACKET_ID_1:
            parser->u.publish.mp->packet_id |= *buf;
            ++buf; --len;
            ++parser->state;
            LOG_DEBUG("pub packet_id %d", parser->u.publish.mp->packet_id);
            break;

        case S_PUBLISH_PAYLOAD:
            //LOG_DEBUG("pub payload %d/%d", parser->i, parser->u.publish.payload_len);
            parser->u.publish.mp->payload[parser->i] = *buf;
            ++parser->i;
            ++buf; --len;
            if (parser->i == parser->u.publish.payload_len) {
                parser->u.publish.mp->payload[parser->i] = '\0';
                if (parser->callbacks->on_recv_publish(parser, parser->u.publish.mp)) {
                    return 1;
                }
                parser->state = S_HEADER;
                break;
            }
            break;
        }
    }

    return 0;
}
