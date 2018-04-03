#include <stdio.h>
#include <string.h>
#include "tests_user.h"
#include "zdb_utils.h"
#include "tests.h"

int zdb_result(redisReply *reply, int value) {
    if(reply)
        freeReplyObject(reply);

    return value;
}

// send command which should not fails
int zdb_command(test_t *test, int argc, const char *argv[]) {
    redisReply *reply;

    if(!(reply = redisCommandArgv(test->zdb, argc, argv, NULL)))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(reply->type != REDIS_REPLY_STATUS) {
        log("%s\n", reply->str);
        return zdb_result(reply, TEST_FAILED);
    }

    return zdb_result(reply, TEST_SUCCESS);
}

// send command which should fails
int zdb_command_error(test_t *test, int argc, const char *argv[]) {
    redisReply *reply;

    if(!(reply = redisCommandArgv(test->zdb, argc, argv, NULL)))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(reply->type != REDIS_REPLY_ERROR && reply->type != REDIS_REPLY_NIL) {
        log("%s\n", reply->str);
        return zdb_result(reply, TEST_FAILED);
    }

    return zdb_result(reply, TEST_SUCCESS);
}


int zdb_set(test_t *test, char *key, char *value) {
    redisReply *reply;

    if(!(reply = redisCommand(test->zdb, "SET %s %s", key, value)))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(reply->type != REDIS_REPLY_STRING) {
        log("%s\n", reply->str);
        return zdb_result(reply, TEST_FAILED_FATAL);
    }

    if(strcmp(reply->str, key)) {
        log("%s\n", reply->str);
        return zdb_result(reply, TEST_FAILED_FATAL);
    }

    return zdb_result(reply, TEST_SUCCESS);

}

int zdb_check(test_t *test, char *key, char *value) {
    redisReply *reply;

    if(!(reply = redisCommand(test->zdb, "GET %s", key)))
        return zdb_result(reply, TEST_FAILED_FATAL);

    if(reply->type != REDIS_REPLY_STRING) {
        log("%s\n", reply->str);
        return zdb_result(reply, TEST_FAILED);
    }

    if(strcmp(reply->str, value)) {
        log("%s\n", reply->str);
        return zdb_result(reply, TEST_FAILED_FATAL);
    }

    return zdb_result(reply, TEST_SUCCESS);
}
