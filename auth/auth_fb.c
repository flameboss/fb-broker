#include <auth.h>
#include <common.h>
#include <mysql.h>
//#define DEBUG 1
#include <debug.h>


#define MAX_QUERY_LEN       256

#define DEVICE_0_PASSWORD   "Icor6ekfmHO1"

struct auth_s {
    MYSQL mysql;
    char q[MAX_QUERY_LEN];
    unsigned char topic[MAX_TOPIC_LEN + 1];
};
typedef struct auth_s auth_st;


void auth_init()
{
    mysql_library_init(0, NULL, NULL);
}

void *auth_thread_init(void)
{
    auth_st *auth = malloc(sizeof(auth_st));
    const char *host = NULL, *user = NULL, *password = NULL, *name = NULL, *socket = NULL;
    int port;
    my_bool reconnect = 1;

    if (auth == NULL) {
        log_error("auth malloc failed");
        exit(1);
    }
    port = 3306;

    config_lookup_string(&config.lc_config, "auth_mysql_host", &host);
    config_lookup_int(&config.lc_config, "auth_mysql_port", &port);
    config_lookup_string(&config.lc_config, "auth_mysql_socket", &socket);

    config_lookup_string(&config.lc_config, "auth_mysql_user", &user);
    config_lookup_string(&config.lc_config, "auth_mysql_password", &password);
    config_lookup_string(&config.lc_config, "auth_mysql_database", &name);

    mysql_init(&auth->mysql);

    mysql_options(&auth->mysql, MYSQL_OPT_RECONNECT, &reconnect);

    if (mysql_real_connect(&auth->mysql, host, user, password, name, port, socket, 0) == NULL) {
        log_error("mysql connect error: %s", mysql_error(&auth->mysql));
        exit(1);
    }
    return auth;
}

/** return 0 if a new connection is authenticated */
int auth_auth(void *authp, client_st *p, const unsigned char *password)
{
    auth_st *auth = authp;
    int rv, srv;
    MYSQL_RES *result;
    MYSQL_ROW row;
    int device_0_flag = 0;

    rv = 1;

    if (p->username[0] == '\0' || p->username[1] != '-' || p->username[2] == '\0'
            || *password == '\0') {
        log_warn("auth failed: %s (invalid)", p->username);
        goto nocleanup;
    }

    switch (p->username[0]) {
    case 'U':
        snprintf(auth->q, MAX_QUERY_LEN, "select api_token, admin, id from users where username = '%s'",
                &p->username[2]);
        break;
    case 'D':
        if (p->username[2] == '0' && p->username[3] == '\0') {
            device_0_flag = 1;
            snprintf(auth->q, MAX_QUERY_LEN, "SELECT '%s', 0, 0 FROM production_ips WHERE address = '%s'",
                     DEVICE_0_PASSWORD, p->ip_address);
        }
        else {
            snprintf(auth->q, MAX_QUERY_LEN, "SELECT SUBSTR(aes_key, 1, 12), 0, 0 FROM devices WHERE id = '%s';", &p->username[2]);
        }
        break;
    case 'T':
        snprintf(auth->q, MAX_QUERY_LEN, "SELECT api_token, admin, id FROM users WHERE id = %s", &p->username[2]);
        break;
    }

    LOG_DEBUG("%s", auth->q);
    if (mysql_query(&auth->mysql, auth->q)) {
        log_warn("auth_auth query error: %s", mysql_error(&auth->mysql));
        goto nocleanup;
    }
    result = mysql_store_result(&auth->mysql);
    if (result == NULL) {
        log_warn("auth failed: %s", p->username);
        goto nocleanup;
    }
    int num_fields = mysql_num_fields(result);
    if (num_fields < 2) {
        log_warn("auth failed: %s, %d fields", p->username, num_fields);
        goto cleanup;
    }
    if ((row = mysql_fetch_row(result))) {
        if (row[0] && strcmp(row[0], (char *)password) == 0) {
            p->is_super = (strcmp(row[1], "1") == 0);
            strcpy(p->user_id_str, row[2]);
            rv = 0;
            goto cleanup;
        }
        else {
            log_warn("auth failed %s", p->username);
        }
    }
    else if (device_0_flag) {
        log_warn("%s: provisioning device from unauthorized ip address %s", config.name, p->ip_address);
        rv = 0;
    }
cleanup:
    mysql_free_result(result);
    while (mysql_next_result(&auth->mysql) == 0)
        ;
nocleanup:
    LOG_DEBUG("auth_auth %s %s rv %d", p->username, password, rv);
    return rv;
}

/** return 0 if this authenticated user is superuser */
int auth_super(void *authp, client_st *p)
{
    auth_st *auth = authp;
    return !p->is_super;
}

