
#ifndef AUTH_H_
#define AUTH_H_

struct client;
typedef struct client client_st;

typedef enum {
    AUTH_SUB = 1,
    AUTH_PUB = 2,
} auth_access_et;

void auth_init(void);

void *auth_thread_init(void);

/** return 0 if a new connection with these attributes is authenticated */
int auth_auth(void *authp, client_st *p, const unsigned char *password);

/** return 0 if this authenticated user is superuser */
int auth_super(void *authp, client_st *p);

/** return 0 if a subscription or publish is allowed */
int auth_acl(void *authp, client_st *p, auth_access_et access, const unsigned char *filter, int filter_len);

/** free any per-client storage */
void auth_client_destroy(client_st *p);

#endif  /* AUTH_H_ */
