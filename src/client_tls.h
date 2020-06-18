
#include <openssl/ssl.h>
#include <stdint.h>
#include <client.h>

#ifndef TLS_H_
#define TLS_H_



extern __thread unsigned char tls_recv_buf[1500];

SSL_CTX* tls_init(void);

int tls_open(SSL_CTX *ctx, client_st *cp);

int tls_recv(client_st *cp, unsigned char* src, int len);

int tls_write(client_st *cp, const unsigned char *buf, int len);

#endif  /* TLS_H_ */