/** return 0 if a subscription or publish is allowed */
int auth_acl(void *authp, client_st *p,
        auth_access_et access, const unsigned char *filter, int filter_len)
{
    auth_st *auth = authp;
    int rv, srv;
    MYSQL_RES *result;
    MYSQL_ROW row;

    LOG_DEBUG("auth_acl sd %d %d %.*s", p->sd, access, filter_len, filter);

    if (p->is_super) {
        return 0;
    }

    /* Pub ACL caching */
    if (access == AUTH_PUB) {
        if (p->auth_info == NULL) {
            p->auth_info = set_create();
        }
        else {
            if (set_exists(p->auth_info, (char *)filter)) {
                return 0;
            }
        }
    }

    rv = 1;
    if (p->username[0] == 'D') {
        /* authorize device */

        if (access == AUTH_SUB) {
            snprintf((char *)auth->topic, sizeof(auth->topic), "flameboss/%s/recv", &p->username[2]);
        }
        else {
            snprintf((char *)auth->topic, sizeof(auth->topic), "flameboss/%s/send/+", &p->username[2]);
        }
        if (mqtt_topic_matches(auth->topic, filter, filter_len)) {
            rv = 0;
        }
        return rv;
    }
    else {
        char q[MAX_QUERY_LEN];
        /* authorize user */

        /* authorize public topics */

        if (access == AUTH_PUB) {
            if (mqtt_topic_matches((unsigned char *)"flameboss/relay/+", filter, filter_len)) {
                rv = 0;
                goto nocleanup;
            }
        }
        else {
            if (mqtt_topic_matches((unsigned char *)"flameboss/+/send/time", filter, filter_len)) {
                rv = 0;
                goto nocleanup;
            }
            if (mqtt_topic_matches((unsigned char *)"flameboss/+/send/open", filter, filter_len)) {
                rv = 0;
                goto nocleanup;
            }
        }

        srv = snprintf((char *)q, MAX_QUERY_LEN, "SELECT device_id FROM devices_users WHERE user_id = %s", p->user_id_str);
        if (srv == 0 || srv >= MAX_QUERY_LEN) {
            goto nocleanup;
        }
        LOG_DEBUG("%s", q);
        if (mysql_query(&auth->mysql, q)) {
            log_warn("auth_acl query error: %s", mysql_error(&auth->mysql));
            goto nocleanup;
        }
        result = mysql_store_result(&auth->mysql);
        if (result == NULL) {
            goto nocleanup;
        }
        int num_fields = mysql_num_fields(result);
        if (num_fields != 1) {
            goto cleanup;
        }
        char access_str[2];
        sprintf(access_str, "%d", access);
        while ((row = mysql_fetch_row(result))) {
            if (access == AUTH_SUB) {
                srv = snprintf((char *)auth->topic, MAX_TOPIC_LEN, "flameboss/%s/send/+", row[0]);
            }
            else {
                srv = snprintf((char *)auth->topic, MAX_TOPIC_LEN, "flameboss/%s/recv", row[0]);
            }
            LOG_DEBUG("auth_acl sub %s ? %.*s", auth->topic, filter_len, filter);
            if (mqtt_topic_matches(auth->topic, filter, filter_len)) {
                rv = 0;
                goto cleanup;
            }
            if (access == AUTH_SUB) {
                srv = snprintf((char *)auth->topic, MAX_TOPIC_LEN, "flameboss/relay/%s/recv", row[0]);
                LOG_DEBUG("auth_acl sub %s ? %.*s", auth->topic, filter_len, filter);
                if (mqtt_topic_matches(auth->topic, filter, filter_len)) {
                    rv = 0;
                    goto cleanup;
                }
                srv = snprintf((char *)auth->topic, MAX_TOPIC_LEN, "flameboss/mqttr/%s/send", row[0]);
                LOG_DEBUG("auth_acl sub %s ? %.*s", auth->topic, filter_len, filter);
                if (mqtt_topic_matches(auth->topic, filter, filter_len)) {
                    rv = 0;
                    goto cleanup;
                }
            }
            else {
                srv = snprintf((char *)auth->topic, MAX_TOPIC_LEN, "flameboss/%s/recv", row[0]);
                LOG_DEBUG("auth_acl sub %s ? %.*s", auth->topic, filter_len, filter);
                if (mqtt_topic_matches(auth->topic, filter, filter_len)) {
                    rv = 0;
                    goto cleanup;
                }
                srv = snprintf((char *)auth->topic, MAX_TOPIC_LEN, "flameboss/mqttr/%s/recv", row[0]);
                LOG_DEBUG("auth_acl sub %s ? %.*s", auth->topic, filter_len, filter);
                if (mqtt_topic_matches(auth->topic, filter, filter_len)) {
                    rv = 0;
                    goto cleanup;
                }
            }
        }
    cleanup:
        mysql_free_result(result);
        while (mysql_next_result(&auth->mysql) == 0)
            ;
    nocleanup:
        return rv;
    }
}

void auth_client_destroy(client_st *p)
{
    if (p->auth_info) {
        set_destroy(p->auth_info);
    }
}
