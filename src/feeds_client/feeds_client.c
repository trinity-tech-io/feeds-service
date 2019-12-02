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

#include <string.h>
#include <pthread.h>
#include <crystal.h>

#include "feeds_client.h"
#include "../jsonrpc.h"

struct FeedsClient {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    ElaCarrier *carrier;
    cJSON *new_posts;
    cJSON *new_comments;
    cJSON *new_likes;
    cJSON *response;
    JsonRPCType response_type;
    JsonRPCBatchInfo *bi;
    bool waiting_response;
    pthread_t carrier_routine_tid;
};

static
void connection_callback(ElaCarrier *w, ElaConnectionStatus status, void *context)
{
    FeedsClient *fc = (FeedsClient *)context;

    pthread_mutex_lock(&fc->lock);
    pthread_mutex_unlock(&fc->lock);
    pthread_cond_signal(&fc->cond);
}

static
void friend_connection_callback(ElaCarrier *w, const char *friendid,
                                ElaConnectionStatus status, void *context)
{
    FeedsClient *fc = (FeedsClient *)context;

    pthread_mutex_lock(&fc->lock);
    pthread_mutex_unlock(&fc->lock);
    pthread_cond_signal(&fc->cond);
}

static
void on_receiving_message(ElaCarrier *carrier, const char *from,
                          const void *msg, size_t len, bool offline, void *context)
{
    FeedsClient *fc = (FeedsClient *)context;
    JsonRPCType type;
    JsonRPCBatchInfo *bi;
    cJSON *rpc;

    type = jsonrpc_decode(msg, &rpc, &bi);
    if (type != JSONRPC_TYPE_NOTIFICATION &&
        type != JSONRPC_TYPE_SUCCESS_RESPONSE &&
        type != JSONRPC_TYPE_ERROR_RESPONSE &&
        type != JSONRPC_TYPE_BATCH)
        goto finally;

    if (type == JSONRPC_TYPE_NOTIFICATION) {
        if (!strcmp(jsonrpc_get_method(rpc), "new_post")) {
            cJSON *params;
            cJSON *channel_id;
            cJSON *id;
            cJSON *content;
            cJSON *created_at;

            if (!(params = (cJSON *)jsonrpc_get_params(rpc)) ||
                !(channel_id = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
                !(id = cJSON_GetObjectItemCaseSensitive(params, "id")) ||
                !(content = cJSON_GetObjectItemCaseSensitive(params, "content")) ||
                !(created_at = cJSON_GetObjectItemCaseSensitive(params, "created_at")) ||
                !cJSON_IsNumber(channel_id) || !cJSON_IsNumber(id) ||
                !cJSON_IsString(content) || !content->valuestring[0] ||
                !cJSON_IsNumber(created_at))
                goto finally;

            cJSON_DetachItemViaPointer(rpc, params);

            pthread_mutex_lock(&fc->lock);
            cJSON_AddItemToArray(fc->new_posts, params);
            pthread_mutex_unlock(&fc->lock);
        } else if (!strcmp(jsonrpc_get_method(rpc), "new_comment")) {
            cJSON *params;
            cJSON *channel_id;
            cJSON *post_id;
            cJSON *id;
            cJSON *username;
            cJSON *comment_id;
            cJSON *content;
            cJSON *created_at;

            if (!(params = (cJSON *)jsonrpc_get_params(rpc)) ||
                !(channel_id = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
                !(post_id = cJSON_GetObjectItemCaseSensitive(params, "post_id")) ||
                !(id = cJSON_GetObjectItemCaseSensitive(params, "id")) ||
                !(comment_id = cJSON_GetObjectItemCaseSensitive(params, "comment_id")) ||
                !(username = cJSON_GetObjectItemCaseSensitive(params, "user_name")) ||
                !(content = cJSON_GetObjectItemCaseSensitive(params, "content")) ||
                !(created_at = cJSON_GetObjectItemCaseSensitive(params, "created_at")) ||
                !cJSON_IsNumber(channel_id) || !cJSON_IsNumber(post_id) ||
                !cJSON_IsNumber(id) || !(cJSON_IsNumber(comment_id) || cJSON_IsNull(comment_id)) ||
                !cJSON_IsString(username) || !username->valuestring[0] ||
                !cJSON_IsString(content) || !content->valuestring[0] ||
                !cJSON_IsNumber(created_at))
                goto finally;

            cJSON_DetachItemViaPointer(rpc, params);

            pthread_mutex_lock(&fc->lock);
            cJSON_AddItemToArray(fc->new_comments, params);
            pthread_mutex_unlock(&fc->lock);
        } else if (!strcmp(jsonrpc_get_method(rpc), "new_likes")) {
            cJSON *params;
            cJSON *channel_id;
            cJSON *post_id;
            cJSON *comment_id;
            cJSON *count;

            if (!(params = (cJSON *)jsonrpc_get_params(rpc)) ||
                !(channel_id = cJSON_GetObjectItemCaseSensitive(params, "channel_id")) ||
                !(post_id = cJSON_GetObjectItemCaseSensitive(params, "post_id")) ||
                !(comment_id = cJSON_GetObjectItemCaseSensitive(params, "comment_id")) ||
                !(count = cJSON_GetObjectItemCaseSensitive(params, "count")) ||
                !cJSON_IsNumber(channel_id) || !cJSON_IsNumber(post_id) ||
                !(cJSON_IsNumber(comment_id) || cJSON_IsNull(comment_id)) ||
                !cJSON_IsNumber(count))
                goto finally;

            cJSON_DetachItemViaPointer(rpc, params);

            pthread_mutex_lock(&fc->lock);
            cJSON_AddItemToArray(fc->new_likes, params);
            pthread_mutex_unlock(&fc->lock);
        }

        goto finally;
    }

    pthread_mutex_lock(&fc->lock);

    if (fc->waiting_response) {
        fc->response = rpc;
        fc->response_type = type;
        fc->bi = bi;
        pthread_cond_signal(&fc->cond);
        rpc = NULL;
        bi = NULL;
    }

    pthread_mutex_unlock(&fc->lock);

finally:
    if (rpc)
        cJSON_Delete(rpc);

    if (bi)
        free(bi);
}

static
void *carrier_routine(void *arg)
{
    ela_run((ElaCarrier *)arg, 10);
    return NULL;
}

static
void feeds_client_destructor(void *obj)
{
    FeedsClient *fc = (FeedsClient *)obj;

    pthread_mutex_destroy(&fc->lock);
    pthread_cond_destroy(&fc->cond);

    if (fc->carrier)
        ela_kill(fc->carrier);

    if (fc->new_posts)
        cJSON_Delete(fc->new_posts);

    if (fc->new_comments)
        cJSON_Delete(fc->new_comments);

    if (fc->new_likes)
        cJSON_Delete(fc->new_likes);

    if (fc->response)
        cJSON_Delete(fc->response);

    if (fc->bi)
        free(fc->bi);
}

FeedsClient *feeds_client_create(ElaOptions *opts)
{
    ElaCallbacks callbacks;
    FeedsClient *fc;
    int rc;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.connection_status = connection_callback;
    callbacks.friend_connection = friend_connection_callback;
    callbacks.friend_message = on_receiving_message;

    fc = rc_zalloc(sizeof(FeedsClient), feeds_client_destructor);
    if (!fc)
        return NULL;

    pthread_mutex_init(&fc->lock, NULL);
    pthread_cond_init(&fc->cond, NULL);

    fc->carrier = ela_new(opts, &callbacks, fc);
    if (!fc->carrier) {
        deref(fc);
        return NULL;
    }

    fc->new_posts = cJSON_CreateArray();
    if (!fc->new_posts) {
        deref(fc);
        return NULL;
    }

    fc->new_comments = cJSON_CreateArray();
    if (!fc->new_comments) {
        deref(fc);
        return NULL;
    }

    fc->new_likes = cJSON_CreateArray();
    if (!fc->new_likes) {
        deref(fc);
        return NULL;
    }

    rc = pthread_create(&fc->carrier_routine_tid, NULL, carrier_routine, fc->carrier);
    if (rc < 0) {
        deref(fc);
        return NULL;
    }

    return fc;
}

void feeds_client_wait_until_online(FeedsClient *fc)
{
    pthread_mutex_lock(&fc->lock);
    while (!ela_is_ready(fc->carrier))
        pthread_cond_wait(&fc->cond, &fc->lock);
    pthread_mutex_unlock(&fc->lock);
}

void feeds_client_delete(FeedsClient *fc)
{
    pthread_t tid = fc->carrier_routine_tid;

    deref(fc);
    pthread_join(tid, NULL);
}

ElaCarrier *feeds_client_get_carrier(FeedsClient *fc)
{
    return fc->carrier;
}

int feeds_client_friend_add(FeedsClient *fc, const char *address, const char *hello)
{
    char node_id[ELA_MAX_ID_LEN + 1];
    int rc = 0;

    ela_get_id_by_address(address, node_id, sizeof(node_id));
    if (!ela_is_friend(fc->carrier, node_id)) {
        rc = ela_add_friend(fc->carrier, address, hello);
        if (rc < 0)
            return -1;
    }

    feeds_client_wait_until_friend_connected(fc, node_id);

    return rc;
}

int feeds_client_wait_until_friend_connected(FeedsClient *fc, const char *friend_node_id)
{
    ElaFriendInfo info;
    int rc;

    pthread_mutex_lock(&fc->lock);
    while (1) {
        rc = ela_get_friend_info(fc->carrier, friend_node_id, &info);
        if (rc < 0)
            break;

        if (info.status != ElaConnectionStatus_Connected)
            pthread_cond_wait(&fc->cond, &fc->lock);
        else
            break;
    }
    pthread_mutex_unlock(&fc->lock);

    return rc;
}

int feeds_client_friend_remove(FeedsClient *fc, const char *user_id)
{
    return ela_remove_friend(fc->carrier, user_id);
}

int transaction_start(FeedsClient *fc, const char *svc_addr, const void *req, size_t len,
                      cJSON **resp, JsonRPCType *type, JsonRPCBatchInfo **bi)
{
    int rc;

    pthread_mutex_lock(&fc->lock);

    fc->waiting_response = true;
    rc = ela_send_friend_message(fc->carrier, svc_addr, req, len, NULL);
    if (rc < 0) {
        fc->waiting_response = false;
        pthread_mutex_unlock(&fc->lock);
        return -1;
    }

    while (!fc->response)
        pthread_cond_wait(&fc->cond, &fc->lock);

    *resp = fc->response;
    *type = fc->response_type;
    *bi = fc->bi;
    fc->response = NULL;
    fc->bi = NULL;
    fc->waiting_response = false;

    pthread_mutex_unlock(&fc->lock);

    return 0;
}

int feeds_client_create_channel(FeedsClient *fc, const char *svc_node_id, const char *name,
                                const char *intro, cJSON **resp)
{
    JsonRPCType type;
    JsonRPCBatchInfo *bi;
    cJSON *params;
    cJSON *req;
    char *req_str;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddStringToObject(params, "name", name)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "introduction", intro)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("create_channel", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_publish_post(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                              const char *content, cJSON **resp)
{
    JsonRPCType type;
    JsonRPCBatchInfo *bi;
    cJSON *params;
    cJSON *req;
    char *req_str;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "channel_id", channel_id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "content", content)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("publish_post", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_post_comment(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                              uint64_t post_id, uint64_t comment_id, const char *content, cJSON **resp)
{
    JsonRPCType type;
    JsonRPCBatchInfo *bi;
    cJSON *params;
    cJSON *req;
    char *req_str;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "channel_id", channel_id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "post_id", post_id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!(comment_id ? cJSON_AddNumberToObject(params, "comment_id", comment_id) :
                       cJSON_AddNullToObject(params, "comment_id"))) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "content", content)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("post_comment", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_post_like(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                           uint64_t post_id, uint64_t comment_id, cJSON **resp)
{
    JsonRPCType type;
    JsonRPCBatchInfo *bi;
    cJSON *params;
    cJSON *req;
    char *req_str;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "channel_id", channel_id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "post_id", post_id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!(comment_id ? cJSON_AddNumberToObject(params, "comment_id", comment_id) :
          cJSON_AddNullToObject(params, "comment_id"))) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("post_like", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_get_my_channels(FeedsClient *fc, const char *svc_node_id, QueryField qf,
                                 uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *channel;
    const cJSON *id;
    const cJSON *name;
    const cJSON *intro;
    const cJSON *subscribers;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "by", qf)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "upper_bound", upper)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "lower_bound", lower)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "max_count", maxcnt)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_my_channels", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    cJSON_ArrayForEach(channel, result) {
        if (!cJSON_IsObject(channel) ||
            !(id = cJSON_GetObjectItemCaseSensitive(channel, "id")) ||
            !(name = cJSON_GetObjectItemCaseSensitive(channel, "name")) ||
            !(intro = cJSON_GetObjectItemCaseSensitive(channel, "introduction")) ||
            !(subscribers = cJSON_GetObjectItemCaseSensitive(channel, "subscribers")) ||
            !cJSON_IsNumber(id) ||
            !cJSON_IsString(name) || !name->valuestring[0] ||
            !cJSON_IsString(intro) || !intro->valuestring[0] ||
            !cJSON_IsNumber(subscribers)) {
            cJSON_Delete(*resp);
            *resp = NULL;
            return -1;
        }
    }

    return 0;
}

