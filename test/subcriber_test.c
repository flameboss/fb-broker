
#include <common.h>
#include <debug.h>

static int found_count;
static client_st clients[11];
static subscription_st subs[11] = {
        { .filter = (uint8_t *)"#", .qos = 0, .client = &clients[0] },
        { .filter = (uint8_t *)"+", .qos = 0, .client = &clients[1] },
        { .filter = (uint8_t *)"flameboss", .qos = 0, .client = &clients[2] },
        { .filter = (uint8_t *)"flameboss/#", .qos = 0, .client = &clients[3] },
        { .filter = (uint8_t *)"flameboss/+/recv", .qos = 0, .client = &clients[4] },
        { .filter = (uint8_t *)"flameboss/+/send/+", .qos = 0, .client = &clients[5] },
        { .filter = (uint8_t *)"flameboss/12345678/send/open", .qos = 0, .client = &clients[6] },
        { .filter = (uint8_t *)"$local/flameboss/+/send/time", .qos = 0, .client = &clients[7] },
        { .filter = (uint8_t *)"flameboss/12345678/send/data", .qos = 0, .client = &clients[8] },
        { .filter = (uint8_t *)"flameboss/12345678/send/+", .qos = 0, .client = &clients[9] },
        { .filter = (uint8_t *)"flameboss/12345678/+", .qos = 0, .client = &clients[10] }
};

static message_st message = {
        .topic = (uint8_t *)"dummy",
        .topic_len = 5,
};


static void found(subscription_st *sp, message_st *mp)
{
    int i;
    i = sp - &subs[0];
    log_info("found %d %s %s", i, sp->filter, mp->topic);
}

void test_subscriber()
{
    trie_mult_node_st *  head = NULL;

    for (int i = 0; i < 11; ++i) {
        sub_insert(&head, &subs[i]);
    }

    message.topic = (uint8_t *)"blah";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 2);
    sub_remove(&head, &subs[0]);

    message.topic = (uint8_t *)"blah";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 1);

    message.topic = (uint8_t *)"flameboss";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 2);

    message.topic = (uint8_t *)"flameboss/hut/recv";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 2);

    message.topic = (uint8_t *)"flameboss/12345678/recv";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 3);

    message.topic = (uint8_t *)"flameboss/12345678/send";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 2);

    message.topic = (uint8_t *)"flameboss/12345678/send/data";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 4);

    message.topic = (uint8_t *)"flameboss/12345678/send/time";
    sub_search(head, &message, &found_count, found);
    ASSERT(found_count == 4);
}
