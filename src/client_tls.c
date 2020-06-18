
#include <client_tls.h>
#include <common.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

//#define DEBUG 1
#include <debug.h>


/** buffer for receiving encrypted data */
__thread unsigned char tls_recv_buf[1500];

/** buffer for sending encrypted data */
__thread unsigned char tls_send_buf[1500];


SSL_CTX *tls_init(void)
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    LOG_DEBUG("tls_init");

    SSL_library_init();
    SSL_load_error_strings();

    OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
    method = TLS_server_method();  /* Create new client-method instance */
    ctx = SSL_CTX_new(method);   /* Create new context */
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, config.tls_file_crt, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, config.tls_file_key, SSL_FILETYPE_PEM) <= 0 ) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

int tls_open(SSL_CTX *ctx, client_st *cp)
{
    int rv;
    cp->rbio = BIO_new(BIO_s_mem());
    cp->wbio = BIO_new(BIO_s_mem());
    cp->ssl = SSL_new(ctx);
    if (cp->rbio == NULL || cp->wbio == NULL || cp->ssl == NULL) {
        LOG_DEBUG("tls_open %d failed", cp->sd);
        return 1;
    }
    SSL_set_accept_state(cp->ssl);
    SSL_set_bio(cp->ssl, cp->rbio, cp->wbio);
    LOG_DEBUG("tls_open %d", cp->sd);
    return 0;
}

/** call by client when encrypted data received on socket
 * return number of bytes received */
int tls_recv(client_st *cp, unsigned char* src, int len)
{
    int n;
    int rv;

    LOG_DEBUG("tls_recv %d %d", cp->sd, len);

    while (len > 0) {
        n = BIO_write(cp->rbio, src, len);

        if (n <= 0) {
            c_disconnect(cp, "tls_recv BIO_write rbio");
            return 0;
        }

        src += n;
        len -= n;

        if (!SSL_is_init_finished(cp->ssl)) {
            n = SSL_accept(cp->ssl);
            rv = SSL_get_error(cp->ssl, n);
            if (rv == SSL_ERROR_WANT_WRITE || rv == SSL_ERROR_WANT_READ) {
                do {
                    n = BIO_read(cp->wbio, tls_send_buf, sizeof(tls_send_buf));
                    if (n > 0) {
                        rv = c_send_data_wire(cp, tls_send_buf, n);
                        if (rv != n) {
                            LOG_DEBUG("tls_recv send error rv %d, n %d", rv, n);
                            c_disconnect(cp, "tls_recv send error");
                            return 0;
                        }
                    }
                }
                while (n > 0);
            }
            else if (rv != SSL_ERROR_NONE) {
                LOG_DEBUG("tls_recv SSL_accept error %d", rv);
                c_disconnect(cp, "tls_recv SSL_accept error");
                return 0;
            }

            if (!SSL_is_init_finished(cp->ssl)) {
                LOG_DEBUG("tls_recv init not finished");
                return 0;
            }
        }

        /* The encrypted data is now in the input bio so now we can perform actual
         * read of unencrypted data. */
        do {
            n = SSL_read(cp->ssl, c_recv_buf, sizeof(c_recv_buf));
            if (n > 0) {
                c_parse(cp, c_recv_buf, n);
            }
        }
        while (n > 0);

        /* Did SSL request to write bytes? This can happen if peer has requested SSL
         * renegotiation. */
        rv = SSL_get_error(cp->ssl, n);
        if (rv == SSL_ERROR_WANT_WRITE || rv == SSL_ERROR_WANT_READ) {
            do {
                n = BIO_read(cp->wbio, tls_send_buf, sizeof(tls_send_buf));
                if (n > 0) {
                    c_send_data_wire(cp, tls_send_buf, n);
                }
            }
            while (n > 0);
            if (n < 0) {
                rv = SSL_get_error(cp->ssl, n);
                if (rv != SSL_ERROR_NONE && rv != SSL_ERROR_WANT_WRITE && rv != SSL_ERROR_WANT_READ) {
                    LOG_DEBUG("tls_recv BIO_read wbio error rv %d, n %d", rv, n);
                    c_disconnect(cp, "tls_recv BIO_read wbio");
                    return 0;
                }
            }
        }
        else if (rv != SSL_ERROR_NONE) {
            LOG_DEBUG("tls_recv SSL_read error rv %d, n %d", rv, n);
            c_disconnect(cp, "tls_recv SSL_read");
            return 0;
        }
    }

    return 0;
}

int tls_write(client_st *cp, const unsigned char *buf, int len)
{
    int rv;
    int n;

    LOG_DEBUG("tls_write %d %d", cp->sd, len);

    if (!SSL_is_init_finished(cp->ssl))
        return 0;

    n = SSL_write(cp->ssl, buf, len);
    rv = SSL_get_error(cp->ssl, n);

    if (n > 0) {
        /* take the output of the SSL object and queue it for socket write */
        do {
            rv = BIO_read(cp->wbio, tls_send_buf, sizeof(tls_send_buf));
            if (rv > 0) {
                c_send_data_wire(cp, tls_send_buf, rv);
            }
        } while (rv > 0);
        return n;
    }
    else if (rv != SSL_ERROR_NONE) {
        LOG_DEBUG("tls_write SSL_write error %d", rv);
        c_disconnect(cp, "tls_write SSL_write");
        return 0;
    }
    return 0;
}

void tls_close(SSL_CTX *ctx, SSL *ssl)
{
    LOG_DEBUG("ssl_close");
    close(SSL_get_fd(ssl));         /* close socket */
    SSL_free(ssl);        /* release connection state */
    SSL_CTX_free(ctx);        /* release context */
}
