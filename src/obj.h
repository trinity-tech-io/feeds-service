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
} ChanInfo;

typedef struct {
    uint64_t    chan_id;
    uint64_t    post_id;
    uint64_t    created_at;
    uint64_t    upd_at;
    uint64_t    cmts;
    uint64_t    likes;
    const void *content;
    size_t      len;
} PostInfo;

typedef struct {
    uint64_t    chan_id;
    uint64_t    post_id;
    uint64_t    cmt_id;
    UserInfo    user;
    uint64_t    reply_to_cmt;
    const void *content;
    size_t      len;
    uint64_t    created_at;
    uint64_t    upd_at;
    uint64_t    likes;
} CmtInfo;

#endif // __OBJ_H__