int feeds_client_get_my_channels_metadata(FeedsClient *fc, const char *svc_node_id, QueryField qf,
                                 uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *channel;
    const cJSON *id;
    const cJSON *subscribers;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "by", qf)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "upper_bound", upper)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "lower_bound", lower)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "max_count", maxcnt)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_my_channels_metadata", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    cJSON_ArrayForEach(channel, result) {
        if (!cJSON_IsObject(channel) ||
            !(id = cJSON_GetObjectItemCaseSensitive(channel, "id")) ||
            !(subscribers = cJSON_GetObjectItemCaseSensitive(channel, "subscribers")) ||
            !cJSON_IsNumber(id) || !cJSON_IsNumber(subscribers)) {
            cJSON_Delete(*resp);
            *resp = NULL;
            return -1;
        }
    }

    return 0;
}

int feeds_client_get_channels(FeedsClient *fc, const char *svc_node_id, QueryField qf,
                              uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *channel;
    const cJSON *id;
    const cJSON *name;
    const cJSON *intro;
    const cJSON *owner_name;
    const cJSON *owner_did;
    const cJSON *subscribers;
    const cJSON *last_update;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "by", qf)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "upper_bound", upper)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "lower_bound", lower)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "max_count", maxcnt)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_channels", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    cJSON_ArrayForEach(channel, result) {
        if (!cJSON_IsObject(channel) ||
            !(id = cJSON_GetObjectItemCaseSensitive(channel, "id")) ||
            !(name = cJSON_GetObjectItemCaseSensitive(channel, "name")) ||
            !(intro = cJSON_GetObjectItemCaseSensitive(channel, "introduction")) ||
            !(owner_name = cJSON_GetObjectItemCaseSensitive(channel, "owner_name")) ||
            !(owner_did = cJSON_GetObjectItemCaseSensitive(channel, "owner_did")) ||
            !(subscribers = cJSON_GetObjectItemCaseSensitive(channel, "subscribers")) ||
            !(last_update = cJSON_GetObjectItemCaseSensitive(channel, "last_update")) ||
            !cJSON_IsNumber(id) ||
            !cJSON_IsString(name) || !name->valuestring[0] ||
            !cJSON_IsString(intro) || !intro->valuestring[0] ||
            !cJSON_IsString(owner_name) || !owner_name->valuestring[0] ||
            !cJSON_IsString(owner_did) || !owner_did->valuestring[0] ||
            !cJSON_IsNumber(subscribers) || !cJSON_IsNumber(last_update)) {
            cJSON_Delete(*resp);
            *resp = NULL;
            return -1;
        }
    }

    return 0;
}

