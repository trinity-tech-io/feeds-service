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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <crystal.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PROCESS_H
#include <process.h>
#endif
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <ela_carrier.h>
#include <cjson/cJSON.h>
#include <crystal.h>

#include "jsonrpc.h"
#include "error_code.h"
#include "db.h"
#include "carrier_config.h"
#include "config.h"
#include "mkdirs.h"

typedef struct {
    hash_entry_t he;
    bool node_owner;
    char jwt_token[256];
} Publisher;

typedef struct {
    hash_entry_t he_name_key;
    hash_entry_t he_id_key;
    hashtable_t *active_subscribers;
    uint64_t id;
    uint64_t next_post_id;
    Publisher *owner;
    char *name;
    char *intro;
    char buf[0];
} Channel;

typedef struct {
    hash_entry_t he;
    char jwt_token[256];
    char node_id[ELA_MAX_ID_LEN + 1];
} ActiveSubscriber;

typedef struct {
    hash_entry_t he;
    ActiveSubscriber *as;
} ActiveSubscriberEntry;

#define CHANNEL_ID_START (1)
#define POST_ID_START (1)
#define COMMENT_ID_START (1)
#define NODE_JWT "ajkds"

#define hashtable_foreach(htab, entry) \
    for (hashtable_iterate((htab), &it); \
         hashtable_iterator_next(&it, NULL, NULL, (void **)&(entry)); \
         deref((entry)))

static uint64_t next_channel_id = CHANNEL_ID_START;
static hashtable_t *active_subscribers;
static hashtable_t *publishers;
static hashtable_t *channels_by_name;
static hashtable_t *channels_by_id;
static size_t connecting_clients;
static ElaCarrier *carrier;
static bool stop;

static
void notify_of_new_post(const ActiveSubscriber *s, uint64_t cid,
                        const char *content, uint64_t pid, uint64_t created_at)
{
    cJSON *result;
    cJSON *notif;
    char *notif_str;

    result = cJSON_CreateObject();
    if (!result)
        return;

    if (!cJSON_AddNumberToObject(result, "channel_id", cid)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddNumberToObject(result, "id", pid)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddStringToObject(result, "content", content)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddNumberToObject(result, "created_at", created_at)) {
        cJSON_Delete(result);
        return;
    }

    notif = jsonrpc_encode_notification("new_post", result);
    if (!notif)
        return;

    notif_str = cJSON_PrintUnformatted(notif);
    cJSON_Delete(notif);
    if (!notif_str)
        return;

    vlogI("Sending notification: %s", notif_str);
    ela_send_friend_message(carrier, s->node_id, notif_str, strlen(notif_str) + 1, NULL);
    free(notif_str);
}

static
void notify_of_new_comment(const ActiveSubscriber *s, uint64_t cid, uint64_t pid, uint64_t id,
                           const char *username, const char *content, uint64_t cmtid, uint64_t created_at)
{
    cJSON *result;
    cJSON *notif;
    char *notif_str;

    result = cJSON_CreateObject();
    if (!result)
        return;

    if (!cJSON_AddNumberToObject(result, "channel_id", cid)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddNumberToObject(result, "post_id", pid)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddNumberToObject(result, "id", id)) {
        cJSON_Delete(result);
        return;
    }

    if (!(cmtid ?
          cJSON_AddNumberToObject(result, "comment_id", cmtid) :
          cJSON_AddNullToObject(result, "comment_id"))) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddStringToObject(result, "user_name", username)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddStringToObject(result, "content", content)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddNumberToObject(result, "created_at", created_at)) {
        cJSON_Delete(result);
        return;
    }

    notif = jsonrpc_encode_notification("new_comment", result);
    if (!notif)
        return;

    notif_str = cJSON_PrintUnformatted(notif);
    cJSON_Delete(notif);
    if (!notif_str)
        return;

    vlogI("Sending notification %s", notif_str);
    ela_send_friend_message(carrier, s->node_id, notif_str,
                            strlen(notif_str) + 1, NULL);
    free(notif_str);
}

static
void notify_of_new_likes(const ActiveSubscriber *s, uint64_t cid,
                         uint64_t pid, uint64_t cmtid, uint64_t likes)
{
    cJSON *result;
    cJSON *notif;
    char *notif_str;

    result = cJSON_CreateObject();
    if (!result)
        return;

    if (!cJSON_AddNumberToObject(result, "channel_id", cid)) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddNumberToObject(result, "post_id", pid)) {
        cJSON_Delete(result);
        return;
    }

    if (!(cmtid ?
          cJSON_AddNumberToObject(result, "comment_id", cmtid) :
          cJSON_AddNullToObject(result, "comment_id"))) {
        cJSON_Delete(result);
        return;
    }

    if (!cJSON_AddNumberToObject(result, "count", likes)) {
        cJSON_Delete(result);
        return;
    }

    notif = jsonrpc_encode_notification("new_likes", result);
    if (!notif)
        return;

    notif_str = cJSON_PrintUnformatted(notif);
    cJSON_Delete(notif);
    if (!notif_str)
        return;

    vlogI("Sending notification: %s", notif_str);
    ela_send_friend_message(carrier, s->node_id, notif_str, strlen(notif_str) + 1, NULL);
    free(notif_str);
}

static
Publisher *publisher_create(const char *jwt_token, bool is_node_owner)
{
    Publisher *p;

    p = rc_zalloc(sizeof(Publisher), NULL);
    if (!p)
        return NULL;

    strcpy(p->jwt_token, jwt_token);

    p->node_owner = is_node_owner;

    p->he.data = p;
    p->he.key = p->jwt_token;
    p->he.keylen = strlen(p->jwt_token);

    return p;
}

static
void channel_destructor(void *obj)
{
    Channel *ch = (Channel *)obj;

    if (ch->active_subscribers) {
        hashtable_clear(ch->active_subscribers);
        deref(ch->active_subscribers);
    }

    deref(ch->owner);
}

