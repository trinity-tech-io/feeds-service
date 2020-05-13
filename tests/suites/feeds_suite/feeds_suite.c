/*
 * Copyright (c) 2020 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <limits.h>
#include <unistd.h>

#include <CUnit/Basic.h>
#include <ela_carrier.h>
#include <crystal.h>
#include <cfg.h>

#include "../case.h"
#include "../../../src/jsonrpc.h"
#include "../../../src/carrier_cfg.h"
#include "../../../src/feeds_client/feeds_client.h"

static FeedsClient *publisher;
static FeedsClient *subscriber;
static SOCKET msg_sock = INVALID_SOCKET;
static char robot_address[ELA_MAX_ADDRESS_LEN + 1];
static char robot_node_id[ELA_MAX_ID_LEN + 1];
static char *topic_name = "topic";
static char topic_desc[ELA_MAX_APP_MESSAGE_LEN * 2 + 1];
static char *non_existent_topic_name = "non-existant-topic";

static
void create_topic_without_permission(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_create_channel(subscriber, robot_node_id,
                                     topic_name, topic_desc, &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of creating topic without permission");

    if (resp)
        cJSON_Delete(resp);
}

static
void create_topic(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_create_channel(publisher, robot_node_id,
                                     topic_name, topic_desc, &resp);
    if (rc < 0)
        CU_FAIL("wrong result of creating topic");

    if (resp)
        cJSON_Delete(resp);
}

static
void create_existing_topic(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_create_channel(publisher, robot_node_id,
                                     topic_name, topic_desc, &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of creating existing topic");

    if (resp)
        cJSON_Delete(resp);
}

static
void post_event_without_permission(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_publish_post(subscriber, robot_node_id,
                                   topic_name, "event1", &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of posting event without permission");

    if (resp)
        cJSON_Delete(resp);
}

static
void post_event_on_non_existent_topic(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_publish_post(publisher, robot_node_id,
                                   non_existent_topic_name, "event1", &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of posting non-existent event");

    if (resp)
        cJSON_Delete(resp);
}

static
void post_event_when_no_subscribers(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_publish_post(publisher, robot_node_id,
                                   topic_name, "event1", &resp);
    if (rc < 0) {
        CU_FAIL("wrong result of posting event when there is no subscribers");
        goto finally;
    }
    cJSON_Delete(resp);

    sleep(2);
    resp = feeds_client_get_new_posts(subscriber);
    if (resp)
        CU_FAIL("got new event(s) when there should not be");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void post_event_when_subscriber_exists(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_publish_post(publisher, robot_node_id,
                                   topic_name, "event2", &resp);
    if (rc < 0) {
        CU_FAIL("wrong result of posting event when only subscriber exists");
        goto finally;
    }
    cJSON_Delete(resp);

    sleep(2);
    resp = feeds_client_get_new_posts(subscriber);
    if (resp)
        CU_FAIL("got new event(s) when there should not be");

finally:
    if (resp)
        cJSON_Delete(resp);

}

static
void post_event_when_active_subscriber_exists(void)
{
    cJSON *event;
    cJSON *resp;
    int rc;

    rc = feeds_client_publish_post(publisher, robot_node_id,
                                   topic_name, "event3", &resp);
    if (rc < 0) {
        CU_FAIL("wrong result of posting event when there is an active subscriber");
        goto finally;
    }
    cJSON_Delete(resp);

    sleep(2);
    resp = feeds_client_get_new_posts(subscriber);
    if (!resp || cJSON_GetArraySize(resp) != 1) {
        CU_FAIL("received incorrect number of new events");
        goto finally;
    }

    event = cJSON_GetArrayItem(resp, 0);
    if (strcmp(cJSON_GetObjectItemCaseSensitive(event, "topic")->valuestring, topic_name) ||
        strcmp(cJSON_GetObjectItemCaseSensitive(event, "event")->valuestring, "event3") ||
        (size_t)cJSON_GetObjectItemCaseSensitive(event, "seqno")->valuedouble != 3)
        CU_FAIL("wrong event info");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void list_owned_topics_when_no_topics(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_get_my_channels(publisher, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp))) {
        CU_FAIL("wrong result of listing owned topics when there is no topics");
        goto finally;
    }
    cJSON_Delete(resp);


    rc = feeds_client_get_my_channels(subscriber, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp)))
        CU_FAIL("wrong result of listing owned topics when there is no topics");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void list_owned_topics_when_topic_exists(void)
{
    const cJSON *res;
    cJSON *topic;
    cJSON *name;
    cJSON *desc;
    cJSON *resp;
    int rc;

    rc = feeds_client_get_my_channels(publisher, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(res = jsonrpc_get_result(resp)) != 1) {
        CU_FAIL("wrong result of listing owned topics");
        goto finally;
    }

    topic = cJSON_GetArrayItem(res, 0);
    name = cJSON_GetObjectItemCaseSensitive(topic, "name");
    desc = cJSON_GetObjectItemCaseSensitive(topic, "desc");
    if (strcmp(name->valuestring, topic_name) || strcmp(desc->valuestring, topic_desc)) {
        CU_FAIL("wrong list owned topics response");
        goto finally;
    }
    cJSON_Delete(resp);

    rc = feeds_client_get_my_channels(subscriber, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp)))
        CU_FAIL("wrong result of listing owned topics");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void list_owned_topics_batch_when_topic_exists(void)
{
    JsonRPCBatchInfo *bi;
    JsonRPCType type;
    const cJSON *res;
    cJSON *topic;
    cJSON *name;
    cJSON *desc;
    cJSON *resp;
    cJSON *req1;
    cJSON *req2;
    cJSON *batch;
    char *batch_str;
    int rc;

    req1 = jsonrpc_encode_request("get_my_channels", NULL, NULL);
    if (!req1) {
        CU_FAIL("can not create request");
        return;
    }

    req2 = jsonrpc_encode_request("get_my_channels", NULL, NULL);
    if (!req2) {
        CU_FAIL("can not create request");
        cJSON_Delete(req1);
        return;
    }

    batch = jsonrpc_encode_empty_batch();
    if (!batch) {
        CU_FAIL("can not create batch");
        cJSON_Delete(req1);
        cJSON_Delete(req2);
        return;
    }

    jsonrpc_add_object_to_batch(batch, req1);
    jsonrpc_add_object_to_batch(batch, req2);
    batch_str = cJSON_PrintUnformatted(batch);
    cJSON_Delete(batch);
    if (!batch_str) {
        CU_FAIL("can not create batch string");
        return;
    }

    rc = transaction_start(publisher, robot_node_id, batch_str, strlen(batch_str) + 1,
                           &resp, &type, &bi);
    free(batch_str);
    if (rc < 0) {
        CU_FAIL("send request failure");
        return;
    }

    if (type != JSONRPC_TYPE_BATCH || bi->nobjs != 2 ||
        bi->obj_types[0] != JSONRPC_TYPE_SUCCESS_RESPONSE ||
        bi->obj_types[1] != JSONRPC_TYPE_SUCCESS_RESPONSE) {
        CU_FAIL("invalid response");
        goto finally;
    }

    res = jsonrpc_batch_get_object(resp, 0);
    topic = cJSON_GetArrayItem(cJSON_GetObjectItem(res, "result"), 0);
    name = cJSON_GetObjectItemCaseSensitive(topic, "name");
    desc = cJSON_GetObjectItemCaseSensitive(topic, "desc");
    if (strcmp(name->valuestring, topic_name) || strcmp(desc->valuestring, topic_desc)) {
        CU_FAIL("wrong list owned topics response");
        goto finally;
    }

    res = jsonrpc_batch_get_object(resp, 1);
    topic = cJSON_GetArrayItem(cJSON_GetObjectItem(res, "result"), 0);
    name = cJSON_GetObjectItemCaseSensitive(topic, "name");
    desc = cJSON_GetObjectItemCaseSensitive(topic, "desc");
    if (strcmp(name->valuestring, topic_name) || strcmp(desc->valuestring, topic_desc))
        CU_FAIL("wrong list owned topics response");

finally:
    if (resp)
        cJSON_Delete(resp);
    if (bi)
        free(bi);
}

static
void subscribe_non_existent_topic(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_subscribe_channel(subscriber, robot_node_id, non_existent_topic_name, &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of subscribing non-existent topic");

    if (resp)
        cJSON_Delete(resp);
}

static
void subscribe(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_subscribe_channel(subscriber, robot_node_id, topic_name, &resp);
    if (rc < 0)
        CU_FAIL("wrong result of subscribing topic");

    if (resp)
        cJSON_Delete(resp);
}

static
void subscribe_when_already_subscribed(void)
{
    subscribe();
}

static
void unsubscribe_non_existent_topic(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_unsubscribe_channel(subscriber, robot_node_id, non_existent_topic_name, &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of unsubscribing non-existent topic");

    if (resp)
        cJSON_Delete(resp);
}

static
void unsubscribe(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_unsubscribe_channel(subscriber, robot_node_id, topic_name, &resp);
    if (rc < 0)
        CU_FAIL("wrong result of unsubscribing topic");

    if (resp)
        cJSON_Delete(resp);
}

static
void unsubscribe_without_subscription(void)
{
    unsubscribe();
}

static
void explore_topics_when_no_topics(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_explore_topics(publisher, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp))) {
        CU_FAIL("wrong result of exploring topics when there is no topic");
        goto finally;
    }
    cJSON_Delete(resp);


    rc = feeds_client_explore_topics(subscriber, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp)))
        CU_FAIL("wrong result of exploring topics when there is no topic");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void explore_topics_when_topic_exists(void)
{
    const cJSON *res;
    cJSON *topic;
    cJSON *name;
    cJSON *desc;
    cJSON *resp;
    int rc;

    rc = feeds_client_explore_topics(publisher, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(res = jsonrpc_get_result(resp)) != 1) {
        CU_FAIL("wrong result of exploring topics");
        goto finally;
    }

    topic = cJSON_GetArrayItem(res, 0);
    name = cJSON_GetObjectItemCaseSensitive(topic, "name");
    desc = cJSON_GetObjectItemCaseSensitive(topic, "desc");
    if (strcmp(name->valuestring, topic_name) || strcmp(desc->valuestring, topic_desc)) {
        CU_FAIL("wrong topic info");
        goto finally;
    }
    cJSON_Delete(resp);

    rc = feeds_client_explore_topics(subscriber, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(res = jsonrpc_get_result(resp)) != 1) {
        CU_FAIL("wrong result of exploring topics");
        goto finally;
    }

    topic = cJSON_GetArrayItem(res, 0);
    name = cJSON_GetObjectItemCaseSensitive(topic, "name");
    desc = cJSON_GetObjectItemCaseSensitive(topic, "desc");
    if (strcmp(name->valuestring, topic_name) || strcmp(desc->valuestring, topic_desc))
        CU_FAIL("wrong topic info");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void list_subscribed_topics_before_subscription(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_list_subscribed(publisher, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp))) {
        CU_FAIL("wrong result of listing subscribed topics");
        goto finally;
    }
    cJSON_Delete(resp);


    rc = feeds_client_list_subscribed(subscriber, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp)))
        CU_FAIL("wrong result of listing subscribed topics");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void list_subscribed_topics_after_subscription(void)
{
    const cJSON *res;
    cJSON *topic;
    cJSON *name;
    cJSON *desc;
    cJSON *resp;
    int rc;

    rc = feeds_client_list_subscribed(publisher, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(jsonrpc_get_result(resp))) {
        CU_FAIL("wrong result of listing subscribed topics");
        goto finally;
    }
    cJSON_Delete(resp);

    rc = feeds_client_list_subscribed(subscriber, robot_node_id, &resp);
    if (rc < 0 || cJSON_GetArraySize(res = jsonrpc_get_result(resp)) != 1) {
        CU_FAIL("wrong result of listing subscribed topics");
        goto finally;
    }

    topic = cJSON_GetArrayItem(res, 0);
    name = cJSON_GetObjectItemCaseSensitive(topic, "name");
    desc = cJSON_GetObjectItemCaseSensitive(topic, "desc");
    if (strcmp(name->valuestring, topic_name) || strcmp(desc->valuestring, topic_desc))
        CU_FAIL("wrong list owned topics response");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void fetch_unreceived_without_subscription(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_fetch_unreceived(subscriber, robot_node_id, topic_name, 1, &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of fetching unreceived without subscription");

    if (resp)
        cJSON_Delete(resp);
}

static
void fetch_unreceived(void)
{
    const cJSON *res;
    cJSON *event;
    cJSON *resp;
    int rc;

    rc = feeds_client_fetch_unreceived(subscriber, robot_node_id, topic_name, 1, &resp);
    if (rc < 0 || cJSON_GetArraySize(res = jsonrpc_get_result(resp)) != 2)
        CU_FAIL("wrong result of fetching unreceived");

    event = cJSON_GetArrayItem(res, 0);
    if (strcmp(cJSON_GetObjectItemCaseSensitive(event, "event")->valuestring, "event1") ||
        (size_t)cJSON_GetObjectItemCaseSensitive(event, "seqno")->valuedouble != 1) {
        CU_FAIL("wrong event info");
        goto finally;
    }

    event = cJSON_GetArrayItem(res, 1);
    if (strcmp(cJSON_GetObjectItemCaseSensitive(event, "event")->valuestring, "event2") ||
        (size_t)cJSON_GetObjectItemCaseSensitive(event, "seqno")->valuedouble != 2)
        CU_FAIL("wrong event info");

finally:
    if (resp)
        cJSON_Delete(resp);
}

static
void fetch_unreceived_when_already_active(void)
{
    cJSON *resp;
    int rc;

    rc = feeds_client_fetch_unreceived(subscriber, robot_node_id, topic_name, 1, &resp);
    if (!rc || !resp)
        CU_FAIL("wrong result of fetching unreceived");

    if (resp)
        cJSON_Delete(resp);
}

DECL_TESTCASE(create_topic_without_permission)
DECL_TESTCASE(create_topic)
DECL_TESTCASE(create_existing_topic)
DECL_TESTCASE(post_event_without_permission)
DECL_TESTCASE(post_event_on_non_existent_topic)
DECL_TESTCASE(post_event_when_no_subscribers)
DECL_TESTCASE(post_event_when_subscriber_exists)
DECL_TESTCASE(post_event_when_active_subscriber_exists)
DECL_TESTCASE(list_owned_topics_when_no_topics)
DECL_TESTCASE(list_owned_topics_when_topic_exists)
DECL_TESTCASE(list_owned_topics_batch_when_topic_exists)
DECL_TESTCASE(subscribe_non_existent_topic)
DECL_TESTCASE(subscribe)
DECL_TESTCASE(subscribe_when_already_subscribed)
DECL_TESTCASE(unsubscribe_non_existent_topic)
DECL_TESTCASE(unsubscribe)
DECL_TESTCASE(unsubscribe_without_subscription)
DECL_TESTCASE(explore_topics_when_no_topics)
DECL_TESTCASE(explore_topics_when_topic_exists)
DECL_TESTCASE(list_subscribed_topics_before_subscription)
DECL_TESTCASE(list_subscribed_topics_after_subscription)
DECL_TESTCASE(fetch_unreceived_without_subscription)
DECL_TESTCASE(fetch_unreceived)
DECL_TESTCASE(fetch_unreceived_when_already_active)

static CU_TestInfo cases[] = {
    // no topic, no subscription
    DEFINE_TESTCASE(create_topic_without_permission),
    DEFINE_TESTCASE(post_event_on_non_existent_topic),
    DEFINE_TESTCASE(list_owned_topics_when_no_topics),
    DEFINE_TESTCASE(subscribe_non_existent_topic),
    DEFINE_TESTCASE(unsubscribe_non_existent_topic),
    DEFINE_TESTCASE(explore_topics_when_no_topics),
    DEFINE_TESTCASE(create_topic),

    // has topic, no subscription
    DEFINE_TESTCASE(create_existing_topic),
    DEFINE_TESTCASE(post_event_without_permission),
    DEFINE_TESTCASE(post_event_when_no_subscribers),
    DEFINE_TESTCASE(list_owned_topics_when_topic_exists),
    DEFINE_TESTCASE(list_owned_topics_batch_when_topic_exists),
    DEFINE_TESTCASE(unsubscribe_without_subscription),
    DEFINE_TESTCASE(explore_topics_when_topic_exists),
    DEFINE_TESTCASE(list_subscribed_topics_before_subscription),
    DEFINE_TESTCASE(fetch_unreceived_without_subscription),
    DEFINE_TESTCASE(subscribe),

    // has topic, has subscription
    DEFINE_TESTCASE(subscribe_when_already_subscribed),
    DEFINE_TESTCASE(post_event_when_subscriber_exists),
    DEFINE_TESTCASE(list_subscribed_topics_after_subscription),
    DEFINE_TESTCASE(fetch_unreceived),

    // has topic, has active subscription
    DEFINE_TESTCASE(fetch_unreceived_when_already_active),
    DEFINE_TESTCASE(post_event_when_active_subscriber_exists),
    DEFINE_TESTCASE(unsubscribe),

    DEFINE_TESTCASE_NULL
};

CU_TestInfo* feeds_get_cases()
{
    return cases;
}

static
void print_nothing(const char *format, va_list args)
{
}

static
int publisher_init()
{
    ElaOptions opts;
    char arg_log_level[128];
    char arg_data_dir[PATH_MAX];
    char arg_log_file[PATH_MAX];
    char *argv[] = {
        "",
        "--log-level",
        arg_log_level,
        "--data-dir",
        arg_data_dir,
        "--log-file",
        arg_log_file
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    sprintf(arg_log_level, "%d", test_config.tests.loglevel);
    sprintf(arg_data_dir, "%s/publisher", test_config.persistent_location);
    if (test_config.log2file)
        sprintf(arg_log_file, "%s/publisher.log", arg_data_dir);
    else
        argc -= 2;

    memset(&opts, 0, sizeof(opts));
    carrier_config_copy(&opts, &test_config.ela_options);
    carrier_config_update(&opts, argc, argv);
    opts.log_printer = print_nothing;

    publisher = feeds_client_create(&opts);
    carrier_config_free(&opts);

    return publisher ? 0 : -1;
}

static
int subscriber_init()
{
    ElaOptions opts;
    char arg_log_level[128];
    char arg_data_dir[PATH_MAX];
    char arg_log_file[PATH_MAX];
    char *argv[] = {
        "",
        "--log-level",
        arg_log_level,
        "--data-dir",
        arg_data_dir,
        "--log-file",
        arg_log_file
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    sprintf(arg_log_level, "%d", test_config.tests.loglevel);
    sprintf(arg_data_dir, "%s/subscriber", test_config.persistent_location);
    if (test_config.log2file)
        sprintf(arg_log_file, "%s/subscriber.log", arg_data_dir);
    else
        argc -= 2;

    memset(&opts, 0, sizeof(opts));
    carrier_config_copy(&opts, &test_config.ela_options);
    carrier_config_update(&opts, argc, argv);
    opts.log_printer = print_nothing;

    subscriber = feeds_client_create(&opts);
    carrier_config_free(&opts);

    return subscriber ? 0 : -1;
}


static
int connect_robot(const char *host, const char *port)
{
    int ntries = 0;
    assert(host && *host);
    assert(port && *port);

    while (ntries < 3) {
        msg_sock = socket_connect(host, port);
        if (msg_sock != INVALID_SOCKET) {
            struct timeval timeout = {900,0};//900s
            setsockopt(msg_sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
            setsockopt(msg_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
            break;
        }

        if (socket_errno() == ENODATA) {
            ntries++;
            sleep(1);
        }
    }

    if (msg_sock == INVALID_SOCKET)
        return -1;

    return 0;
}

static
void disconnect_robot()
{
    if (msg_sock != INVALID_SOCKET) {
        socket_close(msg_sock);
        msg_sock = INVALID_SOCKET;
    }
}

static
int notify_robot_of_publisher_node_id()
{
    char node_id[ELA_MAX_ID_LEN + 1];
    int rc;

    rc = connect_robot(test_config.robot.host, test_config.robot.port);
    if (rc < 0)
        return -1;

    memset(node_id, 0, sizeof(node_id));
    ela_get_nodeid(feeds_client_get_carrier(publisher), node_id, sizeof(node_id));

    rc = send(msg_sock, node_id, strlen(node_id), 0);
    if (rc != strlen(node_id)) {
        disconnect_robot();
        return -1;
    }

    return 0;
}

static
int get_robot_address()
{
    int rc;

    rc = recv(msg_sock, robot_address, sizeof(robot_address) - 1, 0);
    if (rc < 0 || !ela_address_is_valid(robot_address))
        return -1;

    ela_get_id_by_address(robot_address, robot_node_id, sizeof(robot_node_id));

    return 0;
}

int feeds_suite_cleanup()
{
    if (publisher)
        feeds_client_delete(publisher);

    if (subscriber)
        feeds_client_delete(subscriber);

    disconnect_robot();

    return 0;
}

int feeds_suite_init()
{
    int rc;

    rc = publisher_init();
    if (rc < 0)
        goto cleanup;

    rc = subscriber_init();
    if (rc < 0)
        goto cleanup;

    fprintf(stderr, "waiting publisher online ...");
    feeds_client_wait_until_online(publisher);
    fprintf(stderr, "ok\n");

    fprintf(stderr, "waiting subscriber online ...");
    feeds_client_wait_until_online(subscriber);
    fprintf(stderr, "ok\n");

    fprintf(stderr, "notifying robot of publisher's node id ...");
    fflush(stderr);
    rc = notify_robot_of_publisher_node_id();
    if (rc < 0)
        goto cleanup;
    fprintf(stderr, "ok\n");

    fprintf(stderr, "getting carrier address of robot ...");
    fflush(stderr);
    rc = get_robot_address();
    if (rc < 0)
        goto cleanup;
    fprintf(stderr, "ok\n");

    fprintf(stderr, "waiting robot connected to publisher ...");
    fflush(stderr);
    rc = feeds_client_friend_add(publisher, robot_address, "hello");
    if (rc < 0)
        goto cleanup;
    fprintf(stderr, "ok\n");

    fprintf(stderr, "waiting robot connected to subscriber ...");
    fflush(stderr);
    rc = feeds_client_friend_add(subscriber, robot_address, "hello");
    if (rc < 0)
        goto cleanup;
    fprintf(stderr, "ok\n");

    memset(topic_desc, 'a', sizeof(topic_desc) - 1);

    return 0;

cleanup:
    feeds_suite_cleanup();
    CU_FAIL("test suite initialize error");
    return -1;
}

