#include <test/test.h>
#include <common.h>


void test_main(void);
void test_subscriber(void);
void test_keep_alive(void);
void test_queue(void);


int test_run()
{
    test_main();
    test_queue();
    test_keep_alive();
    test_subscriber();
    return 0;
}
