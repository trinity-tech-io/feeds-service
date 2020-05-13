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

#ifndef __DB_H__
#define __DB_H__

#include <stdbool.h>
#include <stdint.h>

#include <sqlite3.h>

#include "obj.h"

typedef struct DBObjIt DBObjIt;

int db_init(const char *db_file);
void db_deinit();
int db_create_chan(const ChanInfo *ci);
int db_add_post(const PostInfo *pi);
int db_cmt_exists(uint64_t channel_id, uint64_t post_id, uint64_t comment_id);
int db_add_cmt(CmtInfo *ci, uint64_t *id);
int db_add_like(uint64_t channel_id, uint64_t post_id, uint64_t comment_id, uint64_t *likes);
int db_add_sub(uint64_t uid, uint64_t chan_id);
int db_unsub(uint64_t uid, uint64_t chan_id);
int db_upsert_user(const UserInfo *ui, uint64_t *uid);
int db_iter_nxt(DBObjIt *it, void **obj);
DBObjIt *db_iter_chans(const QryCriteria *qc);
DBObjIt *db_iter_sub_chans(uint64_t uid, const QryCriteria *qc);
DBObjIt *db_iter_posts(uint64_t chan_id, const QryCriteria *qc);
DBObjIt *db_iter_cmts(uint64_t chan_id, uint64_t post_id, const QryCriteria *qc);
int db_is_suber(uint64_t uid, uint64_t chan_id);
int db_get_owner(UserInfo **ui);

#endif // __DB_H__
