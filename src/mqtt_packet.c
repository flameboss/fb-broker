
#include <mqtt_packet.h>
#include <common.h>


static char *msg_type_names[] = { "NONE", "CONNECT", "CONNACK", "PUBLISH",
        "PUBACK", "PUBREC", "PUBREL", "PUBCOMP", "SUBSCRIBE", "SUBACK",
        "UNSUBSCRIBE", "UNSUBACK", "PINGREQ", "PINGRESP", "DISCONNECT",
        "MQTT_NUM_TYPES" };


char *mqtt_msg_type_name(msg_type_t type)
{
    if (type < MQTT_NUM_TYPES) {
        return msg_type_names[type];
    }
    else {
        return "unknown";
    }
}

char mqtt_topic_matches(const unsigned char *topicFilter, const unsigned char *topicName, int topicLen)
{
    const unsigned char *curf;
    const unsigned char *curn;
    const unsigned char *curn_end;

    if (strncmp((char *)topicName, "$local/", 7) == 0) {
        topicName += 7;
        topicLen -= 7;
    }

    curf = topicFilter;
    curn = topicName;
    curn_end = curn + topicLen;

    while (*curf && curn < curn_end) {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+') {
            // skip until we meet the next separator, or end of string
            const unsigned char *nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    }

    return (curn == curn_end) && (*curf == '\0');
}
