
#include <debug.h>
#include <common.h>


log_level_et log_level = LOG_INFO;
int log_time;
static FILE *fp;
static __thread char _msg[1024];


void log_init(const char *file_name, log_level_et level, int log_time_config)
{
    if (file_name == NULL) {
        fp = stdout;
    }
    else {
        fp = fopen(file_name, "w+");
    }
    log_level = level;
    log_time = log_time_config;
}

static void log_format(char *tag, const char* message, va_list args)
{
    int rv;
    FILE *my_fp = fp;
    if (fp == NULL) {
        my_fp = stdout;
    }
    rv = vsnprintf(_msg, sizeof(_msg), message, args);
    if (log_time) {
        time_t now;
        char date_str[32];
        time(&now);
        strftime(date_str, sizeof date_str, "%FT%TZ", gmtime(&now));
        fprintf(my_fp, "%s [%s] %s\n", date_str, tag, _msg);
    }
    else {
        fprintf(my_fp, "[%s] %s\n", tag, _msg);
    }
}

void log_error(const char* message, ...)
{
    va_list args;
    va_start(args, message);
    log_format("error", message, args);
    va_end(args);
}

void log_warn(const char* message, ...)
{
    va_list args;
    if (log_level <= LOG_WARN) {
        va_start(args, message);
        log_format("warn", message, args);
        va_end(args);
    }
}

void log_info(const char* message, ...)
{
    va_list args;
    if (log_level <= LOG_INFO) {
        va_start(args, message);
        log_format("info", message, args);
        va_end(args);
    }
}

void log_debug(const char* message, ...)
{
    va_list args;
    if (log_level == LOG_DEBUG) {
        va_start(args, message);
        log_format("debug", message, args);
        va_end(args);
    }
}
