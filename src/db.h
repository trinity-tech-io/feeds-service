#ifndef __DB_H__
#define __DB_H__

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

#include <stdbool.h>
#include <stdint.h>

#include <cjson/cJSON.h>

typedef enum {
    ID,
    LAST_UPDATE,
    CREATED_AT
} QueryField;

int db_initialize(const char *db_file);

int db_iterate_channels(int (*it)(uint64_t id, const char *name, const char *intro,
                                  uint64_t next_post_id, const char *owner));

int db_iterate_publishers(int (*it)(const char *jwt));

int db_iterate_node_owners(int (*it)(const char *jwt));

int db_create_channel(const char *name, const char *jwt, const char *intro);

int db_add_post(uint64_t channel_id, uint64_t id, const void *content,
                size_t len, uint64_t ts);

int db_comment_exists(uint64_t channel_id, uint64_t post_id, uint64_t comment_id);

int db_add_comment(uint64_t channel_id, uint64_t post_id, uint64_t comment_id,
                   const char *jwt, const void *content, size_t len,
                   uint64_t created_at, uint64_t *id);

int db_add_like(uint64_t channel_id, uint64_t post_id, uint64_t comment_id, uint64_t *likes);

int db_add_subscriber(uint64_t channel_id, const char *jwt);

int db_unsubscribe(uint64_t channel_id, const char *jwt);

int db_get_owned_channels(const char *jwt, QueryField qf, uint64_t upper,
                          uint64_t lower, uint64_t maxcnt, cJSON **result);

int db_get_owned_channels_metadata(const char *jwt, QueryField qf, uint64_t upper,
                                   uint64_t lower, uint64_t maxcnt, cJSON **result);

int db_get_channels(QueryField qf, uint64_t upper,
                    uint64_t lower, uint64_t maxcnt, cJSON **result);

int db_get_channel_detail(uint64_t id, cJSON **result);

int db_get_subscribed_channels(const char *jwt, QueryField qf, uint64_t upper,
                               uint64_t lower, uint64_t maxcnt, cJSON **result);

int db_get_posts(uint64_t channel_id, QueryField qf, uint64_t upper,
                 uint64_t lower, uint64_t maxcnt, cJSON **result);

int db_get_comments(uint64_t channel_id, uint64_t post_id, QueryField qf, uint64_t upper,
                    uint64_t lower, uint64_t maxcnt, cJSON **result);

int db_add_node_publisher(const char *jwt);

int db_remove_node_publisher(const char *jwt);

int db_add_node_owner(const char *jwt);

int db_is_subscriber(uint64_t channel_id, const char *jwt);

void db_finalize();

#endif // __DB_H__
