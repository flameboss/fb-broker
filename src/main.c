
/*
 * Parse arguments and call test_run or server
 */

#include <common.h>
#include <test/test.h>
#include <execinfo.h>
#include <log.h>


static char **main_argv;
static int run_unit_tests;
char *pid_file;


static void exception_signal_handler(int sig);
static void interrupt_signal_handler(int sig);
static void die(void);


static void usage()
{
    fprintf(stderr, "usage: %s [-c config-file] [-p pid-file] [-t]\n", main_argv[0]);
    die();
}

static void write_pid(char *pid_file)
{
    size_t len;
    size_t urv;
    int rv;
    int fd;
    char s[32];
    fd = open(pid_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        log_error("could not open pid file %s", pid_file);
        return;
    }
    len = snprintf(s, sizeof(s), "%d\n", getpid());
    if (len >= sizeof(s) - 1) {
        log_error("snprintf buffer overflow");
        return;
    }
    urv = write(fd, s, len);
    if (urv != len) {
        log_error("write_pid write failed urv %d, len %d", urv, len);
        return;
    }
    rv = close(fd);
    if (rv < 0) {
        log_error("write_pid close failed");
    }
}

int main(int argc, char *argv[])
{
    int opt;
    char *config_file = "broker.cfg";

    main_argv = argv;

    signal(SIGABRT, exception_signal_handler);
    signal(SIGALRM, exception_signal_handler);
    signal(SIGFPE, exception_signal_handler);
    signal(SIGHUP, exception_signal_handler);
    signal(SIGILL, exception_signal_handler);
    signal(SIGINT, interrupt_signal_handler);
    signal(SIGKILL, exception_signal_handler);
    signal(SIGPIPE, exception_signal_handler);
    signal(SIGQUIT, exception_signal_handler);
    signal(SIGSEGV, exception_signal_handler);
    signal(SIGTERM, exception_signal_handler);
    signal(SIGUSR2, exception_signal_handler);

    fb_config_init();

    while ((opt = getopt(argc, argv, "c:tp:")) != -1) {
        switch (opt) {
        case 'c':
            config_file = optarg;
            break;
        case 't':
            run_unit_tests = 1;
            break;
        case 'p':
            pid_file = optarg;
            break;
        default:
            usage();
            break;
        }
    }
    if (config.num_servers == 0) {
        config.num_servers = 1;
    }

    fb_config_file(config_file, pid_file);

    write_pid(pid_file);

    if (run_unit_tests) {
        exit(test_run());
    }

    servers_init();

    servers_run();
}

static __thread void *array[64];

static void die()
{
    int rv;
    if (pid_file) {
        log_debug("removing %s", pid_file);
        rv = unlink(pid_file);
        if (rv < 0) {
            log_error("could not remove %s, rv %d, errno %d", pid_file, rv, errno);
        }
    }
    log_debug("exiting");
    exit(1);
}

static void exception_signal_handler(int sig)
{
    size_t size;

    size = backtrace(array, 64);

    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    die();
}

static void interrupt_signal_handler(int sig)
{
    (void)sig;
    die();
}
