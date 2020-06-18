
#ifndef MQTT_ENCODE_H_
#define MQTT_ENCODE_H_

#include <mqtt_packet.h>


extern const unsigned char mqtt_ping_response[2];


unsigned int encode_zero(unsigned char *buf, unsigned int len, unsigned char packettype);

unsigned int encode_conack(unsigned char *buf, unsigned int len, unsigned char session_present, unsigned char rc);

unsigned int encode_ack(unsigned char *buf, unsigned int len,
        unsigned char packettype, unsigned char dup, unsigned short packetid);

unsigned int encode_suback(unsigned char* buf, int buflen,
        unsigned short packetid, int count, int* grantedQoSs);

unsigned int encode_unsuback(unsigned char* buf, int buflen,
        unsigned short packetid);

unsigned int encode_publish(unsigned char *buf, unsigned int len, message_st *mp,
        uint8_t qos, uint16_t msg_id);

#endif /* MQTT_ENCODE_H_ */
