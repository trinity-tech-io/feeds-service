#include <crystal.h>

#include "feeds.h"
#include "msgq.h"
#include "auth.h"
#include "did.h"
#include "obj.h"
#include "err.h"
#include "db.h"

extern ElaCarrier *carrier;
extern size_t connecting_clients;

typedef struct {
    hash_entry_t he_name_key;
    hash_entry_t he_id_key;
    list_t *aspcs;
    ChanInfo info;
} Chan;

typedef struct {
    hash_entry_t he;
    hashtable_t *aspcs;
    hashtable_t *ndpass;
    uint64_t uid;
} ActiveSuber;

typedef struct {
    list_entry_t le;
    hash_entry_t he;
    const Chan *chan;
    const ActiveSuber *as;
} ActiveSuberPerChan;

typedef struct {
    hash_entry_t he;
    char node_id[ELA_MAX_ID_LEN + 1];
    list_t *ndpass;
} NotifDest;

typedef struct {
    hash_entry_t he;
    list_entry_t le;
    ActiveSuber *as;
    NotifDest *nd;
} NotifDestPerActiveSuber;

static uint64_t nxt_chan_id = CHAN_ID_START;
static hashtable_t *ass;
static hashtable_t *nds;
static hashtable_t *chans_by_name;
static hashtable_t *chans_by_id;

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
int u64_cmp(const void *key1, size_t len1, const void *key2, size_t len2)
{
    assert(key1 && sizeof(uint64_t) == len1);
    assert(key2 && sizeof(uint64_t) == len2);

    return memcmp(key1, key2, sizeof(uint64_t));
}

static
void chan_dtor(void *obj)
{
    Chan *chan = obj;

    deref(chan->aspcs);
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

    chan->aspcs = list_create(0, NULL);
    if (!chan->aspcs) {
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

    chans_by_id = hashtable_create(8, 0, NULL, u64_cmp);
    if (!chans_by_id) {
        vlogE("Creating channels by id failed");
        goto failure;
    }

    ass = hashtable_create(8, 0, NULL, u64_cmp);
    if (!ass) {
        vlogE("Creating active subscribers failed");
        goto failure;
    }

    nds = hashtable_create(8, 0, NULL, NULL);
    if (!nds) {
        vlogE("Creating notification destinations failed");
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
    deref(nds);
}

static
void notify_of_new_post(const char *peer, const PostInfo *pi)
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

    vlogD("Sending new post notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64 "}",
          peer, pi->chan_id, pi->post_id);
    msgq_enq(peer, notif_marshal);
}

static
void notify_of_new_cmt(const char *peer, const CmtInfo *ci)
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

    vlogD("Sending new comment notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64
          ", comment_id: %" PRIu64 ", refcomment_id: %" PRIu64 "}",
          peer, ci->chan_id, ci->post_id, ci->cmt_id, ci->reply_to_cmt);
    msgq_enq(peer, notif_marshal);
}