int feeds_client_get_channel_detail(FeedsClient *fc, const char *svc_node_id, uint64_t id, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *cid;
    const cJSON *name;
    const cJSON *intro;
    const cJSON *owner_name;
    const cJSON *owner_did;
    const cJSON *subscribers;
    const cJSON *last_update;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "id", id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_channel_detail", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsObject(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    if (!cJSON_IsObject(result) ||
        !(cid = cJSON_GetObjectItemCaseSensitive(result, "id")) ||
        !(name = cJSON_GetObjectItemCaseSensitive(result, "name")) ||
        !(intro = cJSON_GetObjectItemCaseSensitive(result, "introduction")) ||
        !(owner_name = cJSON_GetObjectItemCaseSensitive(result, "owner_name")) ||
        !(owner_did = cJSON_GetObjectItemCaseSensitive(result, "owner_did")) ||
        !(subscribers = cJSON_GetObjectItemCaseSensitive(result, "subscribers")) ||
        !(last_update = cJSON_GetObjectItemCaseSensitive(result, "last_update")) ||
        !cJSON_IsNumber(cid) ||
        !cJSON_IsString(name) || !name->valuestring[0] ||
        !cJSON_IsString(intro) || !intro->valuestring[0] ||
        !cJSON_IsString(owner_name) || !owner_name->valuestring[0] ||
        !cJSON_IsString(owner_did) || !owner_did->valuestring[0] ||
        !cJSON_IsNumber(subscribers) || !cJSON_IsNumber(last_update)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    return 0;
}

