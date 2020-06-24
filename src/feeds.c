#include <crystal.h>

#include "feeds.h"
#include "auth.h"
#include "did.h"
#include "obj.h"
#include "err.h"
#include "db.h"

extern ElaCarrier *carrier;
extern size_t connecting_clients;

static uint64_t nxt_chan_id = CHAN_ID_START;
static hashtable_t *ass;
static hashtable_t *chans_by_name;
static hashtable_t *chans_by_id;

typedef struct {
    hash_entry_t he_name_key;
    hash_entry_t he_id_key;
    list_t *cass;
    ChanInfo info;
} Chan;

typedef struct {
    hash_entry_t he;
    hashtable_t *cass_by_chan_id;
    char node_id[ELA_MAX_ID_LEN + 1];
} ActiveSuber;

typedef struct {
    list_entry_t le;
    hash_entry_t he;
    const Chan *chan;
    const ActiveSuber *as;
} ChanActiveSuber;

#define hashtable_foreach(htab, entry)                                \
    for (hashtable_iterate((htab), &it);                              \
         hashtable_iterator_next(&it, NULL, NULL, (void **)&(entry)); \
         deref((entry)))

#define list_foreach(list, entry)                    \
    for (list_iterate((list), &it);                  \
         list_iterator_next(&it, (void **)&(entry)); \
         deref((entry)))

#define foreach_db_obj(entry) \
    for (;!(rc = db_iter_nxt(it, (void **)&entry)); deref(entry))

static inline
Chan *chan_put(Chan *chan)
{
    hashtable_put(chans_by_name, &chan->he_name_key);
    return hashtable_put(chans_by_id, &chan->he_id_key);
}

static
int channel_id_compare(const void *key1, size_t len1, const void *key2, size_t len2)
{
    assert(key1 && sizeof(uint64_t) == len1);
    assert(key2 && sizeof(uint64_t) == len2);

    return memcmp(key1, key2, sizeof(uint64_t));
}

static
void chan_dtor(void *obj)
{
    Chan *chan = obj;

    deref(chan->cass);
}

static
Chan *chan_create(const ChanInfo *ci)
{
    Chan *chan;
    void *buf;

    chan = rc_zalloc(sizeof(Chan) + strlen(ci->name) +
                     strlen(ci->intro) + 2 + ci->len, chan_dtor);
    if (!chan)
        return NULL;

    chan->cass = list_create(0, NULL);
    if (!chan->cass) {
        deref(chan);
        return NULL;
    }

    buf = chan + 1;
    chan->info        = *ci;
    chan->info.name   = strcpy(buf, ci->name);
    buf += strlen(ci->name) + 1;
    chan->info.intro  = strcpy(buf, ci->intro);
    buf += strlen(ci->intro) + 1;
    chan->info.avatar = memcpy(buf, ci->avatar, ci->len);

    chan->he_name_key.data   = chan;
    chan->he_name_key.key    = chan->info.name;
    chan->he_name_key.keylen = strlen(chan->info.name);

    chan->he_id_key.data   = chan;
    chan->he_id_key.key    = &chan->info.chan_id;
    chan->he_id_key.keylen = sizeof(chan->info.chan_id);

    return chan;
}

static
int load_chans_from_db()
{
    QryCriteria qc = {
        .by     = NONE,
        .upper  = 0,
        .lower  = 0,
        .maxcnt = 0
    };
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    it = db_iter_chans(&qc);
    if (!it) {
        vlogE("Loading channels from database failed");
        return -1;
    }

    foreach_db_obj(cinfo) {
        Chan *chan = chan_create(cinfo);
        if (!chan) {
            deref(it);
            return -1;
        }

        deref(chan_put(chan));

        if (cinfo->chan_id >= nxt_chan_id)
            nxt_chan_id = cinfo->chan_id + 1;
    }

    deref(it);
    return 0;
}


int feeds_init(FeedsConfig *cfg)
{
    int rc;

    chans_by_name = hashtable_create(8, 0, NULL, NULL);
    if (!chans_by_name) {
        vlogE("Creating channels by name failed");
        goto failure;
    }

    chans_by_id = hashtable_create(8, 0, NULL, channel_id_compare);
    if (!chans_by_id) {
        vlogE("Creating channels by id failed");
        goto failure;
    }

    ass = hashtable_create(8, 0, NULL, NULL);
    if (!ass) {
        vlogE("Creating active subscribers failed");
        goto failure;
    }

    rc = load_chans_from_db();
    if (rc < 0)
        goto failure;

    vlogI("Feeds module initialized.");
    return 0;

failure:
    feeds_deinit();
    return -1;
}

