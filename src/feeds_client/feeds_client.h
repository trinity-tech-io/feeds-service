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

#ifndef __FEEDS_CLIENT_H__
#define __FEEDS_CLIENT_H__

#include <ela_carrier.h>
#include <cjson/cJSON.h>

#include "jsonrpc.h"

typedef struct FeedsClient FeedsClient;

FeedsClient *feeds_client_create(ElaOptions *opts);

void feeds_client_wait_until_online(FeedsClient *fc);

void feeds_client_delete(FeedsClient *fc);

ElaCarrier *feeds_client_get_carrier(FeedsClient *fc);

int feeds_client_friend_add(FeedsClient *fc, const char *address, const char *hello);

int feeds_client_wait_until_friend_connected(FeedsClient *fc, const char *friend_node_id);

int feeds_client_friend_remove(FeedsClient *fc, const char *user_id);

int feeds_client_create_channel(FeedsClient *fc, const char *svc_node_id, const char *name,
                                const char *intro, cJSON **resp);

int feeds_client_publish_post(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                              const char *content, cJSON **resp);

int feeds_client_post_comment(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                              uint64_t post_id, uint64_t comment_id, const char *content, cJSON **resp);

int feeds_client_post_like(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                           uint64_t post_id, uint64_t comment_id, cJSON **resp);

typedef enum {
    ID,
    LAST_UPDATE,
    CREATED_AT
} QueryField;
int feeds_client_get_my_channels(FeedsClient *fc, const char *svc_node_id, QueryField qf,
        uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp);

int feeds_client_get_my_channels_metadata(FeedsClient *fc, const char *svc_node_id, QueryField qf,
                                          uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp);

int feeds_client_get_channels(FeedsClient *fc, const char *svc_node_id, QueryField qf,
                              uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp);

int feeds_client_get_channel_detail(FeedsClient *fc, const char *svc_node_id, uint64_t id, cJSON **resp);

int feeds_client_get_subscribed_channels(FeedsClient *fc, const char *svc_node_id, QueryField qf,
                                         uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp);

int feeds_client_get_posts(FeedsClient *fc, const char *svc_node_id, uint64_t cid, QueryField qf,
                           uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp);

int feeds_client_get_comments(FeedsClient *fc, const char *svc_node_id, uint64_t cid, uint64_t pid,
                              QueryField qf, uint64_t upper, uint64_t lower, uint64_t maxcnt, cJSON **resp);

int feeds_client_get_statistics(FeedsClient *fc, const char *svc_node_id, cJSON **resp);

int feeds_client_subscribe_channel(FeedsClient *fc, const char *svc_node_id, uint64_t id, cJSON **resp);

int feeds_client_unsubscribe_channel(FeedsClient *fc, const char *svc_node_id, uint64_t id, cJSON **resp);

int feeds_client_add_node_publisher(FeedsClient *fc, const char *svc_node_id, const char *did, cJSON **resp);

int feeds_client_remove_node_publisher(FeedsClient *fc, const char *svc_node_id, const char *did, cJSON **resp);

int feeds_client_query_channel_creation_permission(FeedsClient *fc, const char *svc_node_id, cJSON **resp);

int feeds_client_enable_notification(FeedsClient *fc, const char *svc_node_id, cJSON **resp);

cJSON *feeds_client_get_new_posts(FeedsClient *fc);

cJSON *feeds_client_get_new_comments(FeedsClient *fc);

cJSON *feeds_client_get_new_likes(FeedsClient *fc);

int transaction_start(FeedsClient *fc, const char *svc_addr, const void *req, size_t len,
                      cJSON **resp, JsonRPCType *type, JsonRPCBatchInfo **bi);

#endif // __FEEDS_CLIENT_H__