int feeds_client_get_subscribed_channels(FeedsClient *fc, const char *svc_node_id, QueryField qf,
        uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *channel;
    const cJSON *id;
    const cJSON *name;
    const cJSON *intro;
    const cJSON *owner_name;
    const cJSON *owner_did;
    const cJSON *subscribers;
    const cJSON *last_update;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "by", qf)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "upper_bound", upper)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "lower_bound", lower)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "max_count", maxcnt)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_subscribed_channels", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    cJSON_ArrayForEach(channel, result) {
        if (!cJSON_IsObject(channel) ||
            !(id = cJSON_GetObjectItemCaseSensitive(channel, "id")) ||
            !(name = cJSON_GetObjectItemCaseSensitive(channel, "name")) ||
            !(intro = cJSON_GetObjectItemCaseSensitive(channel, "introduction")) ||
            !(owner_name = cJSON_GetObjectItemCaseSensitive(channel, "owner_name")) ||
            !(owner_did = cJSON_GetObjectItemCaseSensitive(channel, "owner_did")) ||
            !(subscribers = cJSON_GetObjectItemCaseSensitive(channel, "subscribers")) ||
            !(last_update = cJSON_GetObjectItemCaseSensitive(channel, "last_update")) ||
            !cJSON_IsNumber(id) ||
            !cJSON_IsString(name) || !name->valuestring[0] ||
            !cJSON_IsString(intro) || !intro->valuestring[0] ||
            !cJSON_IsString(owner_name) || !owner_name->valuestring[0] ||
            !cJSON_IsString(owner_did) || !owner_did->valuestring[0] ||
            !cJSON_IsNumber(subscribers) || !cJSON_IsNumber(last_update)) {
            cJSON_Delete(*resp);
            *resp = NULL;
            return -1;
        }
    }

    return 0;
}

