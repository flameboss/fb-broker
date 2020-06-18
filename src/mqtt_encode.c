
/*
 * Encode MQTT packets
 */

#include <mqtt_encode.h>
#include <common.h>
//#define DEBUG 1
#include <debug.h>


const unsigned char mqtt_ping_response[2] = { (PINGRESP << 4), 0 };


static unsigned int packet_len(int rem_len)
{
    rem_len += 1; /* header byte */

    /* now remaining_length field */
    if (rem_len < 128)
        rem_len += 1;
    else if (rem_len < 16384)
        rem_len += 2;
    else if (rem_len < 2097151)
        rem_len += 3;
    else
        rem_len += 4;
    return rem_len;
}

static unsigned char encode_header_byte(unsigned char type, unsigned char dup, unsigned char qos, unsigned char retain)
{
    unsigned char rv;
    rv = ((type << 4) | (dup << 3) | (qos << 1) | retain);
    return rv;
}

static int encode_rem_len(unsigned char* buf, int length)
{
    int rc = 0;
    do {
        unsigned char d = (length % 128);
        length /= 128;
        /* if there are more digits to encode, set the top bit of this digit */
        if (length > 0)
            d |= 0x80;
        buf[rc++] = d;
    } while (length > 0);
    return rc;
}

static void encode_short(unsigned char** pptr, unsigned short anInt)
{
    **pptr = (anInt / 256);
    (*pptr)++;
    **pptr = (anInt % 256);
    (*pptr)++;
}


static void encode_string(unsigned char** pptr, const unsigned char *string, int len)
{
    encode_short(pptr, len);
    if (len > 0) {
        memcpy(*pptr, string, len);
        *pptr += len;
    }
}

unsigned int encode_conack(unsigned char *buf, unsigned int len, unsigned char session_present, unsigned char rc)
{
    unsigned int rv;
    unsigned char *ptr = buf;

    if (len < 2) {
        log_error("encode conack buf too short");
    }

    *ptr++ = encode_header_byte(CONNACK, 0u, 0u, 0u);
    ptr += encode_rem_len(ptr, 2); /* write remaining length */
    *ptr++ = session_present;
    *ptr++ = rc;

    rv = (ptr - buf);
    return rv;
}

unsigned int encode_ack(unsigned char *buf, unsigned int len,
        unsigned char packettype, unsigned char dup, unsigned short packetid)
{
    unsigned int rv;
    unsigned char *ptr = buf;

    if (len < 4) {
        log_error("encode ack buf too short");
        return 0;
    }

    *ptr++ = encode_header_byte(packettype, dup, (packettype == PUBREL) ? 1u : 0u, 0u);
    ptr += encode_rem_len(ptr, 2); /* write remaining length */
    encode_short(&ptr, packetid);
    rv = (ptr - buf);
    return rv;
}

unsigned int encode_zero(unsigned char *buf, unsigned int len, unsigned char packettype)
{
    unsigned int rv;
    unsigned char *ptr = buf;

    if (len < 2) {
        log_error("encode zero buf too short");
    }

    *ptr++ = encode_header_byte(packettype, 0u, 0u, 0u);
    ptr += encode_rem_len(ptr, 0); /* write remaining length */
    rv = (ptr - buf);
    return rv;
}

static unsigned int publish_len(int qos, int topic_len, int payload_len)
{
    int len;

    len = 2 + topic_len + payload_len;
    if (qos > 0) {
        /* packet id */
        len += 2;
    }
    return len;
}

/** return length encoded */
unsigned int encode_publish(unsigned char *buf, unsigned int len, message_st *mp,
        uint8_t qos, uint16_t msg_id)
{
    unsigned int rv;
    unsigned char *ptr = buf;
    unsigned int rem_len;

    rem_len = publish_len(qos, mp->topic_len, mp->payload_len);

    if (packet_len(rem_len) > len) {
        log_error("encode publish buf too short");
        return 0;
    }

    *ptr++ = encode_header_byte(PUBLISH, mp->dup, qos, mp->retain);

    ptr += encode_rem_len(ptr, rem_len);

    encode_string(&ptr, mp->topic, mp->topic_len);

    if (qos > 0) {
        encode_short(&ptr, msg_id);
    }

#if DEBUG
    for (int i = 0; i < mp->payload_len; ++i) {
        ASSERT(mp->payload[i] != '\0');
    }
#endif
    memcpy(ptr, mp->payload, mp->payload_len);
    ptr += mp->payload_len;

    rv = (ptr - buf);
    return rv;
}

unsigned int encode_suback(unsigned char* buf, int buflen,
        unsigned short packetid, int count, int* grantedQoSs)
{
    unsigned char *ptr = buf;
    int i;

    if (buflen < 2 + count) {
        log_error("encode suback buf too short");
        return 0;
    }
    *ptr++ = SUBACK << 4;

    ptr += encode_rem_len(ptr, 2 + count);
    encode_short(&ptr, packetid);

    for (i = 0; i < count; ++i) {
        *ptr++ = grantedQoSs[i];
    }

    return ptr - buf;
}

unsigned int encode_unsuback(unsigned char* buf, int buflen, unsigned short packetid)
{
    unsigned char *ptr = buf;

    ASSERT(buflen >= 4);

    *ptr++ = UNSUBACK << 4;
    ptr += encode_rem_len(ptr, 2);
    encode_short(&ptr, packetid);

    return ptr - buf;
}