static
Channel *channel_create(uint64_t id, const char *name, const char *intro,
                        uint64_t next_post_id, Publisher *p)
{
    Channel *t;

    t = rc_zalloc(sizeof(Channel) + strlen(name) + strlen(intro) + 2, channel_destructor);
    if (!t)
        return NULL;

    t->active_subscribers = hashtable_create(8, 0, NULL, NULL);
    if (!t->active_subscribers) {
        deref(t);
        return NULL;
    }

    t->id = id;
    t->next_post_id = next_post_id;
    t->owner = ref(p);

    strcpy(t->buf, name);
    t->name = t->buf;

    strcpy(t->buf + strlen(t->buf) + 1, intro);
    t->intro = t->buf + strlen(t->buf) + 1;

    t->he_name_key.data = t;
    t->he_name_key.key = t->name;
    t->he_name_key.keylen = strlen(t->name);

    t->he_id_key.data = t;
    t->he_id_key.key = &t->id;
    t->he_id_key.keylen = sizeof(t->id);

    return t;
}

static
ActiveSubscriber *active_subscriber_create(const char *jwt, const char *node_id)
{
    ActiveSubscriber *as;

    as = rc_zalloc(sizeof(ActiveSubscriber), NULL);
    if (!as)
        return NULL;

    strcpy(as->jwt_token, jwt);
    strcpy(as->node_id, node_id);

    as->he.data = as;
    as->he.key = as->jwt_token;
    as->he.keylen = strlen(as->jwt_token);

    return as;
}

static
void subscriber_entry_destructor(void *obj)
{
    ActiveSubscriberEntry *se = (ActiveSubscriberEntry *)obj;

    deref(se->as);
}

static
ActiveSubscriberEntry *subscriber_entry_create(const char *jwt, ActiveSubscriber *as)
{
    ActiveSubscriberEntry *se;

    se = rc_zalloc(sizeof(ActiveSubscriberEntry), subscriber_entry_destructor);
    if (!se)
        return NULL;

    se->as = ref(as);

    se->he.data = se;
    se->he.key = as->jwt_token;
    se->he.keylen = strlen(as->jwt_token);

    return se;
}

static
cJSON *detach_id_from_jsonrpc_req(cJSON *req)
{
    cJSON *id;

    if (!req || !(id = jsonrpc_get_id(req)))
        return NULL;

    cJSON_DetachItemViaPointer(req, id);
    return id;
}

static inline
Publisher *publisher_get(const char *jwt)
{
    return hashtable_get(publishers, jwt, strlen(jwt));
}

static inline
Publisher *publisher_put(Publisher *p)
{
    return hashtable_put(publishers, &p->he);
}

static inline
Publisher *publisher_remove(const char *jwt)
{
    return hashtable_remove(publishers, jwt, strlen(jwt));
}

static inline
int channel_exist_by_name(const char *name)
{
    return hashtable_exist(channels_by_name, name, strlen(name));
}

static inline
Channel *channel_get_by_id(uint64_t id)
{
    return hashtable_get(channels_by_id, &id, sizeof(id));
}

static inline
int channel_exist_by_id(uint64_t id)
{
    return hashtable_exist(channels_by_id, &id, sizeof(id));
}


static inline
Channel *channel_put(Channel *ch)
{
    hashtable_put(channels_by_name, &ch->he_name_key);
    return hashtable_put(channels_by_id, &ch->he_id_key);
}

static inline
ActiveSubscriber *active_subscriber_get(const char *jwt)
{
    return hashtable_get(active_subscribers, jwt, strlen(jwt));
}

static inline
int active_subscriber_exist(const char *jwt)
{
    return hashtable_exist(active_subscribers, jwt, strlen(jwt));
}

static inline
ActiveSubscriber *active_subscriber_put(ActiveSubscriber *as)
{
    return hashtable_put(active_subscribers, &as->he);
}

static inline
ActiveSubscriberEntry *active_subscriber_entry_put(Channel *ch, ActiveSubscriberEntry *ase)
{
    return hashtable_put(ch->active_subscribers, &ase->he);
}

static inline
ActiveSubscriberEntry *active_subscriber_entry_remove(Channel *ch, const char *jwt)
{
    return hashtable_remove(ch->active_subscribers, jwt, strlen(jwt));
}