int feeds_client_get_posts(FeedsClient *fc, const char *svc_node_id, uint64_t cid, QueryField qf,
        uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *post;
    const cJSON *channel_id;
    const cJSON *id;
    const cJSON *content;
    const cJSON *comments;
    const cJSON *likes;
    const cJSON *created_at;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "channel_id", cid)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "by", qf)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "upper_bound", upper)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "lower_bound", lower)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "max_count", maxcnt)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_posts", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    cJSON_ArrayForEach(post, result) {
        if (!cJSON_IsObject(post) ||
            !(channel_id = cJSON_GetObjectItemCaseSensitive(post, "channel_id")) ||
            !(id = cJSON_GetObjectItemCaseSensitive(post, "id")) ||
            !(content = cJSON_GetObjectItemCaseSensitive(post, "content")) ||
            !(comments = cJSON_GetObjectItemCaseSensitive(post, "comments")) ||
            !(likes = cJSON_GetObjectItemCaseSensitive(post, "likes")) ||
            !(created_at = cJSON_GetObjectItemCaseSensitive(post, "created_at")) ||
            !cJSON_IsNumber(channel_id) || !cJSON_IsNumber(id) ||
            !cJSON_IsString(content) || !content->valuestring[0] ||
            !cJSON_IsNumber(comments) || !cJSON_IsNumber(likes) || !cJSON_IsNumber(created_at)) {
            cJSON_Delete(*resp);
            *resp = NULL;
            return -1;
        }
    }

    return 0;
}

