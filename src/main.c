
/*
 * Parse arguments and call test_run or server
 */

#include <common.h>
#include <test/test.h>
#include <execinfo.h>


static int run_unit_tests;


static void exception_signal_handler(int sig);


static void usage()
{
    fprintf(stderr, "usage: %s [-t (run unit tests)] (see broker.cfg)\n", config.name);
    exit(1);
}

int main(int argc, char *argv[])
{
    int opt;

    signal(SIGABRT, exception_signal_handler);
    signal(SIGALRM, exception_signal_handler);
    signal(SIGFPE, exception_signal_handler);
    signal(SIGHUP, exception_signal_handler);
    signal(SIGILL, exception_signal_handler);
    signal(SIGINT, exception_signal_handler);
    signal(SIGKILL, exception_signal_handler);
    signal(SIGPIPE, exception_signal_handler);
    signal(SIGQUIT, exception_signal_handler);
    signal(SIGSEGV, exception_signal_handler);
    signal(SIGTERM, exception_signal_handler);
    signal(SIGUSR2, exception_signal_handler);

    fb_config_init();

    while ((opt = getopt(argc, argv, "t")) != -1) {
        switch (opt) {
        case 't':
            run_unit_tests = 1;
            break;
        default:
            usage();
            break;
        }
    }
    if (config.num_servers == 0) {
        config.num_servers = 1;
    }

    fb_config_file();

    if (run_unit_tests) {
        exit(test_run());
    }

    servers_init();

    servers_run();
}

static __thread void *array[64];

static void exception_signal_handler(int sig)
{
    size_t size;

    size = backtrace(array, 64);

    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}
