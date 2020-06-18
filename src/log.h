/*
 * DO NOT INCLUDE THIS FILE - INCLUDE debug.h INSTEAD
 * And #define DEBUG 1 before including debug.h to compile in debug log statements for the file
 */

#ifndef LOG_H_
#define LOG_H_


typedef enum {
    /** show LOG_DEBUG messages that are enabled at build time */
    LOG_DEBUG,

    /** show normal activity */
    LOG_INFO,

    /** show activity about any connections having abnormal behavior */
    LOG_WARN,

    /** show system errors */
    LOG_ERROR
} log_level_et;

#if DEBUG

#define LOG_DEBUG(...)    \
    do { \
        if (log_level == LOG_DEBUG) { \
            log_debug(__VA_ARGS__); \
        } \
    } while (0)

#else

#define LOG_DEBUG(...)  \
        do { \
        } while (0)

#endif


extern log_level_et log_level;


void log_init(const char *file_name, log_level_et level, int log_times);

void log_error(const char* message, ...);

void log_warn(const char* message, ...);

void log_info(const char* message, ...);

void log_debug(const char* message, ...);


#endif  /* LOG_H_ */