static
void notify_of_new_like(const char *peer, const LikeInfo *li)
{
    NewLikeNotif notif = {
        .method = "new_like",
        .params = {
            .li = (LikeInfo *)li
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_new_like_notif(&notif);
    if (!notif_marshal)
        return;

    vlogD("Sending new like notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64
          ", comment_id: %" PRIu64 ", user_name: %s, user_did: %s, total_count: " PRIu64 "}",
          peer, li->chan_id, li->post_id, li->cmt_id, li->user.name, li->user.did, li->total_cnt);
    msgq_enq(peer, notif_marshal);
}

static
void notify_of_new_sub(const char *peer, const uint64_t chan_id, const UserInfo *uinfo)
{
    NewSubNotif notif = {
        .method = "new_subscription",
        .params = {
            .chan_id = chan_id,
            .uinfo   = (UserInfo *)uinfo
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_new_sub_notif(&notif);
    if (!notif_marshal)
        return;

    vlogD("Sending new subscription notification to [%s]: "
          "{channel_id: %" PRIu64 ", user_name: %s, user_did: %s}",
          peer, chan_id, uinfo->name, uinfo->did);
    msgq_enq(peer, notif_marshal);
}

static
void as_dtor(void *obj)
{
    ActiveSuber *as = obj;

    deref(as->aspcs);
    deref(as->ndpass);
}

static
ActiveSuber *as_create(uint64_t uid)
{
    ActiveSuber *as;

    as = rc_zalloc(sizeof(ActiveSuber), as_dtor);
    if (!as)
        return NULL;

    as->aspcs = hashtable_create(8, 0, NULL, u64_cmp);
    if (!as->aspcs) {
        deref(as);
        return NULL;
    }

    as->ndpass = hashtable_create(8, 0, NULL, NULL);
    if (!as->ndpass) {
        deref(as);
        return NULL;
    }

    as->uid       = uid;
    as->he.data   = as;
    as->he.key    = &as->uid;
    as->he.keylen = sizeof(as->uid);

    return as;
}

static
ActiveSuberPerChan *aspc_create(const ActiveSuber *as, const Chan *chan)
{
    ActiveSuberPerChan *aspc;

    aspc = rc_zalloc(sizeof(ActiveSuberPerChan), NULL);
    if (!aspc)
        return NULL;

    aspc->chan = chan;
    aspc->as   = as;

    aspc->he.data = aspc;
    aspc->he.key = &aspc->chan->info.chan_id;
    aspc->he.keylen = sizeof(aspc->chan->info.chan_id);

    aspc->le.data = aspc;

    return aspc;
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
ActiveSuber *as_get(uint64_t uid)
{
    return hashtable_get(ass, &uid, sizeof(uid));
}

static inline
ActiveSuber *as_remove(uint64_t uid)
{
    return hashtable_remove(ass, &uid, sizeof(uid));
}

static inline
ActiveSuber *as_put(ActiveSuber *as)
{
    return hashtable_put(ass, &as->he);
}

static inline
NotifDest *nd_get(const char *node_id)
{
    return hashtable_get(nds, node_id, strlen(node_id));
}

static inline
NotifDest *nd_put(NotifDest *nd)
{
    return hashtable_put(nds, &nd->he);
}

static inline
NotifDest *nd_remove(const char *node_id)
{
    return hashtable_remove(nds, node_id, strlen(node_id));
}

static inline
int ndpas_exist(ActiveSuber *as, const char *node_id)
{
    return hashtable_exist(as->ndpass, node_id, strlen(node_id));
}

static inline
NotifDestPerActiveSuber *ndpas_put(NotifDestPerActiveSuber *ndpas)
{
    list_add(ndpas->nd->ndpass, &ndpas->le);
    return hashtable_put(ndpas->as->ndpass, &ndpas->he);
}

static inline
ActiveSuberPerChan *aspc_put(ActiveSuberPerChan *aspc)
{
    list_add(aspc->chan->aspcs, &aspc->le);
    return hashtable_put(aspc->as->aspcs, &aspc->he);
}

static inline
ActiveSuberPerChan *aspc_remove(uint64_t uid, Chan *chan)
{
    ActiveSuberPerChan *aspc;
    ActiveSuber *as;

    as = as_get(uid);
    if (!as)
        return NULL;

    aspc = hashtable_remove(as->aspcs, &chan->info.chan_id, sizeof(chan->info.chan_id));
    if (!aspc) {
        deref(as);
        return NULL;
    }

    deref(list_remove_entry(chan->aspcs, &aspc->le));
    deref(as);

    return aspc;
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

    vlogD("Received create_channel request from [%s]: "
          "{access_token: %s, name: %s, introduction: %s, avatar_length: %" PRIu64 "}",
          from, req->params.tk, req->params.name, req->params.intro, req->params.sz);

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
        vlogD("Sending create_channel response: "
              "{id: %" PRIu64 "}", ci.chan_id);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(uinfo);
    deref(chan);
}

void hdl_pub_post_req(ElaCarrier *c, const char *from, Req *base)
{
    PubPostReq *req = (PubPostReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    list_iterator_t it;
    Chan *chan = NULL;
    PostInfo new_post;
    time_t now;
    int rc;

    vlogD("Received publish_post request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", content_length: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.sz);

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
        vlogD("Sending publish_post response: "
              "{id: %" PRIu64 "}", new_post.post_id);
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_new_post(ndpas->nd->node_id, &new_post);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(uinfo);
    deref(chan);
}

void hdl_post_cmt_req(ElaCarrier *c, const char *from, Req *base)
{
    PostCmtReq *req = (PostCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    list_iterator_t it;
    Chan *chan = NULL;
    CmtInfo new_cmt;
    time_t now;
    int rc;

    vlogD("Received post_comment request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 ", content_length: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id, req->params.sz);

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
        vlogD("Sending post_comment response: {id: %" PRIu64 "}", new_cmt.cmt_id);
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_new_cmt(ndpas->nd->node_id, &new_cmt);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(uinfo);
    deref(chan);
}

void hdl_post_like_req(ElaCarrier *c, const char *from, Req *base)
{
    PostLikeReq *req = (PostLikeReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    ActiveSuberPerChan *aspc;
    list_iterator_t it;
    Chan *chan = NULL;
    LikeInfo li;
    int rc;

    vlogD("Received post_like request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id);

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

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE("Posting like on non-existent post");
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
        vlogE("Posting like on non-existent comment");
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

    rc = db_add_like(uinfo->uid, req->params.chan_id, req->params.post_id, req->params.cmt_id, &li.total_cnt);
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
        vlogD("Sending post_like response.");
    }

    li.chan_id = req->params.chan_id;
    li.post_id = req->params.post_id;
    li.cmt_id  = req->params.cmt_id;
    li.user    = *uinfo;

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_new_like(ndpas->nd->node_id, &li);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(uinfo);
    deref(chan);
}

void hdl_post_unlike_req(ElaCarrier *c, const char *from, Req *base)
{
    PostUnlikeReq *req = (PostUnlikeReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogD("Received post_unlike request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id);

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
        vlogD("Sending post_unlike response.");
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(uinfo);
    deref(chan);
}

#define MAX_CONTENT_LEN (ELA_MAX_APP_BULKMSG_LEN - 100 * 1024)
void hdl_get_my_chans_req(ElaCarrier *c, const char *from, Req *base)
{
    GetMyChansReq *req = (GetMyChansReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogD("Received get_my_channels request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

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
        vlogD("Retrieved channel: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, subscribers: %" PRIu64
              ", avatar_length: %" PRIu64 "}",
              cinfo->chan_id, cinfo->name, cinfo->intro, cinfo->subs, cinfo->len);
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
        size_t left = MAX_CONTENT_LEN;
        cvector_vector_type(ChanInfo *) cinfos_tmp = NULL;
        int i;

        if (!cvector_size(cinfos)) {
            GetMyChansResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = true,
                    .cinfos  = cinfos
                }
            };
            resp_marshal = rpc_marshal_get_my_chans_resp(&resp);
            vlogD("Sending get_my_channels response.");
            goto finally;
        }

        for (i = 0; i < cvector_size(cinfos); ++i) {
            left -= cinfos[i]->len;
            cvector_push_back(cinfos_tmp, cinfos[i]);

            if (!(!left || i == cvector_size(cinfos) - 1 || cinfos[i + 1]->len > left))
                continue;

            GetMyChansResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = i == cvector_size(cinfos) - 1,
                    .cinfos  = cinfos_tmp
                }
            };
            resp_marshal = rpc_marshal_get_my_chans_resp(&resp);

            vlogD("Sending get_my_channels response.");

            rc = msgq_enq(from, resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_my_channels_metadata request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

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
        vlogD("Retrieved channel: "
              "{channel_id: %" PRIu64 ", subscribers: %" PRIu64 "}", cinfo->chan_id, cinfo->subs);
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
        vlogD("Sending get_my_channels_metadata response.");
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_channels request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

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
        vlogD("Retrieved channel: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, "
              "owner_name: %s, owner_did: %s, subscribers: %" PRIu64 ", last_update: %" PRIu64
              ", avatar_length: %" PRIu64 "}",
              cinfo->chan_id, cinfo->name, cinfo->intro, cinfo->owner->name,
              cinfo->owner->did, cinfo->subs, cinfo->upd_at, cinfo->len);
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
        size_t left = MAX_CONTENT_LEN;
        cvector_vector_type(ChanInfo *) cinfos_tmp = NULL;
        int i;

        if (!cvector_size(cinfos)) {
            GetChansResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = true,
                    .cinfos  = cinfos
                }
            };
            resp_marshal = rpc_marshal_get_chans_resp(&resp);
            vlogD("Sending get_channels response.");
            goto finally;
        }

        for (i = 0; i < cvector_size(cinfos); ++i) {
            left -= cinfos[i]->len;
            cvector_push_back(cinfos_tmp, cinfos[i]);

            if (!(!left || i == cvector_size(cinfos) - 1 || cinfos[i + 1]->len > left))
                continue;

            GetChansResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = i == cvector_size(cinfos) - 1,
                    .cinfos  = cinfos_tmp
                }
            };
            resp_marshal = rpc_marshal_get_chans_resp(&resp);

            vlogD("Sending get_channels response.");

            rc = msgq_enq(from, resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_channel_detail request from [%s]: "
          "{access_token: %s, id: %" PRIu64 "}", from, req->params.tk, req->params.id);

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
        vlogD("Sending get_channel_detail response: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, "
              "owner_name: %s, owner_did: %s, subscribers: %" PRIu64
              ", last_update: %" PRIu64 ", avatar_length: %" PRIu64 "}",
              chan->info.chan_id, chan->info.name, chan->info.intro, chan->info.owner->name,
              chan->info.owner->did, chan->info.subs, chan->info.upd_at, chan->info.len);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_subscribed_channels request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

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
        vlogD("Retrieved channel: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, owner_name: %s, "
              "owner_did: %s, subscribers: %" PRIu64 ", last_update: %" PRIu64 ", avatar_length: %" PRIu64 "}",
              cinfo->chan_id, cinfo->name, cinfo->intro, cinfo->owner->name,
              cinfo->owner->did, cinfo->subs, cinfo->upd_at, cinfo->len);
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
        size_t left = MAX_CONTENT_LEN;
        cvector_vector_type(ChanInfo *) cinfos_tmp = NULL;
        int i;

        if (!cvector_size(cinfos)) {
            GetSubChansResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = true,
                    .cinfos  = cinfos
                }
            };
            resp_marshal = rpc_marshal_get_sub_chans_resp(&resp);
            vlogD("Sending get_subscribed_channels response.");
            goto finally;
        }

        for (i = 0; i < cvector_size(cinfos); ++i) {
            left -= cinfos[i]->len;
            cvector_push_back(cinfos_tmp, cinfos[i]);

            if (!(!left || i == cvector_size(cinfos) - 1 || cinfos[i + 1]->len > left))
                continue;

            GetSubChansResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = i == cvector_size(cinfos) - 1,
                    .cinfos  = cinfos_tmp
                }
            };
            resp_marshal = rpc_marshal_get_sub_chans_resp(&resp);

            vlogD("Sending get_subscribed_channels response.");

            rc = msgq_enq(from, resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_posts request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", by: %" PRIu64
          ", upper_bound: %" PRIu64 ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.qc.by,
          req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

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
        vlogD("Retrieved post: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comments: %" PRIu64
              ", likes: %" PRIu64 ", created_at: %" PRIu64 ", content_length: %" PRIu64 "}",
              pinfo->chan_id, pinfo->post_id, pinfo->cmts, pinfo->likes, pinfo->created_at, pinfo->len);
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
        size_t left = MAX_CONTENT_LEN;
        cvector_vector_type(PostInfo *) pinfos_tmp = NULL;
        int i;

        if (!cvector_size(pinfos)) {
            GetPostsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = true,
                    .pinfos  = pinfos
                }
            };
            resp_marshal = rpc_marshal_get_posts_resp(&resp);
            vlogD("Sending get_posts response.");
            goto finally;
        }

        for (i = 0; i < cvector_size(pinfos); ++i) {
            left -= pinfos[i]->len;
            cvector_push_back(pinfos_tmp, pinfos[i]);

            if (!(!left || i == cvector_size(pinfos) - 1 || pinfos[i + 1]->len > left))
                continue;

            GetPostsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = i == cvector_size(pinfos) - 1,
                    .pinfos  = pinfos_tmp
                }
            };
            resp_marshal = rpc_marshal_get_posts_resp(&resp);

            vlogD("Sending get_posts response.");

            rc = msgq_enq(from, resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(pinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(pinfos_tmp);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_liked_posts request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

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
        vlogD("Retrieved post: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comments: %" PRIu64
              ", likes: %" PRIu64 ", created_at: %" PRIu64 ", content_length: %" PRIu64 "}",
              pinfo->chan_id, pinfo->post_id, pinfo->cmts, pinfo->likes, pinfo->created_at, pinfo->len);
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
        size_t left = MAX_CONTENT_LEN;
        cvector_vector_type(PostInfo *) pinfos_tmp = NULL;
        int i;

        if (!cvector_size(pinfos)) {
            GetLikedPostsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = true,
                    .pinfos  = pinfos
                }
            };
            resp_marshal = rpc_marshal_get_liked_posts_resp(&resp);
            vlogD("Sending get_liked_posts response.");
            goto finally;
        }

        for (i = 0; i < cvector_size(pinfos); ++i) {
            left -= pinfos[i]->len;
            cvector_push_back(pinfos_tmp, pinfos[i]);

            if (!(!left || i == cvector_size(pinfos) - 1 || pinfos[i + 1]->len > left))
                continue;

            GetLikedPostsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = i == cvector_size(pinfos) - 1,
                    .pinfos  = pinfos_tmp
                }
            };
            resp_marshal = rpc_marshal_get_liked_posts_resp(&resp);

            vlogD("Sending get_liked_posts response.");

            rc = msgq_enq(from, resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(pinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(pinfos_tmp);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_comments request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", post_id: %" PRIu64 ", by: %" PRIu64
          ", upper_bound: %" PRIu64 ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id,
          req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

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
        vlogD("Retrieved comment: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comment_id: %" PRIu64
              ", refcomment_id: %" PRIu64 ", user_name: %s, likes: %" PRIu64
              ", created_at: %" PRIu64 ", content_length: %" PRIu64 "}",
              cinfo->chan_id, cinfo->post_id, cinfo->cmt_id, cinfo->reply_to_cmt,
              cinfo->user.name, cinfo->likes, cinfo->created_at, cinfo->len);
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
        size_t left = MAX_CONTENT_LEN;
        cvector_vector_type(CmtInfo *) cinfos_tmp = NULL;
        int i;

        if (!cvector_size(cinfos)) {
            GetCmtsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = true,
                    .cinfos  = cinfos
                }
            };
            resp_marshal = rpc_marshal_get_cmts_resp(&resp);
            vlogD("Sending get_comments response.");
            goto finally;
        }

        for (i = 0; i < cvector_size(cinfos); ++i) {
            left -= cinfos[i]->len;
            cvector_push_back(cinfos_tmp, cinfos[i]);

            if (!(!left || i == cvector_size(cinfos) - 1 || cinfos[i + 1]->len > left))
                continue;

            GetCmtsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = i == cvector_size(cinfos) - 1,
                    .cinfos  = cinfos_tmp
                }
            };
            resp_marshal = rpc_marshal_get_cmts_resp(&resp);

            vlogD("Sending get_comments response.");

            rc = msgq_enq(from, resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
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

    vlogD("Received get_statistics request from [%s]: "
          "{access_token: %s}", from, req->params.tk);

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
        vlogD("Sending get_statistics response: "
              "{did: %s, connecting_clients: %zu}", feeds_owner_info.did, connecting_clients);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(uinfo);
}

void hdl_sub_chan_req(ElaCarrier *c, const char *from, Req *base)
{
    SubChanReq *req = (SubChanReq *)base;
    ActiveSuberPerChan *aspc = NULL;
    Marshalled *resp_marshal = NULL;
    ActiveSuber *owner = NULL;
    ActiveSuber *as = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogD("Received subscribe_channel request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 "}", from, req->params.tk, req->params.id);

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

    as = as_get(uinfo->uid);
    if (as) {
        aspc = aspc_create(as, chan);
        if (!aspc) {
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

    if (aspc)
        aspc_put(aspc);

    ++chan->info.subs;
    vlogI("[%s] subscribed to channel [%" PRIu64 "]", uinfo->did, req->params.id);

    {
        SubChanResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_sub_chan_resp(&resp);
        vlogD("Sending subscribe_channel response.");
    }

    if ((owner = as_get(OWNER_USER_ID))) {
        hashtable_iterator_t it;
        NotifDestPerActiveSuber *ndpas;

        hashtable_foreach(owner->ndpass, ndpas)
            notify_of_new_sub(ndpas->nd->node_id, chan->info.chan_id, uinfo);
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(owner);
    deref(uinfo);
    deref(chan);
    deref(aspc);
    deref(as);
}

void hdl_unsub_chan_req(ElaCarrier *c, const char *from, Req *base)
{
    UnsubChanReq *req = (UnsubChanReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogD("Received unsubscribe_channel request from [%s]: "
          "{access_token: %s, channel_id%" PRIu64 "}", from, req->params.tk, req->params.id);

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

    deref(aspc_remove(uinfo->uid, chan));
    --chan->info.subs;
    vlogI("[%s] unsubscribed channel [%" PRIu64 "]", uinfo->did, req->params.id);

    {
        UnsubChanResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_unsub_chan_resp(&resp);
        vlogD("Sending unsubscribe_channel response.");
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    deref(chan);
    deref(uinfo);
}

static
void nd_dtor(void *obj)
{
    NotifDest *nd = (NotifDest *)obj;

    deref(nd->ndpass);
}

static
NotifDest *nd_create(const char *node_id)
{
    NotifDest *nd = rc_zalloc(sizeof(NotifDest), nd_dtor);
    if (!nd) {
        vlogE("Creating Notification Destination failed.");
        return NULL;
    }

    nd->ndpass = list_create(0, NULL);
    if (!nd->ndpass) {
        deref(nd);
        return NULL;
    }

    strcpy(nd->node_id, node_id);
    nd->he.data   = nd;
    nd->he.key    = nd->node_id;
    nd->he.keylen = strlen(nd->node_id);

    return nd;
}

static
NotifDestPerActiveSuber *ndpas_create(ActiveSuber *as, NotifDest *nd)
{
    NotifDestPerActiveSuber *ndpas = rc_zalloc(sizeof(NotifDestPerActiveSuber), NULL);
    if (!ndpas)
        return NULL;

    ndpas->as = as;
    ndpas->nd = nd;

    ndpas->he.data   = ndpas;
    ndpas->he.key    = nd->node_id;
    ndpas->he.keylen = strlen(nd->node_id);

    ndpas->le.data = ndpas;

    return ndpas;
}

void hdl_enbl_notif_req(ElaCarrier *c, const char *from, Req *base)
{
    EnblNotifReq *req = (EnblNotifReq *)base;
    cvector_vector_type(ActiveSuberPerChan *) aspcs = NULL;
    NotifDestPerActiveSuber *ndpas = NULL;
    Marshalled *resp_marshal = NULL;
    ActiveSuber *as = NULL;
    UserInfo *uinfo = NULL;
    ActiveSuberPerChan **i;
    NotifDest *nd = NULL;
    bool new_nd = false;
    bool new_as = false;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    QryCriteria qc = {
        .by     = NONE,
        .upper  = 0,
        .lower  = 0,
        .maxcnt = 0
    };
    int rc;

    vlogD("Received enable_notification request from [%s]: "
          "{access_token: %s}", from, req->params.tk);

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

    as = as_get(uinfo->uid);
    if (as && ndpas_exist(as, from)) {
        vlogE("Already enabled notification");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    } else if (!as) {
        as = as_create(uinfo->uid);
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
        new_as = true;
    }

    nd = nd_get(from);
    if (!nd) {
        nd = nd_create(from);
        if (!nd) {
            vlogE("Creating notification destination failed.");
            rc = ERR_INTERNAL_ERROR;
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }
        new_nd = true;
    }

    ndpas = ndpas_create(as, nd);
    if (!ndpas) {
        vlogE("Creating notification destination per active subscriber failed.");
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
        ActiveSuberPerChan *aspc = aspc_create(as, chan);
        vlogD("Enabling notification of channel [%" PRIu64 "] for [%s]", chan->info.chan_id, uinfo->did);
        deref(chan);
        if (!aspc) {
            vlogE("Creating channel active subscriber failed.");
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }
        cvector_push_back(aspcs, aspc);
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

    ndpas_put(ndpas);
    cvector_foreach(aspcs, i)
        aspc_put(*i);

    if (new_as)
        as_put(as);
    if (new_nd)
        nd_put(nd);

    {
        EnblNotifResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_enbl_notif_resp(&resp);
        vlogD("Sending enable_notification response.");
    }

finally:
    if (resp_marshal)
        msgq_enq(from, resp_marshal);
    if (aspcs) {
        cvector_foreach(aspcs, i)
            deref(*i);
        cvector_free(aspcs);
    }
    deref(ndpas);
    deref(nd);
    deref(as);
    deref(uinfo);
    deref(it);
}

void feeds_deactivate_suber(const char *node_id)
{
    list_iterator_t it;
    NotifDest *nd;
    NotifDestPerActiveSuber *ndpas;

    nd = nd_remove(node_id);
    if (!nd)
        return;

    list_foreach(nd->ndpass, ndpas) {
        ActiveSuber *as = ndpas->as;
        deref(hashtable_remove(as->ndpass, node_id, strlen(node_id)));
        if (hashtable_is_empty(as->ndpass)) {
            hashtable_iterator_t it;
            ActiveSuberPerChan *aspc;

            hashtable_foreach(as->aspcs, aspc)
                deref(list_remove_entry(aspc->chan->aspcs, &aspc->le));
            deref(as_remove(as->uid));
        }
    }

    deref(nd);
}
