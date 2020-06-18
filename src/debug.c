
#include <common.h>
#include <debug.h>

void debug_assert_failure(const char *func, const char *file, int line, const char *cond)
{
    log_error("Assertion failed: (%s), function %s, %s:%d", cond, func, file, line);

    /* breakpoint */
    __asm("int $3");

    exit(1);
}
