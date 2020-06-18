#include <common.h>

static queue_st q;
static queue_item_st items[100];

void test_queue()
{
    int i;

    for (i = 0; i < 100; ++i) {
        items[i].p = (void *)(intptr_t)i;
    }
    queue_init(&q, "test");
    queue_push(&q, &items[0]);

    assert(queue_pop(&q) == &items[0]);
    assert(queue_pop(&q) == NULL);

    for (i = 0; i < 100; ++i) {
        queue_push(&q, &items[i]);
    }

    for (i = 0; i < 100; ++i) {
        assert(queue_pop(&q) == &items[i]);
    }

    assert(queue_pop(&q) == NULL);
}