static
cJSON *handle_create_channel_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *name;
    const cJSON *intro;
    const cJSON *jwt;
    cJSON *resp = NULL;
    cJSON *res = NULL;
    Publisher *p = NULL;
    Channel *ch = NULL;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(name = cJSON_GetObjectItemCaseSensitive(params, "name")) ||
        !(intro = cJSON_GetObjectItemCaseSensitive(params, "introduction")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsString(name) || !name->valuestring[0] ||
        !cJSON_IsString(intro) || !intro->valuestring[0] ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    p = publisher_get(jwt->valuestring);
    if (!p) {
        vlogE("Creating channel when not being node publisher");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_AUTHORIZED),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (channel_exist_by_name(name->valuestring)) {
        vlogE("Creating an existing Channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_EALREADY_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    ch = channel_create(next_channel_id++, name->valuestring,
                        intro->valuestring, POST_ID_START, p);
    if (!ch) {
        vlogE("Creating channel failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_create_channel(name->valuestring, p->jwt_token, intro->valuestring);
    if (rc < 0) {
        vlogE("Adding channel to database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    channel_put(ch);

    res = cJSON_CreateObject();
    if (!res)
        goto finally;

    if (!cJSON_AddNumberToObject(res, "id", ch->id))
        goto finally;

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    deref(ch);
    deref(p);
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_publish_post_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *cid;
    const cJSON *content;
    const cJSON *jwt;
    cJSON *resp = NULL;
    cJSON *res = NULL;
    uint64_t pid;
    hashtable_iterator_t it;
    ActiveSubscriberEntry *ase;
    Channel *ch = NULL;
    int rc;
    time_t now;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
        !(content = cJSON_GetObjectItemCaseSensitive(params, "content")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(cid) || !cJSON_IsString(content) || !content->valuestring[0] ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        rc = JSONRPC_EINVALID_PARAMS;
        vlogE("Invalid parameter");
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    ch = channel_get_by_id(cid->valuedouble);
    if (!ch) {
        vlogE("Publishing post on non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (strcmp(ch->owner->jwt_token, jwt->valuestring)) {
        printf("Publishing post when not being channel owner");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_AUTHORIZED),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    pid = ch->next_post_id++;
    rc = db_add_post(cid->valuedouble, pid, content->valuestring,
                     strlen(content->valuestring) + 1, now = time(NULL));
    if (rc < 0) {
        vlogE("Adding post to database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    res = cJSON_CreateObject();
    if (!res)
        goto finally;

    if (!cJSON_AddNumberToObject(res, "id", pid))
        goto finally;

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

    hashtable_foreach(ch->active_subscribers, ase)
        notify_of_new_post(ase->as, cid->valuedouble, content->valuestring, pid, now);

finally:
    deref(ch);
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_post_comment_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *cid;
    const cJSON *pid;
    const cJSON *comment;
    const cJSON *content;
    const cJSON *jwt;
    cJSON *resp = NULL;
    cJSON *res = NULL;
    hashtable_iterator_t it;
    ActiveSubscriberEntry *ase;
    Channel *ch = NULL;
    uint64_t cmtid;
    uint64_t id;
    int rc;
    time_t now;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
        !(pid = cJSON_GetObjectItemCaseSensitive(params, "post_id")) ||
        !(comment = cJSON_GetObjectItemCaseSensitive(params, "comment_id")) ||
        !(content = cJSON_GetObjectItemCaseSensitive(params, "content")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(cid) || !cJSON_IsNumber(pid) ||
        !(cJSON_IsNumber(comment) || cJSON_IsNull(comment)) ||
        !cJSON_IsString(content) || !content->valuestring[0] ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    ch = channel_get_by_id(cid->valuedouble);
    if (!ch) {
        vlogE("Posting comment on non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (pid->valuedouble < POST_ID_START || pid->valuedouble >= ch->next_post_id) {
        vlogE("Posting comment on non-existent post");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    cmtid = cJSON_IsNumber(comment) ? (uint64_t)comment->valuedouble : 0;
    if (cmtid && ((rc = db_comment_exists(cid->valuedouble, pid->valuedouble, cmtid)) < 0 || !rc)) {
        vlogE("Posting comment on non-existent comment");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_add_comment(cid->valuedouble, pid->valuedouble, cmtid, jwt->valuestring,
                        content->valuestring, strlen(content->valuestring) + 1,
                        now = time(NULL), &id);
    if (rc < 0) {
        vlogE("Adding comment to database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    res = cJSON_CreateObject();
    if (!res)
        goto finally;

    if (!cJSON_AddNumberToObject(res, "id", id))
        goto finally;

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

    hashtable_foreach(ch->active_subscribers, ase)
        notify_of_new_comment(ase->as, cid->valuedouble, pid->valuedouble,
                              id, jwt->valuestring, content->valuestring, cmtid, now);

finally:
    deref(ch);
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_post_like_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *cid;
    const cJSON *pid;
    const cJSON *comment;
    const cJSON *jwt;
    cJSON *resp = NULL;
    hashtable_iterator_t it;
    ActiveSubscriberEntry *ase;
    Channel *ch = NULL;
    uint64_t cmtid;
    uint64_t likes;
    int rc;
    time_t now;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
        !(pid = cJSON_GetObjectItemCaseSensitive(params, "post_id")) ||
        !(comment = cJSON_GetObjectItemCaseSensitive(params, "comment_id")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(cid) || !cJSON_IsNumber(pid) ||
        !(cJSON_IsNumber(comment) || cJSON_IsNull(comment)) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    ch = channel_get_by_id(cid->valuedouble);
    if (!ch) {
        vlogE("Posting like on non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (pid->valuedouble < POST_ID_START || pid->valuedouble >= ch->next_post_id) {
        vlogE("Posting like on non-existent post");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    cmtid = cJSON_IsNumber(comment) ? (uint64_t)comment->valuedouble : 0;
    if (cmtid && ((rc = db_comment_exists(cid->valuedouble, pid->valuedouble, cmtid)) < 0 || !rc)) {
        vlogE("Posting like on non-existent comment");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_add_like(cid->valuedouble, pid->valuedouble, cmtid, &likes);
    if (rc < 0) {
        vlogE("Adding like to database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(NULL, detach_id_from_jsonrpc_req(req));

    if (!(likes % 10))
        hashtable_foreach(ch->active_subscribers, ase)
            notify_of_new_likes(ase->as, cid->valuedouble, pid->valuedouble, cmtid, likes);

finally:
    deref(ch);
    cJSON_Delete(req);

    return resp;
}

static
cJSON *handle_get_my_channels_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *jwt;
    const cJSON *by;
    const cJSON *upper;
    const cJSON *lower;
    const cJSON *maxcnt;
    cJSON *res = NULL;
    cJSON *resp;
    QueryField qf;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(by = cJSON_GetObjectItemCaseSensitive(params, "by")) ||
        !(upper = cJSON_GetObjectItemCaseSensitive(params, "upper_bound")) ||
        !(lower = cJSON_GetObjectItemCaseSensitive(params, "lower_bound")) ||
        !(maxcnt = cJSON_GetObjectItemCaseSensitive(params, "max_count")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(by) || (qf = by->valuedouble) < ID || qf > CREATED_AT ||
        !(cJSON_IsNumber(upper) || cJSON_IsNull(upper)) ||
        !(cJSON_IsNumber(lower) || cJSON_IsNull(lower)) ||
        (cJSON_IsNumber(upper) && cJSON_IsNumber(lower) &&
         (uint64_t)upper->valuedouble < (uint64_t)lower->valuedouble) ||
        !(cJSON_IsNumber(maxcnt) || cJSON_IsNull(maxcnt)) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_owned_channels(jwt->valuestring, qf,
                               cJSON_IsNumber(upper) ? (uint64_t)upper->valuedouble : 0,
                               cJSON_IsNumber(lower) ? (uint64_t)lower->valuedouble : 0,
                               cJSON_IsNumber(maxcnt) ? (uint64_t)maxcnt->valuedouble : 0,
                               &res);
    if (rc < 0) {
        vlogE("Getting owned channels from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_get_my_channels_metadata_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *jwt;
    const cJSON *by;
    const cJSON *upper;
    const cJSON *lower;
    const cJSON *maxcnt;
    cJSON *res = NULL;
    cJSON *resp;
    QueryField qf;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(by = cJSON_GetObjectItemCaseSensitive(params, "by")) ||
        !(upper = cJSON_GetObjectItemCaseSensitive(params, "upper_bound")) ||
        !(lower = cJSON_GetObjectItemCaseSensitive(params, "lower_bound")) ||
        !(maxcnt = cJSON_GetObjectItemCaseSensitive(params, "max_count")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(by) || (qf = by->valuedouble) < ID || qf > CREATED_AT ||
        !(cJSON_IsNumber(upper) || cJSON_IsNull(upper)) ||
        !(cJSON_IsNumber(lower) || cJSON_IsNull(lower)) ||
        (cJSON_IsNumber(upper) && cJSON_IsNumber(lower) &&
         (uint64_t)upper->valuedouble < (uint64_t)lower->valuedouble) ||
        !(cJSON_IsNumber(maxcnt) || cJSON_IsNull(maxcnt)) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_owned_channels_metadata(jwt->valuestring, qf,
                                        cJSON_IsNumber(upper) ? (uint64_t)upper->valuedouble : 0,
                                        cJSON_IsNumber(lower) ? (uint64_t)lower->valuedouble : 0,
                                        cJSON_IsNumber(maxcnt) ? (uint64_t)maxcnt->valuedouble : 0,
                                        &res);
    if (rc < 0) {
        vlogE("Getting owned channels metadata from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_get_channels_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *jwt;
    const cJSON *by;
    const cJSON *upper;
    const cJSON *lower;
    const cJSON *maxcnt;
    cJSON *res = NULL;
    cJSON *resp;
    QueryField qf;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(by = cJSON_GetObjectItemCaseSensitive(params, "by")) ||
        !(upper = cJSON_GetObjectItemCaseSensitive(params, "upper_bound")) ||
        !(lower = cJSON_GetObjectItemCaseSensitive(params, "lower_bound")) ||
        !(maxcnt = cJSON_GetObjectItemCaseSensitive(params, "max_count")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(by) || (qf = by->valuedouble) < ID || qf > CREATED_AT ||
        !(cJSON_IsNumber(upper) || cJSON_IsNull(upper)) ||
        !(cJSON_IsNumber(lower) || cJSON_IsNull(lower)) ||
        (cJSON_IsNumber(upper) && cJSON_IsNumber(lower) &&
         (uint64_t)upper->valuedouble < (uint64_t)lower->valuedouble) ||
        !(cJSON_IsNumber(maxcnt) || cJSON_IsNull(maxcnt)) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_channels(qf,
                         cJSON_IsNumber(upper) ? (uint64_t)upper->valuedouble : 0,
                         cJSON_IsNumber(lower) ? (uint64_t)lower->valuedouble : 0,
                         cJSON_IsNumber(maxcnt) ? (uint64_t)maxcnt->valuedouble : 0,
                         &res);
    if (rc < 0) {
        vlogE("Getting channels from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_get_channel_detail_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *id;
    const cJSON *jwt;
    cJSON *res = NULL;
    cJSON *resp;
    QueryField qf;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(id = cJSON_GetObjectItemCaseSensitive(params, "id")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(id) || !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (!channel_exist_by_id(id->valuedouble)) {
        vlogE("Getting detail on non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_channel_detail(id->valuedouble, &res);
    if (rc < 0) {
        vlogE("Getting channel detail from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_get_subscribed_channels_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *jwt;
    const cJSON *by;
    const cJSON *upper;
    const cJSON *lower;
    const cJSON *maxcnt;
    cJSON *res = NULL;
    cJSON *resp;
    QueryField qf;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(by = cJSON_GetObjectItemCaseSensitive(params, "by")) ||
        !(upper = cJSON_GetObjectItemCaseSensitive(params, "upper_bound")) ||
        !(lower = cJSON_GetObjectItemCaseSensitive(params, "lower_bound")) ||
        !(maxcnt = cJSON_GetObjectItemCaseSensitive(params, "max_count")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(by) || (qf = by->valuedouble) < ID || qf > CREATED_AT ||
        !(cJSON_IsNumber(upper) || cJSON_IsNull(upper)) ||
        !(cJSON_IsNumber(lower) || cJSON_IsNull(lower)) ||
        (cJSON_IsNumber(upper) && cJSON_IsNumber(lower) &&
         (uint64_t)upper->valuedouble < (uint64_t)lower->valuedouble) ||
        !(cJSON_IsNumber(maxcnt) || cJSON_IsNull(maxcnt)) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_subscribed_channels(jwt->valuestring, qf,
                                    cJSON_IsNumber(upper) ? (uint64_t)upper->valuedouble : 0,
                                    cJSON_IsNumber(lower) ? (uint64_t)lower->valuedouble : 0,
                                    cJSON_IsNumber(maxcnt) ? (uint64_t)maxcnt->valuedouble : 0,
                                    &res);
    if (rc < 0) {
        vlogE("Getting subscribed channels from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_get_posts_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *cid;
    const cJSON *jwt;
    const cJSON *by;
    const cJSON *upper;
    const cJSON *lower;
    const cJSON *maxcnt;
    cJSON *res = NULL;
    cJSON *resp;
    QueryField qf;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
        !(by = cJSON_GetObjectItemCaseSensitive(params, "by")) ||
        !(upper = cJSON_GetObjectItemCaseSensitive(params, "upper_bound")) ||
        !(lower = cJSON_GetObjectItemCaseSensitive(params, "lower_bound")) ||
        !(maxcnt = cJSON_GetObjectItemCaseSensitive(params, "max_count")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(cid) ||
        !cJSON_IsNumber(by) || (qf = by->valuedouble) < ID || qf > CREATED_AT ||
        !(cJSON_IsNumber(upper) || cJSON_IsNull(upper)) ||
        !(cJSON_IsNumber(lower) || cJSON_IsNull(lower)) ||
        (cJSON_IsNumber(upper) && cJSON_IsNumber(lower) &&
         (uint64_t)upper->valuedouble < (uint64_t)lower->valuedouble) ||
        !(cJSON_IsNumber(maxcnt) || cJSON_IsNull(maxcnt)) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (!channel_exist_by_id(cid->valuedouble)) {
        vlogE("Getting posts from non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_posts(cid->valuedouble, qf,
                      cJSON_IsNumber(upper) ? (uint64_t)upper->valuedouble : 0,
                      cJSON_IsNumber(lower) ? (uint64_t)lower->valuedouble : 0,
                      cJSON_IsNumber(maxcnt) ? (uint64_t)maxcnt->valuedouble : 0,
                      &res);
    if (rc < 0) {
        vlogE("Getting posts from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_get_comments_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *cid;
    const cJSON *pid;
    const cJSON *jwt;
    const cJSON *by;
    const cJSON *upper;
    const cJSON *lower;
    const cJSON *maxcnt;
    cJSON *res = NULL;
    cJSON *resp;
    QueryField qf;
    Channel *ch = NULL;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
        !(pid = cJSON_GetObjectItemCaseSensitive(params, "post_id")) ||
        !(by = cJSON_GetObjectItemCaseSensitive(params, "by")) ||
        !(upper = cJSON_GetObjectItemCaseSensitive(params, "upper_bound")) ||
        !(lower = cJSON_GetObjectItemCaseSensitive(params, "lower_bound")) ||
        !(maxcnt = cJSON_GetObjectItemCaseSensitive(params, "max_count")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(cid) || !cJSON_IsNumber(pid) ||
        !cJSON_IsNumber(by) || (qf = by->valuedouble) < ID || qf > CREATED_AT ||
        !(cJSON_IsNumber(upper) || cJSON_IsNull(upper)) ||
        !(cJSON_IsNumber(lower) || cJSON_IsNull(lower)) ||
        (cJSON_IsNumber(upper) && cJSON_IsNumber(lower) &&
        (uint64_t)upper->valuedouble < (uint64_t)lower->valuedouble) ||
        !(cJSON_IsNumber(maxcnt) || cJSON_IsNull(maxcnt)) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    ch = channel_get_by_id(cid->valuedouble);
    if (!ch) {
        vlogE("Getting comment from non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (pid->valuedouble < POST_ID_START || pid->valuedouble >= ch->next_post_id) {
        vlogE("Getting comment from non-existent post");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_comments(cid->valuedouble, pid->valuedouble, qf,
                         cJSON_IsNumber(upper) ? (uint64_t)upper->valuedouble : 0,
                         cJSON_IsNumber(lower) ? (uint64_t)lower->valuedouble : 0,
                         cJSON_IsNumber(maxcnt) ? (uint64_t)maxcnt->valuedouble : 0,
                         &res);
    if (rc < 0) {
        vlogE("Getting comments from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    deref(ch);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_get_statistics_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *jwt;
    cJSON *res = NULL;
    cJSON *resp = NULL;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    res = cJSON_CreateObject();
    if (!res)
        goto finally;

    if (!cJSON_AddStringToObject(res, "did", NODE_JWT))
        goto finally;

    if (!cJSON_AddNumberToObject(res, "connecting_clients", connecting_clients))
        goto finally;

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_subscribe_channel_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *cid;
    const cJSON *jwt;
    cJSON *resp;
    Channel *ch = NULL;
    ActiveSubscriber *as = NULL;
    ActiveSubscriberEntry *ase = NULL;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(params, "id")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(cid) || !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    ch = channel_get_by_id(cid->valuedouble);
    if (!ch) {
        vlogE("Subscribing non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if ((rc = db_is_subscriber(cid->valuedouble, jwt->valuestring)) < 0 || rc) {
        vlogE("Subscribing subscribed channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_EALREADY_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    as = active_subscriber_get(jwt->valuestring);
    if (as) {
        ase = subscriber_entry_create(jwt->valuestring, as);
        if (!ase) {
            rc = JSONRPC_EINTERNAL_ERROR;
            resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                                 NULL, detach_id_from_jsonrpc_req(req));
            goto finally;
        }
    }

    rc = db_add_subscriber(cid->valuedouble, jwt->valuestring);
    if (rc < 0) {
        vlogE("Adding subscription to database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (ase)
        active_subscriber_entry_put(ch, ase);

    resp = jsonrpc_encode_success_response(NULL, detach_id_from_jsonrpc_req(req));

finally:
    cJSON_Delete(req);
    deref(ch);
    deref(as);
    deref(ase);

    return resp;
}

static
cJSON *handle_unsubscribe_channel_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *cid;
    const cJSON *jwt;
    cJSON *resp;
    Channel *ch = NULL;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(params, "id")) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsNumber(cid) || !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    ch = channel_get_by_id(cid->valuedouble);
    if (!ch) {
        vlogE("Unsubscribing non-existent channel");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if ((rc = db_is_subscriber(cid->valuedouble, jwt->valuestring)) < 0 || !rc) {
        vlogE("Unsubscribing non-existent subscription");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_unsubscribe(cid->valuedouble, jwt->valuestring);
    if (rc < 0) {
        vlogE("Removing subscription from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    deref(active_subscriber_entry_remove(ch, jwt->valuestring));

    resp = jsonrpc_encode_success_response(NULL, detach_id_from_jsonrpc_req(req));

finally:
    cJSON_Delete(req);
    deref(ch);

    return resp;
}

static
cJSON *handle_add_node_publisher_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *did;
    const cJSON *jwt;
    cJSON *resp;
    Publisher *p = NULL;
    uint64_t cid;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !(did = cJSON_GetObjectItemCaseSensitive(params, "did")) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0] ||
        !cJSON_IsString(did) || !did->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    p = publisher_get(jwt->valuestring);
    if (!p || !p->node_owner) {
        vlogE("Adding node publisher when not being node owner");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_AUTHORIZED),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }
    deref(p);

    p = publisher_get(did->valuestring);
    if (p) {
        vlogE("Adding existing node publisher");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_EALREADY_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    p = publisher_create(did->valuestring, false);
    if (!p) {
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_add_node_publisher(jwt->valuestring);
    if (rc < 0) {
        vlogE("Adding node publisher to database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    publisher_put(p);

    resp = jsonrpc_encode_success_response(NULL, detach_id_from_jsonrpc_req(req));

finally:
    cJSON_Delete(req);
    deref(p);

    return resp;
}

static
cJSON *handle_remove_node_publisher_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *did;
    const cJSON *jwt;
    cJSON *resp;
    Publisher *p = NULL;
    uint64_t cid;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !(did = cJSON_GetObjectItemCaseSensitive(params, "did")) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0] ||
        !cJSON_IsString(did) || !did->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    p = publisher_get(jwt->valuestring);
    if (!p || !p->node_owner) {
        vlogE("Removing node publisher when not being node owner");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_AUTHORIZED),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }
    deref(p);

    p = publisher_get(did->valuestring);
    if (!p || p->node_owner) {
        vlogE("Removing non-existent node publisher");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_ENOT_EXISTS),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_remove_node_publisher(jwt->valuestring);
    if (rc < 0) {
        vlogE("Removing node publisher from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    deref(publisher_remove(did->valuestring));

    resp = jsonrpc_encode_success_response(NULL, detach_id_from_jsonrpc_req(req));

finally:
    cJSON_Delete(req);
    deref(p);

    return resp;
}

static
cJSON *handle_query_channel_creation_permission_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *jwt;
    cJSON *resp = NULL;
    cJSON *res = NULL;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        rc = JSONRPC_EINVALID_PARAMS;
        vlogE("Invalid parameter");
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    res = cJSON_CreateObject();
    if (!res)
        goto finally;

    if (!cJSON_AddBoolToObject(res, "authorized",
                               hashtable_exist(publishers,
                                                        jwt->valuestring,
                                                        strlen(jwt->valuestring)) ?
                               true : false))
        goto finally;

    resp = jsonrpc_encode_success_response(res, detach_id_from_jsonrpc_req(req));
    res = NULL;

finally:
    cJSON_Delete(req);
    if (res)
        cJSON_Delete(res);

    return resp;
}

static
cJSON *handle_enable_notification_request(const char *from, cJSON *req)
{
    const cJSON *params;
    const cJSON *jwt;
    cJSON *resp;
    cJSON *schs = NULL;
    ActiveSubscriber *as = NULL;
    size_t i;
    size_t nschs;
    ActiveSubscriberEntry **ases = NULL;
    int rc;

    params = jsonrpc_get_params(req);
    if (!params || !cJSON_IsObject(params) ||
        !(jwt = cJSON_GetObjectItemCaseSensitive(params, "jwttoken")) ||
        !cJSON_IsString(jwt) || !jwt->valuestring[0]) {
        vlogE("Invalid parameter");
        rc = JSONRPC_EINVALID_PARAMS;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc), NULL,
                                             detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    if (active_subscriber_exist(jwt->valuestring)) {
        vlogE("Already enabled notification");
        resp = jsonrpc_encode_error_response(JSONRPC_EINVALID_PARAMS,
                                             error_code_strerror(FEEDS_EWRONG_STATE),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    as = active_subscriber_create(jwt->valuestring, from);
    if (!as) {
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    rc = db_get_subscribed_channels(jwt->valuestring, ID, 0, 0, 0, &schs);
    if (rc < 0) {
        vlogE("Getting subscribed channels from database failed");
        rc = JSONRPC_EINTERNAL_ERROR;
        resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, detach_id_from_jsonrpc_req(req));
        goto finally;
    }

    nschs = cJSON_GetArraySize(schs);
    if (!nschs)
        goto success;

    ases = alloca(sizeof(ActiveSubscriberEntry *) * nschs);
    memset(ases, 0, sizeof(ActiveSubscriberEntry *) * nschs);

    for (i = 0; i < nschs; ++i) {
        ases[i] = subscriber_entry_create(jwt->valuestring, as);
        if (!ases[i]) {
            rc = JSONRPC_EINTERNAL_ERROR;
            resp = jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                                 NULL, detach_id_from_jsonrpc_req(req));
            goto finally;
        }
    }

    for (i = 0; i < nschs; ++i) {
        Channel *ch = channel_get_by_id(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(schs, i),
                                                                         "id")->valuedouble);

        active_subscriber_entry_put(ch, ases[i]);
        deref(ch);
    }

success:
    active_subscriber_put(as);
    resp = jsonrpc_encode_success_response(NULL, detach_id_from_jsonrpc_req(req));

finally:
    cJSON_Delete(req);
    deref(as);
    if (schs)
        cJSON_Delete(schs);
    if (ases) {
        for (i = 0; i < nschs && ases[i]; ++i)
            deref(ases[i]);
    }

    return resp;
}

typedef cJSON *MethodHandler(const char *from, cJSON *req);
static struct {
    char *name;
    MethodHandler *handler;
} method_handlers[] = {
    {"create_channel"                   , handle_create_channel_request                    },
    {"publish_post"                     , handle_publish_post_request                      },
    {"post_comment"                     , handle_post_comment_request                      },
    {"post_like"                        , handle_post_like_request                         },
    {"get_my_channels"                  , handle_get_my_channels_request                   },
    {"get_my_channels_metadata"         , handle_get_my_channels_metadata_request          },
    {"get_channels"                     , handle_get_channels_request                      },
    {"get_channel_detail"               , handle_get_channel_detail_request                },
    {"get_subscribed_channels"          , handle_get_subscribed_channels_request           },
    {"get_posts"                        , handle_get_posts_request                         },
    {"get_comments"                     , handle_get_comments_request                      },
    {"get_statistics"                   , handle_get_statistics_request                    },
    {"subscribe_channel"                , handle_subscribe_channel_request                 },
    {"unsubscribe_channel"              , handle_unsubscribe_channel_request               },
    {"add_node_publisher"               , handle_add_node_publisher_request                },
    {"remove_node_publisher"            , handle_remove_node_publisher_request             },
    {"query_channel_creation_permission", handle_query_channel_creation_permission_request },
    {"enable_notification"              , handle_enable_notification_request               }
};

static
MethodHandler *get_method_handler(const char *method)
{
    for (int i = 0; i < sizeof(method_handlers) / sizeof(method_handlers[0]); ++i) {
        if (!strcmp(method, method_handlers[i].name))
            return method_handlers[i].handler;
    }

    return NULL;
}

static
cJSON *handle_request(const char *from, cJSON *req)
{
    MethodHandler *handler;
    int rc;

    handler = get_method_handler(jsonrpc_get_method(req));
    if (!handler) {
        vlogE("Unsupported method: %s", jsonrpc_get_method(req));
        rc = JSONRPC_EMETHOD_NOT_FOUND;
        if (req)
            cJSON_Delete(req);
        return jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, NULL);
    }

    return handler(from, req);
}

static
cJSON *handle_object(const char *from, cJSON *obj, JsonRPCType type)
{
    int rc;

    if (type == JSONRPC_TYPE_INVALID) {
        vlogE("Invalid request");
        rc = JSONRPC_EPARSE_ERROR;
        if (obj)
            cJSON_Delete(obj);
        return jsonrpc_encode_error_response(rc, jsonrpc_error_message(rc),
                                             NULL, NULL);
    }

    if (type == JSONRPC_TYPE_REQUEST)
        return handle_request(from, obj);

    return NULL;
}

static
void on_receiving_message(ElaCarrier *c, const char *from,
                          const void *msg, size_t len, bool offline, void *context)
{
    int i;
    cJSON *rpc = NULL;
    cJSON *resp = NULL;
    JsonRPCType type;
    JsonRPCBatchInfo *bi = NULL;

    vlogI("Receiving data: %.*s", (int)len, msg);

    type = jsonrpc_decode(msg, &rpc, &bi);
    if (type != JSONRPC_TYPE_BATCH) {
        resp = handle_object(from, rpc, type);
        rpc = NULL;
        goto finally;
    }

    resp = jsonrpc_encode_empty_batch();
    if (!resp)
        goto finally;

    for (i = 0; i < bi->nobjs; ++i) {
        cJSON *obj = cJSON_DetachItemFromArray(rpc, 0);
        cJSON *resp_obj = handle_object(from, obj, bi->obj_types[i]);
        if (resp_obj)
            jsonrpc_add_object_to_batch(resp, resp_obj);
    }

finally:
    if (resp) {
        char *resp_str = cJSON_PrintUnformatted(resp);
        if (resp_str) {
            vlogI("Sending response: %s", resp_str);
            ela_send_friend_message(carrier, from, resp_str, strlen(resp_str) + 1, NULL);
            free(resp_str);
        }
        cJSON_Delete(resp);
    }

    if (rpc)
        cJSON_Delete(rpc);

    if (bi)
        free(bi);
}

static
int load_channel_from_db(uint64_t id, const char *name, const char *desc,
                         uint64_t next_post_id, const char *jwt)
{
    Publisher *p;
    Channel *ch;

    p = publisher_get(jwt);
    if (!p) {
        p = publisher_create(jwt, false);
        if (!p)
            return -1;
    }

    ch = channel_create(id, name, desc, next_post_id, p);
    deref(p);
    if (!ch)
        return -1;

    deref(channel_put(ch));

    if (id >= next_channel_id)
        next_channel_id = id + 1;

    return 0;
}

static
void feeds_finalize()
{
    if (publishers) {
        hashtable_clear(publishers);
        deref(publishers);
    }

    if (channels_by_name) {
        hashtable_clear(channels_by_name);
        deref(channels_by_name);
    }

    if (channels_by_id) {
        hashtable_clear(channels_by_id);
        deref(channels_by_id);
    }

    if (active_subscribers) {
        hashtable_clear(active_subscribers);
        deref(active_subscribers);
    }

    if (carrier)
        ela_kill(carrier);
}

static
void idle_callback(ElaCarrier *c, void *context)
{
    if (stop) {
        ela_kill(carrier);
        carrier = NULL;
    }
}

static
void friend_connection_callback(ElaCarrier *c, const char *friend_id,
                                ElaConnectionStatus status, void *context)
{
    hashtable_iterator_t as_it;

    vlogI("[%s] %s", friend_id, status == ElaConnectionStatus_Connected ?
                                "connected" : "disconnected");

    if (status == ElaConnectionStatus_Connected) {
        ++connecting_clients;
        return;
    }

    --connecting_clients;
    hashtable_iterate(active_subscribers, &as_it);
    while (hashtable_iterator_has_next(&as_it)) {
        ActiveSubscriber *as;
        hashtable_iterator_t it;
        Channel *ch;
        int rc;

        rc = hashtable_iterator_next(&as_it, NULL, NULL, (void **)&as);
        if (rc <= 0)
            break;

        if (strcmp(as->node_id, friend_id)) {
            deref(as);
            continue;
        }

        hashtable_foreach(channels_by_name, ch)
            deref(active_subscriber_entry_remove(ch, as->jwt_token));

        hashtable_iterator_remove(&as_it);
        deref(as);
    }
}

static
void friend_request_callback(ElaCarrier *c, const char *user_id,
                             const ElaUserInfo *info, const char *hello,
                             void *context)
{
    ela_accept_friend(c, user_id);
}

static
int channel_id_compare(const void *key1, size_t len1, const void *key2, size_t len2)
{
    assert(key1 && sizeof(uint64_t) == len1);
    assert(key2 && sizeof(uint64_t) == len2);

    return memcmp(key1, key2, sizeof(uint64_t));
}

static
int load_node_owner_from_db(const char *jwt_token)
{
    Publisher *p;

    p = publisher_create(jwt_token, true);
    if (!p)
        return -1;

    deref(publisher_put(p));

    return 0;
}

static
int load_publisher_from_db(const char *jwt_token)
{
    Publisher *p;

    p = publisher_create(jwt_token, false);
    if (!p)
        return -1;

    deref(publisher_put(p));

    return 0;
}

static
int feeds_initialize(FeedsConfig *cfg)
{
    ElaCallbacks callbacks;
    char fpath[PATH_MAX];
    char addr[ELA_MAX_ADDRESS_LEN + 1];
    int rc;
    int i;
    int fd;

    publishers = hashtable_create(8, 0, NULL, NULL);
    if (!publishers) {
        vlogE("Creating publishers failed");
        return -1;
    }

    channels_by_name = hashtable_create(8, 0, NULL, NULL);
    if (!channels_by_name) {
        vlogE("Creating channels failed");
        goto failure;
    }

    channels_by_id = hashtable_create(8, 0, NULL, channel_id_compare);
    if (!channels_by_id) {
        vlogE("Creating channels failed");
        goto failure;
    }

    active_subscribers = hashtable_create(8, 0, NULL, NULL);
    if (!active_subscribers) {
        vlogE("Creating active subscribers failed");
        goto failure;
    }

    rc = db_iterate_node_owners(load_node_owner_from_db);
    if (rc < 0) {
        vlogE("Loading node owner from database failed");
        goto failure;
    }

    if (hashtable_is_empty(publishers)) {
        Publisher *node_owner;

        if (!cfg->node_owner[0]) {
            vlogE("Initial node owner is not configured");
            goto failure;
        }

        node_owner = publisher_create(cfg->node_owner, true);
        if (!node_owner) {
            vlogE("Creating node owner failed");
            goto failure;
        }

        rc = db_add_node_owner(cfg->node_owner);
        if (rc < 0) {
            vlogE("Adding initial node owner to database failed");
            deref(node_owner);
            goto failure;
        }

        deref(publisher_put(node_owner));
    } else {
        rc = db_iterate_publishers(load_publisher_from_db);
        if (rc < 0) {
            vlogE("Loading publishers from database failed");
            goto failure;
        }
    }

    rc = db_iterate_channels(load_channel_from_db);
    if (rc < 0) {
        vlogE("Loading channels from database failed");
        goto failure;
    }

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.idle = idle_callback;
    callbacks.friend_connection = friend_connection_callback;
    callbacks.friend_request = friend_request_callback;
    callbacks.friend_message = on_receiving_message;

    carrier = ela_new(&cfg->ela_options, &callbacks, NULL);
    if (!carrier) {
        vlogE("Creating carrier instance failed");
        goto failure;
    }

    sprintf(fpath, "%s/address.txt", cfg->ela_options.persistent_location);
    fd = open(fpath, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        vlogE("Opening file [%s] failed", fpath);
        goto failure;
    }

    ela_get_address(carrier, addr, sizeof(addr));
    rc = write(fd, addr, strlen(addr));
    close(fd);
    if (rc < 0) {
        vlogE("Writing carrier address to file failed");
        goto failure;
    }

    return 0;

failure:
    feeds_finalize();
    return -1;
}

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>

static
int sys_coredump_set(bool enable)
{
    const struct rlimit rlim = {
        enable ? RLIM_INFINITY : 0,
        enable ? RLIM_INFINITY : 0
    };

    return setrlimit(RLIMIT_CORE, &rlim);
}
#endif

static
void usage(void)
{
    printf("Elastos feeds server.\n");
    printf("Usage: elafeeds [OPTION]...\n");
    printf("\n");
    printf("First run options:\n");
    printf("  -d, --daemon              Run as daemon.\n");
    printf("  -c, --config=CONFIG_FILE  Set config file path.\n");
    printf("      --udp-enabled=0|1     Enable UDP, override the option in config.\n");
    printf("      --log-level=LEVEL     Log level(0-7), override the option in config.\n");
    printf("      --log-file=FILE       Log file name, override the option in config.\n");
    printf("      --data-dir=PATH       Data location, override the option in config.\n");
    printf("\n");
    printf("Debugging options:\n");
    printf("      --debug               Wait for debugger attach after start.\n");
    printf("\n");
}

#define CONFIG_NAME "feeds.conf"
static const char *default_config_files[] = {
    "./"CONFIG_NAME,
    "../etc/feeds/"CONFIG_NAME,
    "/usr/local/etc/feeds/"CONFIG_NAME,
    "/etc/feeds/"CONFIG_NAME,
    NULL
};

static
void shutdown_process(int signum)
{
    stop = true;
}

static
int daemonize()
{
    pid_t pid;
    struct sigaction sa;

    /*
     * Clear file creation mask.
     */
    umask(0);

    /*
     * Become a session leader to lose controlling TTY.
     */
    if ((pid = fork()) < 0)
        return -1;
    else if (pid != 0) /* parent */
        exit(0);
    setsid();

    /*
     * Ensure future opens won’t allocate controlling TTYs.
     */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return -1;
    if ((pid = fork()) < 0)
        return -1;
    else if (pid != 0) /* parent */
        exit(0);

    /*
     * Change the current working directory to the root so
     * we won’t prevent file systems from being unmounted.
     */
    if (chdir("/") < 0)
        return -1;

    /* Attach file descriptors 0, 1, and 2 to /dev/null. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);

    return 0;
}

int main(int argc, char *argv[])
{
    char buf[ELA_MAX_ADDRESS_LEN + 1];
    const char *config_file = NULL;
    int wait_for_attach = 0;
    char db_file[PATH_MAX];
    FeedsConfig cfg;
    int daemon = 0;
    int rc;

    int opt;
    int idx;
    struct option options[] = {
        { "daemon",      no_argument,        NULL, 'd' },
        { "config",      required_argument,  NULL, 'c' },
        { "udp-enabled", required_argument,  NULL, 1 },
        { "log-level",   required_argument,  NULL, 2 },
        { "log-file",    required_argument,  NULL, 3 },
        { "data-dir",    required_argument,  NULL, 4 },
        { "debug",       no_argument,        NULL, 5 },
        { "help",        no_argument,        NULL, 'h' },
        { NULL,          0,                  NULL, 0 }
    };

#ifdef HAVE_SYS_RESOURCE_H
    sys_coredump_set(true);
#endif

    while ((opt = getopt_long(argc, argv, "dc:h?", options, &idx)) != -1) {
        switch (opt) {
        case 'd':
            daemon = 1;
            break;

        case 'c':
            config_file = optarg;
            break;

        case 1:
        case 2:
        case 3:
        case 4:
            break;

        case 5:
            wait_for_attach = 1;
            break;

        case 'h':
        case '?':
        default:
            usage();
            exit(-1);
        }
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    if (wait_for_attach) {
        printf("Wait for debugger attaching, process id is: %d.\n", getpid());
        printf("After debugger attached, press any key to continue......");
        getchar();
    }

    config_file = get_config_file(config_file, default_config_files);
    if (!config_file) {
        fprintf(stderr, "Error: Missing config file.\n");
        usage();
        return -1;
    }

    memset(&cfg, 0, sizeof(cfg));
    if (!load_config(config_file, &cfg)) {
        fprintf(stderr, "loading configure failed!\n");
        return -1;
    }

    carrier_config_update(&cfg.ela_options, argc, argv);

    rc = mkdirs(cfg.ela_options.persistent_location, S_IRWXU);
    if (rc < 0) {
        free_config(&cfg);
        return -1;
    }

    sprintf(db_file, "%s/feeds.sqlite3", cfg.ela_options.persistent_location);
    rc = db_initialize(db_file);
    if (rc < 0) {
        free_config(&cfg);
        return -1;
    }

    rc = feeds_initialize(&cfg);
    free_config(&cfg);
    if (rc < 0) {
        db_finalize();
        return -1;
    }

    signal(SIGINT, shutdown_process);
    signal(SIGTERM, shutdown_process);
    signal(SIGSEGV, shutdown_process);

    printf("Carrier node identities:\n");
    printf("  Node ID: %s\n", ela_get_nodeid(carrier, buf, sizeof(buf)));
    printf("  User ID: %s\n", ela_get_userid(carrier, buf, sizeof(buf)));
    printf("  Address: %s\n", ela_get_address(carrier, buf, sizeof(buf)));

    if (daemon && daemonize() < 0) {
        fprintf(stderr, "daemonize failure!\n");
        free_config(&cfg);
        return -1;
    }

    rc = ela_run(carrier, 10);

    feeds_finalize();
    db_finalize();

    return rc;
}