int feeds_client_get_comments(FeedsClient *fc, const char *svc_node_id, uint64_t cid, uint64_t pid,
        QueryField qf, uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *comment;
    const cJSON *channel_id;
    const cJSON *post_id;
    const cJSON *comment_id;
    const cJSON *id;
    const cJSON *username;
    const cJSON *content;
    const cJSON *likes;
    const cJSON *created_at;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "channel_id", cid)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "post_id", pid)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "by", qf)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "upper_bound", upper)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "lower_bound", lower)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddNumberToObject(params, "max_count", maxcnt)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_comments", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    cJSON_ArrayForEach(comment, result) {
        if (!cJSON_IsObject(comment) ||
            !(channel_id = cJSON_GetObjectItemCaseSensitive(comment, "channel_id")) ||
            !(post_id = cJSON_GetObjectItemCaseSensitive(comment, "post_id")) ||
            !(id = cJSON_GetObjectItemCaseSensitive(comment, "id")) ||
            !(comment_id = cJSON_GetObjectItemCaseSensitive(comment, "comment_id")) ||
            !(username = cJSON_GetObjectItemCaseSensitive(comment, "user_name")) ||
            !(content = cJSON_GetObjectItemCaseSensitive(comment, "content")) ||
            !(likes = cJSON_GetObjectItemCaseSensitive(comment, "likes")) ||
            !(created_at = cJSON_GetObjectItemCaseSensitive(comment, "created_at")) ||
            !cJSON_IsNumber(channel_id) || !cJSON_IsNumber(post_id) ||
            !cJSON_IsNumber(id) || !(cJSON_IsNumber(comment_id) || cJSON_IsNull(comment_id)) ||
            !cJSON_IsString(username) || !username->valuestring[0] ||
            !cJSON_IsString(content) || !content->valuestring[0] ||
            !cJSON_IsNumber(likes) || !cJSON_IsNumber(created_at)) {
            cJSON_Delete(*resp);
            *resp = NULL;
            return -1;
        }
    }

    return 0;
}

