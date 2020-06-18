
#ifndef DEBUG_H_
#define DEBUG_H_

#ifndef DEBUG
#define DEBUG 0
#endif

#include <log.h>
#include <config.h>

#if CONFIG_ASSERTIONS

#define ASSERT(cond) \
        do { \
            if (! (cond)) { \
                debug_assert_failure(__func__, __FILE__, __LINE__, #cond); \
            } \
        } while (0)

#else


#define ASSERT(cond) \
        do { \
        } while (0)

#endif

void debug_assert_failure(const char *func, const char *file, int line, const char *cond);

#endif  /* DEBUG_H_ */
