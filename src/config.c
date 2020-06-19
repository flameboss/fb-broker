
/*
 * Reads broker.cfg file and sets config
 */

#include <config.h>
#include <common.h>

//#define DEBUG 1
#include <debug.h>


config_st config;


void fb_config_init()
{
    config.config_file_path = CONFIG_FILE_PATH;
    config.name = "fb-broker";
    config.num_workers = DEFAULT_NUM_WORKERS;
    config.num_servers = 1;
    config.servers[0].port = 1883;
    config.servers[0].is_tls = 0;

    config_init(&config.lc_config);
}

void fb_config_file(char *config_file, char *pid_file)
{
    config_setting_t *setting;
    const char *log_file_name;
    int log_time;
    const char *log_level_str;
    log_level_et level;

    /* XXX make pid_file configurable in config file? */
    (void)pid_file;

    if (!config_read_file(&config.lc_config, config_file)) {
        log_error("%s:%d - %s", config_error_file(&config.lc_config),
                config_error_line(&config.lc_config),
                config_error_text(&config.lc_config));
        config_destroy(&config.lc_config);
        exit(1);
    }

    config_lookup_string(&config.lc_config, "log_file", &log_file_name);
    config_lookup_bool(&config.lc_config, "log_time", &log_time);

    level = LOG_INFO;
    config_lookup_string(&config.lc_config, "log_level", &log_level_str);
    if (log_level_str != NULL) {
        if (strcmp(log_level_str, "error") == 0) {
            level = LOG_ERROR;
        }
        else if (strcmp(log_level_str, "warn") == 0) {
            level = LOG_WARN;
        }
        else if (strcmp(log_level_str, "debug") == 0) {
            level = LOG_DEBUG;
        }
    }
    log_init(log_file_name, level, log_time);
    LOG_DEBUG("config: log_init %s, %d, %d", log_file_name, level, log_time);

    config.allow_anonymous = 0;
    config_lookup_bool(&config.lc_config, "allow_anonymous", &config.allow_anonymous);
    LOG_DEBUG("config: allow_anonymous %d", config.allow_anonymous);

    config_lookup_int(&config.lc_config, "workers", &config.num_workers);
    LOG_DEBUG("config: num_workers %d", config.num_workers);

    config_lookup_string(&config.lc_config, "tls_file_key", &config.tls_file_key);
    config_lookup_string(&config.lc_config, "tls_file_key", &config.tls_file_crt);

    setting = config_lookup(&config.lc_config, "servers");
    if (setting && config_setting_is_list(setting)) {
        config.num_servers = config_setting_length(setting);
        for (unsigned int i = 0; i < config.num_servers; ++i) {
            config_setting_t *server_setting;
            server_setting = config_setting_get_elem(setting, i);

            config.servers[i].port = 1883;
            config.servers[i].is_tls = 0;
            config.servers[i].is_ws = 0;
            config.servers[i].backlog = 100;

            config_setting_lookup_int(server_setting, "port", &config.servers[i].port);
            config_setting_lookup_int(server_setting, "backlog", &config.servers[i].backlog);
            config_setting_lookup_bool(server_setting, "tls", &config.servers[i].is_tls);
            config_setting_lookup_bool(server_setting, "ws", &config.servers[i].is_ws);

            LOG_DEBUG("server %d, port %d, tls %d, ws %d, backlog %d",
                    i, config.servers[i].port, config.servers[i].is_tls,
                    config.servers[i].is_ws, config.servers[i].backlog);
        }
    }
}