void feeds_deinit()
{
    deref(chans_by_name);
    deref(chans_by_id);
    deref(ass);
}

static
void notify_of_new_post(const ActiveSuber *as, const PostInfo *pi)
{
    NewPostNotif notif = {
        .method = "new_post",
        .params = {
            .pinfo = (PostInfo *)pi
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_new_post_notif(&notif);
    if (!notif_marshal)
        return;

    vlogI("Sending new post notification to [%s].", as->node_id);
    vlogD("  channel_id: %" PRIu64, pi->chan_id);
    vlogD("  post_id: %" PRIu64, pi->post_id);
    ela_send_friend_message(carrier, as->node_id, notif_marshal->data, notif_marshal->sz + 1, NULL);
    deref(notif_marshal);
}

static
void notify_of_new_cmt(const ActiveSuber *as, const CmtInfo *ci)
{
    NewCmtNotif notif = {
        .method = "new_comment",
        .params = {
            .cinfo = (CmtInfo *)ci
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_new_cmt_notif(&notif);
    if (!notif_marshal)
        return;

    vlogI("Sending new comment notification to [%s].", as->node_id);
    vlogD("  channel_id: %" PRIu64, ci->chan_id);
    vlogD("  post_id: %" PRIu64, ci->post_id);
    vlogD("  comment_id: %" PRIu64, ci->cmt_id);
    vlogD("  refcomment_id: %" PRIu64, ci->reply_to_cmt);
    ela_send_friend_message(carrier, as->node_id, notif_marshal->data, notif_marshal->sz + 1, NULL);
    deref(notif_marshal);
}

static
void notify_of_new_likes(const ActiveSuber *as, uint64_t cid,
                         uint64_t pid, uint64_t cmtid, uint64_t likes)
{
    NewLikesNotif notif = {
        .method = "new_likes",
        .params = {
            .chan_id = cid,
            .post_id = pid,
            .cmt_id  = cmtid,
            .cnt     = likes
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_new_likes_notif(&notif);
    if (!notif_marshal)
        return;

    vlogI("Sending new likes notification to [%s].", as->node_id);
    vlogD("  channel_id: %" PRIu64, cid);
    vlogD("  post_id: %" PRIu64, pid);
    vlogD("  comment_id: %" PRIu64, cmtid);
    ela_send_friend_message(carrier, as->node_id, notif_marshal->data, notif_marshal->sz + 1, NULL);
    deref(notif_marshal);
}

static
void as_dtor(void *obj)
{
    ActiveSuber *as = obj;

    deref(as->cass_by_chan_id);
}

static
ActiveSuber *as_create(const char *node_id)
{
    ActiveSuber *as;

    as = rc_zalloc(sizeof(ActiveSuber), as_dtor);
    if (!as)
        return NULL;

    as->cass_by_chan_id = hashtable_create(8, 0, NULL, channel_id_compare);
    if (!as->cass_by_chan_id) {
        deref(as);
        return NULL;
    }

    strcpy(as->node_id, node_id);

    as->he.data   = as;
    as->he.key    = as->node_id;
    as->he.keylen = strlen(as->node_id);

    return as;
}

static
ChanActiveSuber *cas_create(const ActiveSuber *as, const Chan *chan)
{
    ChanActiveSuber *cas;

    cas = rc_zalloc(sizeof(ChanActiveSuber), NULL);
    if (!cas)
        return NULL;

    cas->chan = chan;
    cas->as = as;

    cas->he.data = cas;
    cas->he.key = &cas->chan->info.chan_id;
    cas->he.keylen = sizeof(cas->chan->info.chan_id);

    cas->le.data = cas;

    return cas;
}

static inline
int chan_exist_by_name(const char *name)
{
    return hashtable_exist(chans_by_name, name, strlen(name));
}

static inline
Chan *chan_get_by_id(uint64_t id)
{
    return hashtable_get(chans_by_id, &id, sizeof(id));
}

static inline
int chan_exist_by_id(uint64_t id)
{
    return hashtable_exist(chans_by_id, &id, sizeof(id));
}

static inline
ActiveSuber *as_get(const char *node_id)
{
    return hashtable_get(ass, node_id, strlen(node_id));
}

static inline
int as_exist(const char *node_id)
{
    return hashtable_exist(ass, node_id, strlen(node_id));
}

static inline
ActiveSuber *as_remove(const char *node_id)
{
    return hashtable_remove(ass, node_id, strlen(node_id));
}

static inline
ActiveSuber *as_put(ActiveSuber *as)
{
    return hashtable_put(ass, &as->he);
}

static inline
ChanActiveSuber *cas_put(ChanActiveSuber *cas)
{
    list_add(cas->chan->cass, &cas->le);
    return hashtable_put(cas->as->cass_by_chan_id, &cas->he);
}

static inline
ChanActiveSuber *cas_remove(const char *node_id, Chan *chan)
{
    ChanActiveSuber *cas;
    ActiveSuber *as;

    as = as_get(node_id);
    if (!as)
        return NULL;

    cas = hashtable_remove(as->cass_by_chan_id, &chan->info.chan_id, sizeof(chan->info.chan_id));
    if (!cas) {
        deref(as);
        return NULL;
    }

    deref(list_remove_entry(chan->cass, &cas->le));
    deref(as);

    return cas;
}

void hdl_create_chan_req(ElaCarrier *c, const char *from, Req *base)
{
    CreateChanReq *req = (CreateChanReq *)base;
    ChanInfo ci = {
        .chan_id      = nxt_chan_id,
        .name         = req->params.name,
        .intro        = req->params.intro,
        .owner        = &feeds_owner_info,
        .created_at   = time(NULL),
        .upd_at       = ci.created_at,
        .subs         = 0,
        .next_post_id = POST_ID_START,
        .avatar       = req->params.avatar,
        .len          = req->params.sz
    };
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogI("Received create_channel request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  name: %s", req->params.name);
    vlogD("  introduction: %s", req->params.intro);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE("Creating channel while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (chan_exist_by_name(req->params.name)) {
        vlogE("Creating an existing channel.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ALREADY_EXISTS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_create(&ci);
    if (!chan) {
        vlogE("Creating channel failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_create_chan(&ci);
    if (rc < 0) {
        vlogE("Adding channel to database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan_put(chan);
    ++nxt_chan_id;
    vlogI("Channel [%" PRIu64 "] created.", ci.chan_id);

    {
        CreateChanResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .id = chan->info.chan_id
            }
        };
        resp_marshal = rpc_marshal_create_chan_resp(&resp);
        vlogI("Sending create_channel response.");
        vlogD("  id: %" PRIu64, ci.chan_id);
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_pub_post_req(ElaCarrier *c, const char *from, Req *base)
{
    PubPostReq *req = (PubPostReq *)base;
    Marshalled *resp_marshal = NULL;
    ChanActiveSuber *cas;
    UserInfo *uinfo = NULL;
    list_iterator_t it;
    Chan *chan = NULL;
    PostInfo new_post;
    time_t now;
    int rc;

    vlogI("Received publish_post request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id: %" PRIu64, req->params.chan_id);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE("Publishing post while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE("Publishing post on non-existent channel.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&new_post, 0, sizeof(new_post));
    new_post.chan_id    = req->params.chan_id;
    new_post.post_id    = chan->info.next_post_id;
    new_post.created_at = now = time(NULL);
    new_post.upd_at     = now;
    new_post.content    = req->params.content;
    new_post.len        = req->params.sz;

    rc = db_add_post(&new_post);
    if (rc < 0) {
        vlogE("Inserting post into database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    ++chan->info.next_post_id;
    chan->info.upd_at = now;
    vlogI("Post [%" PRIu64 "] on channel [%" PRIu64 "] created.", new_post.post_id, new_post.chan_id);

    {
        PubPostResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .id = new_post.post_id
            }
        };
        resp_marshal = rpc_marshal_pub_post_resp(&resp);
        vlogI("Sending publish_post response.");
        vlogD("  id: %" PRIu64, new_post.post_id);
    }

    list_foreach(chan->cass, cas)
        notify_of_new_post(cas->as, &new_post);

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_post_cmt_req(ElaCarrier *c, const char *from, Req *base)
{
    PostCmtReq *req = (PostCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    ChanActiveSuber *cas;
    UserInfo *uinfo = NULL;
    list_iterator_t it;
    Chan *chan = NULL;
    CmtInfo new_cmt;
    time_t now;
    int rc;

    vlogI("Received create_channel request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id: %" PRIu64, req->params.chan_id);
    vlogD("  post_id: %" PRIu64, req->params.post_id);
    vlogD("  comment_id: %" PRIu64, req->params.cmt_id);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE("Posting comment on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE("Posting comment on non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.cmt_id &&
        ((rc = db_cmt_exists(req->params.chan_id,
                             req->params.post_id,
                             req->params.cmt_id)) < 0 || !rc)) {
        vlogE("Posting comment on non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&new_cmt, 0, sizeof(new_cmt));
    new_cmt.chan_id      = req->params.chan_id;
    new_cmt.post_id      = req->params.post_id;
    new_cmt.user         = *uinfo;
    new_cmt.reply_to_cmt = req->params.cmt_id;
    new_cmt.content      = req->params.content;
    new_cmt.len          = req->params.sz;
    new_cmt.created_at   = now = time(NULL);
    new_cmt.upd_at       = now;

    rc = db_add_cmt(&new_cmt, &new_cmt.cmt_id);
    if (rc < 0) {
        vlogE("Adding comment to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan->info.upd_at = now;
    vlogI("Comment [%" PRIu64 "] on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] created.",
          new_cmt.cmt_id, new_cmt.chan_id, new_cmt.post_id, new_cmt.reply_to_cmt);

    {
        PostCmtResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .id = new_cmt.cmt_id
            }
        };
        resp_marshal = rpc_marshal_post_cmt_resp(&resp);
        vlogI("Sending post_comment response.");
        vlogD("  id: %" PRIu64, new_cmt.cmt_id);
    }

    list_foreach(chan->cass, cas)
        notify_of_new_cmt(cas->as, &new_cmt);

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_post_like_req(ElaCarrier *c, const char *from, Req *base)
{
    PostLikeReq *req = (PostLikeReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    ChanActiveSuber *cas;
    list_iterator_t it;
    Chan *chan = NULL;
    uint64_t likes;
    int rc;

    vlogI("Received post_like request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id: %" PRIu64, req->params.chan_id);
    vlogD("  post_id: %" PRIu64, req->params.post_id);
    vlogD("  comment_id: %" PRIu64, req->params.cmt_id);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE("Posting like on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_like_exists(uinfo->uid, req->params.chan_id,
                             req->params.post_id, req->params.cmt_id)) < 0 ||
        rc > 0) {
        vlogE("Posting like on liked subject");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_add_like(uinfo->uid, req->params.chan_id, req->params.post_id, req->params.cmt_id, &likes);
    if (rc < 0) {
        vlogE("Adding like to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI("Like on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] by [%s].",
          req->params.chan_id, req->params.post_id, req->params.cmt_id, uinfo->did);

    {
        PostLikeResp resp = {
            .tsx_id = req->tsx_id
        };
        resp_marshal = rpc_marshal_post_like_resp(&resp);
        vlogI("Sending post_like response.");
    }

    if (!(likes % 10))
        list_foreach(chan->cass, cas)
            notify_of_new_likes(cas->as, req->params.chan_id,
                                req->params.post_id, req->params.cmt_id, likes);

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_post_unlike_req(ElaCarrier *c, const char *from, Req *base)
{
    PostUnlikeReq *req = (PostUnlikeReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    list_iterator_t it;
    Chan *chan = NULL;
    int rc;

    vlogI("Received post_unlike request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id: %" PRIu64, req->params.chan_id);
    vlogD("  post_id: %" PRIu64, req->params.post_id);
    vlogD("  comment_id: %" PRIu64, req->params.cmt_id);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE("Posting unlike on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_like_exists(uinfo->uid, req->params.chan_id,
                             req->params.post_id, req->params.cmt_id)) < 0 ||
        !rc) {
        vlogE("Posting unlike on unliked subject");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_rm_like(uinfo->uid, req->params.chan_id, req->params.post_id, req->params.cmt_id);
    if (rc < 0) {
        vlogE("Removing like to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI("Unlike on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] by [%s].",
          req->params.chan_id, req->params.post_id, req->params.cmt_id, uinfo->did);

    {
        PostUnlikeResp resp = {
            .tsx_id = req->tsx_id
        };
        resp_marshal = rpc_marshal_post_unlike_resp(&resp);
        vlogI("Sending post_unlike response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_get_my_chans_req(ElaCarrier *c, const char *from, Req *base)
{
    GetMyChansReq *req = (GetMyChansReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogI("Received get_my_channels request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  by: %" PRIu64, req->params.qc.by);
    vlogD("  upper_bound: %" PRIu64, req->params.qc.upper);
    vlogD("  lower_bound: %" PRIu64, req->params.qc.lower);
    vlogD("  max_count: %" PRIu64, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE("Getting owned channels while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_chans(&req->params.qc);
    if (!it) {
        vlogE("Getting owned channels from database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD("channel_id: %" PRIu64, cinfo->chan_id);
        vlogD("name: %s", cinfo->name);
        vlogD("introduction: %s", cinfo->intro);
        vlogD("subscribers: %" PRIu64, cinfo->subs);
    }
    if (rc < 0) {
        vlogE("Iterating owned channels failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetMyChansResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .cinfos = cinfos
            }
        };
        resp_marshal = rpc_marshal_get_my_chans_resp(&resp);
        vlogI("Sending get_my_channels response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (cinfos) {
        ChanInfo **i;
        cvector_foreach(cinfos, i)
            deref(*i);
        cvector_free(cinfos);
    }
    deref(uinfo);
    deref(it);
}

void hdl_get_my_chans_meta_req(ElaCarrier *c, const char *from, Req *base)
{
    GetMyChansMetaReq *req = (GetMyChansMetaReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogI("Received get_my_channels_metadata request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  by: %" PRIu64, req->params.qc.by);
    vlogD("  upper_bound: %" PRIu64, req->params.qc.upper);
    vlogD("  lower_bound: %" PRIu64, req->params.qc.lower);
    vlogD("  max_count: %" PRIu64, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE("Getting owned channels metadata while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_chans(&req->params.qc);
    if (!it) {
        vlogE("Getting owned channels metadata from database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD("channel_id: %" PRIu64, cinfo->chan_id);
        vlogD("subscribers: %" PRIu64, cinfo->subs);
    }
    if (rc < 0) {
        vlogE("Iterating owned channels metadata failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetMyChansMetaResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .cinfos = cinfos
            }
        };
        resp_marshal = rpc_marshal_get_my_chans_meta_resp(&resp);
        vlogI("Sending get_my_channels_metadata response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (cinfos) {
        ChanInfo **i;
        cvector_foreach(cinfos, i)
            deref(*i);
        cvector_free(cinfos);
    }
    deref(uinfo);
    deref(it);
}

void hdl_get_chans_req(ElaCarrier *c, const char *from, Req *base)
{
    GetChansReq *req = (GetChansReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogI("Received get_channels request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  by: %" PRIu64, req->params.qc.by);
    vlogD("  upper_bound: %" PRIu64, req->params.qc.upper);
    vlogD("  lower_bound: %" PRIu64, req->params.qc.lower);
    vlogD("  max_count: %" PRIu64, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_chans(&req->params.qc);
    if (!it) {
        vlogE("Getting channels from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD("channel_id: %" PRIu64, cinfo->chan_id);
        vlogD("name: %s", cinfo->name);
        vlogD("introduction: %s", cinfo->intro);
        vlogD("owner_name: %s", cinfo->owner->name);
        vlogD("owner_did: %s", cinfo->owner->did);
        vlogD("subscribers: %" PRIu64, cinfo->subs);
        vlogD("last_update: %" PRIu64, cinfo->upd_at);
    }
    if (rc < 0) {
        vlogE("Iterating channels failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetChansResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .cinfos = cinfos
            }
        };
        resp_marshal = rpc_marshal_get_chans_resp(&resp);
        vlogI("Sending get_channels response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (cinfos) {
        ChanInfo **i;
        cvector_foreach(cinfos, i)
            deref(*i);
        cvector_free(cinfos);
    }
    deref(uinfo);
    deref(it);
}

void hdl_get_chan_dtl_req(ElaCarrier *c, const char *from, Req *base)
{
    GetChanDtlReq *req = (GetChanDtlReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogI("Received get_channel_detail request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  id: %" PRIu64, req->params.id);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!(chan = chan_get_by_id(req->params.id))) {
        vlogE("Getting detail on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetChanDtlResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .cinfo = &chan->info
            }
        };
        resp_marshal = rpc_marshal_get_chan_dtl_resp(&resp);
        vlogI("Sending get_channel_detail response.");
        vlogD("  channel_id: %" PRIu64, chan->info.chan_id);
        vlogD("  name: %s", chan->info.name);
        vlogD("  introduction: %s", chan->info.intro);
        vlogD("  owner_name: %s", chan->info.owner->name);
        vlogD("  owner_did: %s", chan->info.owner->did);
        vlogD("  subscribers: %" PRIu64, chan->info.subs);
        vlogD("  last_update: %" PRIu64, chan->info.upd_at);
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_get_sub_chans_req(ElaCarrier *c, const char *from, Req *base)
{
    GetSubChansReq *req = (GetSubChansReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogI("Received get_subscribed_channels request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  by: %" PRIu64, req->params.qc.by);
    vlogD("  upper_bound: %" PRIu64, req->params.qc.upper);
    vlogD("  lower_bound: %" PRIu64, req->params.qc.lower);
    vlogD("  max_count: %" PRIu64, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_sub_chans(uinfo->uid, &req->params.qc);
    if (!it) {
        vlogE("Getting subscribed channels from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD("  channel_id: %" PRIu64, cinfo->chan_id);
        vlogD("  name: %s", cinfo->name);
        vlogD("  introduction: %s", cinfo->intro);
        vlogD("  owner_name: %s", cinfo->owner->name);
        vlogD("  owner_did: %s", cinfo->owner->did);
        vlogD("  subscribers: %" PRIu64, cinfo->subs);
        vlogD("  last_update: %" PRIu64, cinfo->upd_at);
    }
    if (rc < 0) {
        vlogE("Iterating subscribed channels failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetSubChansResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .cinfos = cinfos
            }
        };
        resp_marshal = rpc_marshal_get_sub_chans_resp(&resp);
        vlogI("Sending get_subscribed_channels response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (cinfos) {
        ChanInfo **i;
        cvector_foreach(cinfos, i)
            deref(*i);
        cvector_free(cinfos);
    }
    deref(uinfo);
    deref(it);
}

void hdl_get_posts_req(ElaCarrier *c, const char *from, Req *base)
{
    GetPostsReq *req = (GetPostsReq *)base;
    cvector_vector_type(PostInfo *) pinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    PostInfo *pinfo;
    int rc;

    vlogI("Received get_posts request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id: %" PRIu64, req->params.chan_id);
    vlogD("  by: %" PRIu64, req->params.qc.by);
    vlogD("  upper_bound: %" PRIu64, req->params.qc.upper);
    vlogD("  lower_bound: %" PRIu64, req->params.qc.lower);
    vlogD("  max_count: %" PRIu64, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!chan_exist_by_id(req->params.chan_id)) {
        vlogE("Getting posts from non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_posts(req->params.chan_id, &req->params.qc);
    if (!it) {
        vlogE("Getting posts from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(pinfo) {
        cvector_push_back(pinfos, ref(pinfo));
        vlogD("channel_id: %" PRIu64, pinfo->chan_id);
        vlogD("post_id: %" PRIu64, pinfo->post_id);
        vlogD("comments: %" PRIu64, pinfo->cmts);
        vlogD("likes: %" PRIu64, pinfo->likes);
        vlogD("created_at: %" PRIu64, pinfo->created_at);
    }
    if (rc < 0) {
        vlogE("Iterating posts failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetPostsResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .pinfos = pinfos
            }
        };
        resp_marshal = rpc_marshal_get_posts_resp(&resp);
        vlogI("Sending get_posts response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (pinfos) {
        PostInfo **i;
        cvector_foreach(pinfos, i)
            deref(*i);
        cvector_free(pinfos);
    }
    deref(uinfo);
    deref(it);
}

void hdl_get_liked_posts_req(ElaCarrier *c, const char *from, Req *base)
{
    GetLikedPostsReq *req = (GetLikedPostsReq *)base;
    cvector_vector_type(PostInfo *) pinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    PostInfo *pinfo;
    int rc;

    vlogI("Received get_liked_posts request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  by: %" PRIu64, req->params.qc.by);
    vlogD("  upper_bound: %" PRIu64, req->params.qc.upper);
    vlogD("  lower_bound: %" PRIu64, req->params.qc.lower);
    vlogD("  max_count: %" PRIu64, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_liked_posts(uinfo->uid, &req->params.qc);
    if (!it) {
        vlogE("Getting liked posts from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(pinfo) {
        cvector_push_back(pinfos, ref(pinfo));
        vlogD("channel_id: %" PRIu64, pinfo->chan_id);
        vlogD("post_id: %" PRIu64, pinfo->post_id);
        vlogD("comments: %" PRIu64, pinfo->cmts);
        vlogD("likes: %" PRIu64, pinfo->likes);
        vlogD("created_at: %" PRIu64, pinfo->created_at);
    }
    if (rc < 0) {
        vlogE("Iterating posts failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetLikedPostsResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .pinfos = pinfos
            }
        };
        resp_marshal = rpc_marshal_get_liked_posts_resp(&resp);
        vlogI("Sending get_liked_posts response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (pinfos) {
        PostInfo **i;
        cvector_foreach(pinfos, i)
            deref(*i);
        cvector_free(pinfos);
    }
    deref(uinfo);
    deref(it);
}

void hdl_get_cmts_req(ElaCarrier *c, const char *from, Req *base)
{
    GetCmtsReq *req = (GetCmtsReq *)base;
    cvector_vector_type(CmtInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    Chan *chan = NULL;
    CmtInfo *cinfo;
    int rc;

    vlogI("Received get_comments request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id: %" PRIu64, req->params.chan_id);
    vlogD("  post_id: %" PRIu64, req->params.post_id);
    vlogD("  by: %" PRIu64, req->params.qc.by);
    vlogD("  upper_bound: %" PRIu64, req->params.qc.upper);
    vlogD("  lower_bound: %" PRIu64, req->params.qc.lower);
    vlogD("  max_count: %" PRIu64, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!(chan = chan_get_by_id(req->params.chan_id))) {
        vlogE("Getting comments from non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE("Getting comment from non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_cmts(req->params.chan_id, req->params.post_id, &req->params.qc);
    if (!it) {
        vlogE("Getting comments from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD("channel_id: %" PRIu64, cinfo->chan_id);
        vlogD("post_id: %" PRIu64, cinfo->post_id);
        vlogD("comment_id: %" PRIu64, cinfo->cmt_id);
        vlogD("refcomment_id: %" PRIu64, cinfo->reply_to_cmt);
        vlogD("user_name: %" PRIu64, cinfo->user.name);
        vlogD("likes: %" PRIu64, cinfo->likes);
        vlogD("created_at: %" PRIu64, cinfo->created_at);
    }
    if (rc < 0) {
        vlogE("Iterating comments failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetCmtsResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .cinfos = cinfos
            }
        };
        resp_marshal = rpc_marshal_get_cmts_resp(&resp);
        vlogI("Sending get_comments response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (cinfos) {
        CmtInfo **i;
        cvector_foreach(cinfos, i)
            deref(*i);
        cvector_free(cinfos);
    }
    deref(uinfo);
    deref(chan);
    deref(it);
}

void hdl_get_stats_req(ElaCarrier *c, const char *from, Req *base)
{
    GetStatsReq *req = (GetStatsReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    int rc;

    vlogI("Received get_statistics request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetStatsResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .did     = feeds_owner_info.did,
                .conn_cs = connecting_clients
            }
        };
        resp_marshal = rpc_marshal_get_stats_resp(&resp);
        vlogI("Sending get_statistics response.");
        vlogD("  did: %s", feeds_owner_info.did);
        vlogD("  connecting_clients: %zu", connecting_clients);
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
}

void hdl_sub_chan_req(ElaCarrier *c, const char *from, Req *base)
{
    SubChanReq *req = (SubChanReq *)base;
    ChanActiveSuber *cas = NULL;
    Marshalled *resp_marshal = NULL;
    ActiveSuber *as = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogI("Received subscribe_channel request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id: %" PRIu64, req->params.id);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.id);
    if (!chan) {
        vlogE("Subscribing non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_is_suber(uinfo->uid, req->params.id)) < 0 || rc) {
        vlogE("Subscribing subscribed channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    as = as_get(from);
    if (as) {
        cas = cas_create(as, chan);
        if (!cas) {
            vlogE("Creating channel active subscriber failed.");
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }
    }

    rc = db_add_sub(uinfo->uid, req->params.id);
    if (rc < 0) {
        vlogE("Adding subscription to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (cas)
        cas_put(cas);

    ++chan->info.subs;
    vlogI("[%s] subscribed to channel [%" PRIu64 "]", uinfo->did, req->params.id);

    {
        SubChanResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_sub_chan_resp(&resp);
        vlogD("Sending subscribe_channel response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
    deref(cas);
    deref(as);
}

void hdl_unsub_chan_req(ElaCarrier *c, const char *from, Req *base)
{
    UnsubChanReq *req = (UnsubChanReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogI("Received unsubscribe_channel request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);
    vlogD("  channel_id%" PRIu64, req->params.id);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.id);
    if (!chan) {
        vlogE("Unsubscribing non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_is_suber(uinfo->uid, req->params.id)) < 0 || !rc) {
        vlogE("Unsubscribing non-existent subscription");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_unsub(uinfo->uid, req->params.id);
    if (rc < 0) {
        vlogE("Removing subscription from database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    deref(cas_remove(from, chan));
    --chan->info.subs;
    vlogI("[%s] unsubscribed channel [%" PRIu64 "]", uinfo->did, req->params.id);

    {
        UnsubChanResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_unsub_chan_resp(&resp);
        vlogI("Sending unsubscribe_channel response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    deref(chan);
    deref(uinfo);
}

void hdl_enbl_notif_req(ElaCarrier *c, const char *from, Req *base)
{
    EnblNotifReq *req = (EnblNotifReq *)base;
    cvector_vector_type(ChanActiveSuber *) cass = NULL;
    Marshalled *resp_marshal = NULL;
    ActiveSuber *as = NULL;
    UserInfo *uinfo = NULL;
    ChanActiveSuber **i;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    QryCriteria qc = {
        .by     = NONE,
        .upper  = 0,
        .lower  = 0,
        .maxcnt = 0
    };
    int rc;

    vlogI("Received enable_notification request from [%s].", from);
    vlogD("  access_token: %s", req->params.tk);

    if (!did_is_ready()) {
        vlogE("Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE("Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (as_exist(from)) {
        vlogE("Already enabled notification");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    as = as_create(from);
    if (!as) {
        vlogE("Creating active subscriber failed.");
        rc = ERR_INTERNAL_ERROR;
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_sub_chans(uinfo->uid, &qc);
    if (!it) {
        vlogE("Getting subscribed channels from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        Chan *chan = chan_get_by_id(cinfo->chan_id);
        ChanActiveSuber *cas = cas_create(as, chan);
        vlogD("Enabling notification of channel [%" PRIu64 "] for [%s]", chan->info.chan_id, uinfo->did);
        deref(chan);
        if (!cas) {
            vlogE("Creating channel active subscriber failed.");
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }
        cvector_push_back(cass, cas);
    }
    if (rc < 0) {
        vlogE("Iterating subscribed channels failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    cvector_foreach(cass, i)
        cas_put(*i);

    as_put(as);

    {
        EnblNotifResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_enbl_notif_resp(&resp);
        vlogI("Sending enable_notification response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (cass) {
        cvector_foreach(cass, i)
            deref(*i);
        cvector_free(cass);
    }
    deref(as);
    deref(uinfo);
    deref(it);
}

void feeds_deactivate_suber(const char *node_id)
{
    ActiveSuber *as = as_remove(node_id);
    hashtable_iterator_t it;
    ChanActiveSuber *cas;

    if (!as)
        return;

    hashtable_foreach(as->cass_by_chan_id, cas)
        deref(list_remove_entry(cas->chan->cass, &cas->le));

    deref(as);
}