int feeds_client_get_statistics(FeedsClient *fc, const char *svc_node_id, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    const cJSON *result;
    const cJSON *did;
    const cJSON *connecting_clients;
    cJSON *params;
    JsonRPCType type;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("get_statistics", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsObject(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    if (!cJSON_IsObject(result) ||
        !(did = cJSON_GetObjectItemCaseSensitive(result, "did")) ||
        !(connecting_clients = cJSON_GetObjectItemCaseSensitive(result, "connecting_clients")) ||
        !cJSON_IsString(did) || !did->valuestring[0] || !cJSON_IsNumber(connecting_clients)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    return 0;
}

int feeds_client_subscribe_channel(FeedsClient *fc, const char *svc_node_id, uint64_t id, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    JsonRPCType type;
    cJSON *params;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "id", id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("subscribe_channel", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_unsubscribe_channel(FeedsClient *fc, const char *svc_node_id, uint64_t id, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    JsonRPCType type;
    cJSON *params;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddNumberToObject(params, "id", id)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("unsubscribe_channel", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_add_node_publisher(FeedsClient *fc, const char *svc_node_id, const char *did, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    JsonRPCType type;
    cJSON *params;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddStringToObject(params, "did", did)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("add_node_publisher", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_remove_node_publisher(FeedsClient *fc, const char *svc_node_id, const char *did, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    JsonRPCType type;
    cJSON *params;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddStringToObject(params, "did", did)) {
        cJSON_Delete(params);
        return -1;
    }

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("remove_node_publisher", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

int feeds_client_query_channel_creation_permission(FeedsClient *fc, const char *svc_node_id, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    JsonRPCType type;
    cJSON *params;
    const cJSON *result;
    const cJSON *authorized;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("query_channel_creation_permission", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    result = jsonrpc_get_result(*resp);
    if (!cJSON_IsObject(result)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    if (!cJSON_IsObject(result) ||
        !(authorized = cJSON_GetObjectItemCaseSensitive(result, "authorized")) ||
        !cJSON_IsBool(authorized)) {
        cJSON_Delete(*resp);
        *resp = NULL;
        return -1;
    }

    return 0;
}

int feeds_client_enable_notification(FeedsClient *fc, const char *svc_node_id, cJSON **resp)
{
    JsonRPCBatchInfo *bi;
    JsonRPCType type;
    cJSON *params;
    const cJSON *result;
    const cJSON *authorized;
    char *req_str;
    cJSON *req;
    int rc;

    *resp = NULL;

    params = cJSON_CreateObject();
    if (!params)
        return -1;

    if (!cJSON_AddStringToObject(params, "jwttoken", "abc")) {
        cJSON_Delete(params);
        return -1;
    }

    req = jsonrpc_encode_request("enable_notification", params, NULL);
    if (!req)
        return -1;

    req_str = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!req_str)
        return -1;

    rc = transaction_start(fc, svc_node_id, req_str, strlen(req_str) + 1, resp, &type, &bi);
    free(req_str);
    if (rc < 0 || type == JSONRPC_TYPE_ERROR_RESPONSE)
        return -1;

    return 0;
}

cJSON *feeds_client_get_new_posts(FeedsClient *fc)
{
    cJSON *evs = NULL;
    int size;
    int i;

    pthread_mutex_lock(&fc->lock);

    size = cJSON_GetArraySize(fc->new_posts);
    if (size) {
        evs = cJSON_CreateArray();
        if (evs) {
            for (i = 0; i < size; ++i)
                cJSON_AddItemToArray(evs,
                                     cJSON_DetachItemFromArray(fc->new_posts,
                                                                     0));
        }
    }

    pthread_mutex_unlock(&fc->lock);

    return evs;
}

cJSON *feeds_client_get_new_comments(FeedsClient *fc)
{
    cJSON *evs = NULL;
    int size;
    int i;

    pthread_mutex_lock(&fc->lock);

    size = cJSON_GetArraySize(fc->new_comments);
    if (size) {
        evs = cJSON_CreateArray();
        if (evs) {
            for (i = 0; i < size; ++i)
                cJSON_AddItemToArray(evs,
                                     cJSON_DetachItemFromArray(fc->new_comments,
                                                               0));
        }
    }

    pthread_mutex_unlock(&fc->lock);

    return evs;
}

cJSON *feeds_client_get_new_likes(FeedsClient *fc)
{
    cJSON *evs = NULL;
    int size;
    int i;

    pthread_mutex_lock(&fc->lock);

    size = cJSON_GetArraySize(fc->new_likes);
    if (size) {
        evs = cJSON_CreateArray();
        if (evs) {
            for (i = 0; i < size; ++i)
                cJSON_AddItemToArray(evs,
                                     cJSON_DetachItemFromArray(fc->new_likes,
                                                               0));
        }
    }

    pthread_mutex_unlock(&fc->lock);

    return evs;
}
