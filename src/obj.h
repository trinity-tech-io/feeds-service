/*
 * Copyright (c) 2020 trinity-tech
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

#ifndef __OBJ_H__
#define __OBJ_H__

#include <stdint.h>

#define CHAN_ID_START 1
#define POST_ID_START 1
#define CMT_ID_START  1
#define USER_ID_START 1
#define OWNER_USER_ID 1

#define chan_id_is_valid(id) ((uint64_t)(id) >= CHAN_ID_START)
#define post_id_is_valid(id) ((uint64_t)(id) >= POST_ID_START)
#define cmt_id_is_valid(id)  ((uint64_t)(id) >= CMT_ID_START)
#define user_id_is_owner(id) ((uint64_t)(id) == USER_ID_START)

typedef enum {
    NONE,
    ID,
    UPD_AT,
    CREATED_AT
} QryFld;

#define qry_fld_is_valid(qf) ((uint64_t)(qf) <= CREATED_AT)

typedef struct {
    uint64_t by;
    uint64_t upper;
    uint64_t lower;
    uint64_t maxcnt;
} QryCriteria;

typedef struct {
    uint64_t uid;
    char    *did;
    char    *name;
    char    *email;
    char    *display_name;  //v2.0
    uint64_t upd_at;  //v2.0
    char    *memo;  //v2.0
    void    *avatar;  //v2.0
    size_t   len;  //v2.0
} UserInfo;

typedef struct {
    uint64_t    chan_id;
    const char *name;
    const char *intro;
    UserInfo   *owner;
    uint64_t    created_at;
    uint64_t    upd_at;
    uint64_t    subs;
    uint64_t    next_post_id;
    void       *avatar;
    size_t      len;
    const char *tip_methods;  //v2.0
    const char *proof;  //v2.0
    uint64_t    status;  //2.0
} ChanInfo;

typedef enum {
    POST_AVAILABLE,
    POST_DELETED,
    POST_DECLARED,
    POST_WRONG_STAT
} PostStat;

static inline
const char *post_stat_str(PostStat stat)
{
    assert(stat < POST_WRONG_STAT);

    switch (stat) {
    case POST_AVAILABLE:
        return "available";
    case POST_DELETED:
        return "deleted";
    case POST_DECLARED:
        return "declared";
    default:
        abort();
    }
}

typedef struct {
    uint64_t    chan_id;
    uint64_t    post_id;
    PostStat    stat;
    uint64_t    created_at;
    uint64_t    upd_at;
    uint64_t    cmts;
    uint64_t    likes;
    void       *content;
    size_t      con_len;
    void       *thumbnails;  //2.0
    size_t      thu_len;  //2.0
    const char *hash_id;  //2.0
    const char *proof;  //2.0
    const char *origin_post_url;  //2.0
} PostInfo;

typedef enum {
    CMT_AVAILABLE,
    CMT_DELETED,
    CMT_BLOCKED,
    CMT_WRONG_STAT
} CmtStat;

static inline
const char *cmt_stat_str(CmtStat stat)
{
    assert(stat < CMT_WRONG_STAT);

    switch (stat) {
    case CMT_AVAILABLE:
        return "available";
    case CMT_DELETED:
        return "deleted";
    default:
        abort();
    }
}

typedef struct {
    uint64_t    chan_id;
    uint64_t    post_id;
    uint64_t    cmt_id;
    CmtStat     stat;
    UserInfo    user;
    uint64_t    reply_to_cmt;
    const void *content;
    size_t      con_len;
    uint64_t    created_at;
    uint64_t    upd_at;
    uint64_t    likes;
    void       *thumbnails;  //2.0
    size_t      thu_len;  //2.0
    const char *hash_id;  //2.0
    const char *proof;  //2.0
} CmtInfo;

typedef struct {
    uint64_t chan_id;
    uint64_t post_id;
    uint64_t cmt_id;
    UserInfo user;
    uint64_t total_cnt;
    uint64_t created_at;  //add for read only in 2.0
    const char *proof;  //2.0
} LikeInfo;

typedef struct {
    uint64_t    chan_id;
    uint64_t    post_id;
    uint64_t    cmt_id;
    UserInfo    reporter;
    const char *reasons;
    uint64_t    created_at;
} ReportedCmtInfo;

#endif // __OBJ_H__
