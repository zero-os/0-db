#include <stdio.h>
#include <string.h>
#include "tests_user.h"
#include "tests.h"
#include "zdb_utils.h"

runtest_prio(101, simple_ping) {
    redisReply *reply;

    if(!(reply = redisCommand(test->zdb, "PING")))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(strcmp(reply->str, "PONG"))
        return zdb_result(reply, TEST_FAILED);

    return zdb_result(reply, TEST_SUCCESS);
}


runtest_prio(102, default_set_hello) {
    return zdb_set(test, "hello", "world");
}

runtest_prio(103, default_get_hello) {
    return zdb_check(test, "hello", "world");
}


static int overwrite(test_t *test, char *key, char *original, char *newvalue) {
    redisReply *reply;

    // first set
    if(!(reply = redisCommand(test->zdb, "SET %s %s", key, original)))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(strcmp(reply->str, key))
        return zdb_result(reply, TEST_FAILED_FATAL);

    // overwrite
    if(!(reply = redisCommand(test->zdb, "SET %s %s", key, newvalue)))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(strcmp(reply->str, key))
        return zdb_result(reply, TEST_FAILED_FATAL);

    // check value
    if(!(reply = redisCommand(test->zdb, "GET %s", key)))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(strcmp(reply->str, newvalue))
        return zdb_result(reply, TEST_FAILED_FATAL);

    return zdb_result(reply, TEST_SUCCESS);

}

runtest_prio(104, simple_overwrite_same_length) {
    return overwrite(test, "overwrite_normal", "original", "newvalue");
}

runtest_prio(104, simple_overwrite_shorter) {
    return overwrite(test, "overwrite_shorter", "original", "new");
}

runtest_prio(104, simple_overwrite_longer) {
    return overwrite(test, "overwrite_longer", "original", "newvaluelonger");
}