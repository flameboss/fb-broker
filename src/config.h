
#ifndef CONFIG_H_
#define CONFIG_H_

/*
 * Compile time configuration
 */

#ifndef CONFIG_ASSERTIONS
#define CONFIG_ASSERTIONS   1
#endif

#define MAX_TOPIC_LEN           127
#define MAX_NUM_WORKERS         64
#define DEFAULT_NUM_WORKERS     2
#define MAX_NUM_SERVERS         4
#define CONFIG_FILE_PATH        "/etc/flameboss/broker.conf"

#include <server.h>
#include <libconfig.h>

typedef struct {
    int allow_anonymous;
    const char *tls_file_key;
    const char *tls_file_crt;
    char *name;
    int num_workers;
    unsigned int num_servers;
    server_st servers[MAX_NUM_SERVERS];
    char *config_file_path;
    config_t lc_config;
} config_st;


extern config_st config;


void fb_config_init(void);
void fb_config_file(char *config_file, char *pid_file);

#endif /* CONFIG_H_ */
