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

#include <crystal.h>
#include <inttypes.h>

#include "feeds.h"
#include "msgq.h"
#include "auth.h"
#include "did.h"
#include "obj.h"
#include "err.h"
#include "db.h"
#include "ver.h"

#define TAG_CMD "[Feedsd.Cmd ]: "

extern Carrier *carrier;
extern size_t connecting_clients;

typedef struct {
    linked_hash_entry_t he_name_key;
    linked_hash_entry_t he_id_key;
    linked_list_t *aspcs;
    ChanInfo info;
} Chan;

typedef struct {
    linked_hash_entry_t he;
    linked_hashtable_t *aspcs;
    linked_hashtable_t *ndpass;
    uint64_t uid;
} ActiveSuber;

typedef struct {
    linked_list_entry_t le;
    linked_hash_entry_t he;
    const Chan *chan;
    const ActiveSuber *as;
} ActiveSuberPerChan;

typedef struct {
    linked_hash_entry_t he;
    char node_id[ELA_MAX_ID_LEN + 1];
    linked_list_t *ndpass;
} NotifDest;

typedef struct {
    linked_hash_entry_t he;
    linked_list_entry_t le;
    ActiveSuber *as;
    NotifDest *nd;
} NotifDestPerActiveSuber;

static uint64_t nxt_chan_id = CHAN_ID_START;
static linked_hashtable_t *ass;
static linked_hashtable_t *nds;
static linked_hashtable_t *chans_by_name;
static linked_hashtable_t *chans_by_id;

#define hashtable_foreach(htab, entry)                                \
    for (linked_hashtable_iterate((htab), &it);                              \
         linked_hashtable_iterator_next(&it, NULL, NULL, (void **)&(entry)); \
         deref((entry)))

#define list_foreach(list, entry)                    \
    for (linked_list_iterate((list), &it);                  \
         linked_list_iterator_next(&it, (void **)&(entry)); \
         deref((entry)))

#define foreach_db_obj(entry) \
    for (;!(rc = db_iter_nxt(it, (void **)&entry)); deref(entry))

static inline
Chan *chan_put(Chan *chan)
{
    linked_hashtable_put(chans_by_name, &chan->he_name_key);
    return linked_hashtable_put(chans_by_id, &chan->he_id_key);
}

static inline
Chan *chan_rm(Chan *chan)
{
    deref(linked_hashtable_remove(chans_by_name, chan->info.name, strlen(chan->info.name)));
    return linked_hashtable_remove(chans_by_id, &chan->info.chan_id, sizeof(chan->info.chan_id));
}

static inline
Chan *chan_sub(Chan *upd, Chan *old)
{
    ActiveSuberPerChan *aspc;
    linked_list_iterator_t it;

    deref(chan_rm(old));

    list_foreach(upd->aspcs, aspc)
        aspc->chan = upd;
    linked_hashtable_put(chans_by_name, &upd->he_name_key);
    return linked_hashtable_put(chans_by_id, &upd->he_id_key);
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

    chan->aspcs = linked_list_create(0, NULL);
    if (!chan->aspcs) {
        deref(chan);
        return NULL;
    }

    buf = chan + 1;
    chan->info        = *ci;
    chan->info.name   = strcpy(buf, ci->name);
    buf = (char*)buf + strlen(ci->name) + 1;
    chan->info.intro  = strcpy(buf, ci->intro);
    buf = (char*)buf + strlen(ci->intro) + 1;
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
Chan *chan_create_upd(const Chan *from, const ChanInfo *ci)
{
    Chan *chan;
    void *buf;

    chan = rc_zalloc(sizeof(Chan) + strlen(ci->name) +
                     strlen(ci->intro) + 2 + ci->len, chan_dtor);
    if (!chan)
        return NULL;

    chan->aspcs = ref(from->aspcs);

    buf = chan + 1;
    chan->info        = *ci;
    chan->info.name   = strcpy(buf, ci->name);
    buf = (char*)buf + strlen(ci->name) + 1;
    chan->info.intro  = strcpy(buf, ci->intro);
    buf = (char*)buf + strlen(ci->intro) + 1;
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
        vlogE(TAG_CMD"Loading channels from database failed");
        return -1;
    }

    foreach_db_obj(cinfo) {
        Chan *chan = chan_create(cinfo);
        if (!chan) {
            deref(it);
            deref(cinfo);
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

    chans_by_name = linked_hashtable_create(8, 0, NULL, NULL);
    if (!chans_by_name) {
        vlogE(TAG_CMD"Creating channels by name failed");
        goto failure;
    }

    chans_by_id = linked_hashtable_create(8, 0, NULL, u64_cmp);
    if (!chans_by_id) {
        vlogE(TAG_CMD"Creating channels by id failed");
        goto failure;
    }

    ass = linked_hashtable_create(8, 0, NULL, u64_cmp);
    if (!ass) {
        vlogE(TAG_CMD "Creating active subscribers failed");
        goto failure;
    }

    nds = linked_hashtable_create(8, 0, NULL, NULL);
    if (!nds) {
        vlogE(TAG_CMD "Creating notification destinations failed");
        goto failure;
    }

    rc = load_chans_from_db();
    if (rc < 0)
        goto failure;

    vlogI(TAG_CMD "Feeds module initialized.");
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
void notify_of_chan_upd(const char *peer, const ChanInfo *ci)
{
    ChanUpdNotif notif = {
        .method = "feedinfo_update",
        .params = {
            .cinfo = (ChanInfo *)ci
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_chan_upd_notif(&notif);
    if (!notif_marshal)
        return;

    vlogD(TAG_CMD "Sending channel update notification to [%s]: {channel_id: %" PRIu64 "}", peer, ci->chan_id);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
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

    vlogD(TAG_CMD "Sending new post notification to [%s]: " "{channel_id: %" PRIu64 ", post_id: %" PRIu64 "}",
          peer, pi->chan_id, pi->post_id);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
}

static
void notify_of_post_upd(const char *peer, const PostInfo *pi)
{
    PostUpdNotif notif = {
        .method = "post_update",
        .params = {
            .pinfo = (PostInfo *)pi
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_post_upd_notif(&notif);
    if (!notif_marshal)
        return;

    vlogD(TAG_CMD "Sending post update notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", status: %s, content_len: %zu"
          ", comments: %" PRIu64 ", likes: %" PRIu64 ", created_at: %" PRIu64
          ", updated_at: %" PRIu64 "}",
          peer, pi->chan_id, pi->post_id, post_stat_str(pi->stat), pi->len, pi->cmts,
          pi->likes, pi->created_at, pi->upd_at);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
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

    vlogD(TAG_CMD "Sending new comment notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64
          ", comment_id: %" PRIu64 ", refcomment_id: %" PRIu64 "}",
          peer, ci->chan_id, ci->post_id, ci->cmt_id, ci->reply_to_cmt);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
}

static
void notify_of_cmt_upd(const char *peer, const CmtInfo *ci)
{
    CmtUpdNotif notif = {
        .method = "comment_update",
        .params = {
            .cinfo = (CmtInfo *)ci
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_cmt_upd_notif(&notif);
    if (!notif_marshal)
        return;

    vlogD(TAG_CMD "Sending comment_update notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64
          ", comment_id: %" PRIu64 ", refcomment_id: %" PRIu64 ", status: %s}",
          peer, ci->chan_id, ci->post_id, ci->cmt_id, ci->reply_to_cmt, cmt_stat_str(ci->stat));
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
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

    vlogD(TAG_CMD "Sending new like notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64
          ", comment_id: %" PRIu64 ", user_name: %s, user_did: %s, total_count: %" PRIu64 "}",
          peer, li->chan_id, li->post_id, li->cmt_id, li->user.name, li->user.did, li->total_cnt);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
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

    vlogD(TAG_CMD "Sending new subscription notification to [%s]: "
          "{channel_id: %" PRIu64 ", user_name: %s, user_did: %s}",
          peer, chan_id, uinfo->name, uinfo->did);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
}

static
void notify_of_stats_changed(const char *peer, uint64_t total_clients)
{
    StatsChangedNotif notif = {
        .method = "statistics_changed",
        .params = {
            .total_cs = total_clients
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_stats_changed_notif(&notif);
    if (!notif_marshal)
        return;

    vlogD(TAG_CMD "Sending statistics changed notification to [%s]: " "{total_clients: %" PRIu64 "}",
          peer, total_clients);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
}

static
void notify_of_report_cmt(const char *peer, const ReportedCmtInfo *li)
{
    ReportCmtNotif notif = {
        .method = "report_illegal_comment",
        .params = {
            .li = (ReportedCmtInfo *)li
        }
    };
    Marshalled *notif_marshal;

    notif_marshal = rpc_marshal_report_cmt_notif(&notif);
    if (!notif_marshal)
        return;

    vlogD(TAG_CMD "Sending new like notification to [%s]: "
          "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comment_id: %" PRIu64
          ", reporter_name: %s, reporter_did: %s, reasons: %s created_at: %" PRIu64 "}",
          peer, li->chan_id, li->post_id, li->cmt_id,
          li->reporter.name, li->reporter.did, li->reasons, li->created_at);
    msgq_enq(peer, notif_marshal);
    deref(notif_marshal);
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

    as->aspcs = linked_hashtable_create(8, 0, NULL, u64_cmp);
    if (!as->aspcs) {
        deref(as);
        return NULL;
    }

    as->ndpass = linked_hashtable_create(8, 0, NULL, NULL);
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
    return linked_hashtable_exist(chans_by_name, name, strlen(name));
}

static inline
Chan *chan_get_by_id(uint64_t id)
{
    return linked_hashtable_get(chans_by_id, &id, sizeof(id));
}

static inline
int chan_exist_by_id(uint64_t id)
{
    return linked_hashtable_exist(chans_by_id, &id, sizeof(id));
}

static inline
ActiveSuber *as_get(uint64_t uid)
{
    return linked_hashtable_get(ass, &uid, sizeof(uid));
}

static inline
ActiveSuber *as_remove(uint64_t uid)
{
    return linked_hashtable_remove(ass, &uid, sizeof(uid));
}

static inline
ActiveSuber *as_put(ActiveSuber *as)
{
    return linked_hashtable_put(ass, &as->he);
}

static inline
NotifDest *nd_get(const char *node_id)
{
    return linked_hashtable_get(nds, node_id, strlen(node_id));
}

static inline
NotifDest *nd_put(NotifDest *nd)
{
    return linked_hashtable_put(nds, &nd->he);
}

static inline
NotifDest *nd_remove(const char *node_id)
{
    return linked_hashtable_remove(nds, node_id, strlen(node_id));
}

static inline
int ndpas_exist(ActiveSuber *as, const char *node_id)
{
    return linked_hashtable_exist(as->ndpass, node_id, strlen(node_id));
}

static inline
NotifDestPerActiveSuber *ndpas_put(NotifDestPerActiveSuber *ndpas)
{
    linked_list_add(ndpas->nd->ndpass, &ndpas->le);
    return linked_hashtable_put(ndpas->as->ndpass, &ndpas->he);
}

static inline
ActiveSuberPerChan *aspc_put(ActiveSuberPerChan *aspc)
{
    linked_list_add(aspc->chan->aspcs, &aspc->le);
    return linked_hashtable_put(aspc->as->aspcs, &aspc->he);
}

static inline
ActiveSuberPerChan *aspc_remove(uint64_t uid, Chan *chan)
{
    ActiveSuberPerChan *aspc;
    ActiveSuber *as;

    as = as_get(uid);
    if (!as)
        return NULL;

    aspc = linked_hashtable_remove(as->aspcs, &chan->info.chan_id, sizeof(chan->info.chan_id));
    if (!aspc) {
        deref(as);
        return NULL;
    }

    deref(linked_list_remove_entry(chan->aspcs, &aspc->le));
    deref(as);

    return aspc;
}

void hdl_create_chan_req(Carrier *c, const char *from, Req *base)
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

    vlogD(TAG_CMD "Received create_channel request from [%s]: "
          "{access_token: %s, name: %s, introduction: %s, avatar_length: %zu}",
          from, req->params.tk, req->params.name, req->params.intro, req->params.sz);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Creating channel while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (chan_exist_by_name(req->params.name)) {
        vlogE(TAG_CMD "Creating an existing channel.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ALREADY_EXISTS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_create(&ci);
    if (!chan) {
        vlogE(TAG_CMD "Creating channel failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_create_chan(&ci);
    if (rc < 0) {
        vlogE(TAG_CMD "Adding channel to database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan_put(chan);
    ++nxt_chan_id;
    vlogI(TAG_CMD "Channel [%" PRIu64 "] created.", ci.chan_id);

    {
        CreateChanResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .id = chan->info.chan_id
            }
        };
        resp_marshal = rpc_marshal_create_chan_resp(&resp);
        vlogD(TAG_CMD "Sending create_channel response: "
              "{id: %" PRIu64 "}", ci.chan_id);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_upd_chan_req(Carrier *c, const char *from, Req *base)
{
    UpdChanReq *req = (UpdChanReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    Chan *chan_upd = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    ChanInfo ci;
    int rc;

    vlogD(TAG_CMD "Received update_feedinfo request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", name: %s, introduction: %s, avatar_length: %zu}",
          from, req->params.tk, req->params.chan_id, req->params.name, req->params.intro, req->params.sz);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Creating channel while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Channel to update does not exist.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (strcmp(chan->info.name, req->params.name) &&
        chan_exist_by_name(req->params.name)) {
        vlogE(TAG_CMD "Channel name to update already exists.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ALREADY_EXISTS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    ci.chan_id      = req->params.chan_id;
    ci.name         = req->params.name;
    ci.intro        = req->params.intro;
    ci.owner        = chan->info.owner;
    ci.created_at   = chan->info.created_at;
    ci.upd_at       = time(NULL);
    ci.subs         = chan->info.subs;
    ci.next_post_id = chan->info.next_post_id;
    ci.avatar       = req->params.avatar;
    ci.len          = req->params.sz;
    chan_upd = chan_create_upd(chan, &ci);
    if (!chan_upd) {
        vlogE(TAG_CMD "Creating updated channel failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_upd_chan(&ci);
    if (rc < 0) {
        vlogE(TAG_CMD "Updating channel to database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan_sub(chan_upd, chan);
    vlogI(TAG_CMD "Channel [%" PRIu64 "] updated.", ci.chan_id);

    {
        UpdChanResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_upd_chan_resp(&resp);
        vlogD(TAG_CMD "Sending update_feedinfo response.");
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_chan_upd(ndpas->nd->node_id, &ci);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
    deref(chan_upd);
}

void hdl_pub_post_req(Carrier *c, const char *from, Req *base)
{
    PubPostReq *req = (PubPostReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    PostInfo new_post;
    time_t now;
    int rc;

    vlogD(TAG_CMD "Received publish_post request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", content_length: %zu}",
          from, req->params.tk, req->params.chan_id, req->params.sz);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Publishing post while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Publishing post on non-existent channel.");
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
    new_post.stat       = POST_AVAILABLE;

    rc = db_add_post(&new_post);
    if (rc < 0) {
        vlogE(TAG_CMD "Inserting post into database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    ++chan->info.next_post_id;
    vlogI(TAG_CMD "Post [%" PRIu64 "] on channel [%" PRIu64 "] created.", new_post.post_id, new_post.chan_id);

    {
        PubPostResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .id = new_post.post_id
            }
        };
        resp_marshal = rpc_marshal_pub_post_resp(&resp);
        vlogD(TAG_CMD "Sending publish_post response: "
              "{id: %" PRIu64 "}", new_post.post_id);
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_new_post(ndpas->nd->node_id, &new_post);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_declare_post_req(Carrier *c, const char *from, Req *base)
{
    DeclarePostReq *req = (DeclarePostReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    PostInfo new_post;
    time_t now;
    int rc;

    vlogD(TAG_CMD "Received declare_post request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", content_length: %zu}",
          from, req->params.tk, req->params.chan_id, req->params.sz);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Declareing post while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Declareing post on non-existent channel.");
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
    new_post.stat       = (req->params.with_notify ? POST_AVAILABLE : POST_DECLARED);

    rc = db_add_post(&new_post);
    if (rc < 0) {
        vlogE(TAG_CMD "Inserting post into database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    ++chan->info.next_post_id;
    vlogI(TAG_CMD "Declare %s post [%" PRIu64 "] on channel [%" PRIu64 "] created.",
          post_stat_str(new_post.stat), new_post.post_id, new_post.chan_id);

    {
        DeclarePostResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .id = new_post.post_id
            }
        };
        resp_marshal = rpc_marshal_declare_post_resp(&resp);
        vlogD(TAG_CMD "Sending declare_post response: "
              "{id: %" PRIu64 "}", new_post.post_id);
    }

    if(req->params.with_notify) {
        list_foreach(chan->aspcs, aspc) {
            NotifDestPerActiveSuber *ndpas;
            linked_hashtable_iterator_t it;

            hashtable_foreach(aspc->as->ndpass, ndpas)
                notify_of_new_post(ndpas->nd->node_id, &new_post);
        }
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_notify_post_req(Carrier *c, const char *from, Req *base)
{
    NotifyPostReq *req = (NotifyPostReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    PostInfo post_notify;
    int rc;

    vlogI(TAG_CMD "Received notify_post request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", post_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Editing post while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Notifying non-existent post: invalid channel id.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_get_post_status(req->params.chan_id, req->params.post_id);
    if (rc < 0) {
        vlogE(TAG_CMD "Notifying non-existent post: invalid post id.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }
    if(rc == POST_DELETED) {
        vlogE(TAG_CMD "Notifying post: invalid post id.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&post_notify, 0, sizeof(post_notify));
    post_notify.chan_id = req->params.chan_id;
    post_notify.post_id = req->params.post_id;
    post_notify.stat    = POST_AVAILABLE;

    rc = db_set_post_status(&post_notify);
    if (rc < 0) {
        vlogE(TAG_CMD "Notifying post in database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Post [%" PRIu64 "] on channel [%" PRIu64 "] updated.", post_notify.post_id, post_notify.chan_id);

    {
        NotifyPostResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_notify_post_resp(&resp);
        vlogD(TAG_CMD "Sending notifyete_post response");
    }

    rc = db_get_post(post_notify.chan_id, post_notify.post_id, &post_notify);
    if (rc < 0) {
        vlogE(TAG_CMD "Notifying get post in database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_post_upd(ndpas->nd->node_id, &post_notify);
    }

    deref(post_notify.content);

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_edit_post_req(Carrier *c, const char *from, Req *base)
{
    EditPostReq *req = (EditPostReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    PostInfo post_mod;
    int rc;

    vlogD(TAG_CMD "Received edit_post request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", post_id: %" PRIu64 ", content_length: %zu}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.sz);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Editing post while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Editing non-existent post: invalid channel id.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_post_is_avail(req->params.chan_id, req->params.post_id)) < 0 || !rc) {
        vlogE(TAG_CMD "Editing non-existent post: invalid post id.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&post_mod, 0, sizeof(post_mod));
    post_mod.chan_id    = req->params.chan_id;
    post_mod.post_id    = req->params.post_id;
    post_mod.stat       = POST_AVAILABLE;
    post_mod.upd_at     = time(NULL);
    post_mod.content    = req->params.content;
    post_mod.len        = req->params.sz;

    rc = db_upd_post(&post_mod);
    if (rc < 0) {
        vlogE(TAG_CMD "Updating post in database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Post [%" PRIu64 "] on channel [%" PRIu64 "] updated.", post_mod.post_id, post_mod.chan_id);

    {
        EditPostResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_edit_post_resp(&resp);
        vlogD(TAG_CMD "Sending edit_post response");
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_post_upd(ndpas->nd->node_id, &post_mod);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_del_post_req(Carrier *c, const char *from, Req *base)
{
    DelPostReq *req = (DelPostReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    PostInfo post_del;
    int rc;

    vlogD(TAG_CMD "Received delete_post request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", post_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Editing post while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Deleting non-existent post: invalid channel id.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_post_is_avail(req->params.chan_id, req->params.post_id)) < 0 || !rc) {
        vlogE(TAG_CMD "Deleting non-existent post: invalid post id.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&post_del, 0, sizeof(post_del));
    post_del.chan_id = req->params.chan_id;
    post_del.post_id = req->params.post_id;
    post_del.stat    = POST_DELETED;
    post_del.upd_at  = time(NULL);

    rc = db_set_post_status(&post_del);
    if (rc < 0) {
        vlogE(TAG_CMD "Deleting post in database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Post [%" PRIu64 "] on channel [%" PRIu64 "] updated.", post_del.post_id, post_del.chan_id);

    {
        DelPostResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_del_post_resp(&resp);
        vlogD(TAG_CMD "Sending delete_post response");
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_post_upd(ndpas->nd->node_id, &post_del);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_post_cmt_req(Carrier *c, const char *from, Req *base)
{
    PostCmtReq *req = (PostCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    CmtInfo new_cmt;
    time_t now;
    int rc;

    vlogD(TAG_CMD "Received post_comment request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 ", content_length: %zu}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id, req->params.sz);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Posting comment on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Posting comment on non-existent post");
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
        vlogE(TAG_CMD "Posting comment on non-existent post");
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
        vlogE(TAG_CMD "Adding comment to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Comment [%" PRIu64 "] on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] created by [%s]",
          new_cmt.cmt_id, new_cmt.chan_id, new_cmt.post_id, new_cmt.reply_to_cmt, new_cmt.user.did);

    {
        PostCmtResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .id = new_cmt.cmt_id
            }
        };
        resp_marshal = rpc_marshal_post_cmt_resp(&resp);
        vlogD(TAG_CMD "Sending post_comment response: {id: %" PRIu64 "}", new_cmt.cmt_id);
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_new_cmt(ndpas->nd->node_id, &new_cmt);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_edit_cmt_req(Carrier *c, const char *from, Req *base)
{
    EditCmtReq *req = (EditCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    uint64_t cmt_uid;
    CmtInfo cmt_mod;
    int rc;

    vlogD(TAG_CMD "Received edit_comment request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", id: %" PRIu64 ", comment_id: %" PRIu64 ", content_length: %zu}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.id,
          req->params.cmt_id, req->params.sz);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Editing comment on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Editing comment on non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_is_avail(req->params.chan_id,
                              req->params.post_id,
                              req->params.id)) < 0 || !rc) {
        vlogE(TAG_CMD "Editing unavailable comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_uid(req->params.chan_id,
                         req->params.post_id,
                         req->params.id,
                         &cmt_uid)) < 0 || cmt_uid != uinfo->uid) {
        vlogE(TAG_CMD "Editing other's comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.cmt_id &&
        ((rc = db_cmt_exists(req->params.chan_id,
                             req->params.post_id,
                             req->params.cmt_id)) < 0 || !rc)) {
        vlogE(TAG_CMD "Editing comment to reply to non-existent comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&cmt_mod, 0, sizeof(cmt_mod));
    cmt_mod.chan_id      = req->params.chan_id;
    cmt_mod.post_id      = req->params.post_id;
    cmt_mod.cmt_id       = req->params.id;
    cmt_mod.stat         = CMT_AVAILABLE;
    cmt_mod.user         = *uinfo;
    cmt_mod.reply_to_cmt = req->params.cmt_id;
    cmt_mod.content      = req->params.content;
    cmt_mod.len          = req->params.sz;
    cmt_mod.upd_at       = time(NULL);

    rc = db_upd_cmt(&cmt_mod);
    if (rc < 0) {
        vlogE(TAG_CMD "Updating comment in database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Comment [%" PRIu64 "] on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] updated by [%s].",
          cmt_mod.cmt_id, cmt_mod.chan_id, cmt_mod.post_id, cmt_mod.reply_to_cmt, cmt_mod.user.did);

    {
        EditCmtResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_edit_cmt_resp(&resp);
        vlogD(TAG_CMD "Sending edit_comment response");
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_cmt_upd(ndpas->nd->node_id, &cmt_mod);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_del_cmt_req(Carrier *c, const char *from, Req *base)
{
    DelCmtReq *req = (DelCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    uint64_t cmt_uid;
    CmtInfo cmt_del;
    int rc;

    vlogD(TAG_CMD "Received delete_comment request from [%s]: {access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Deleting comment on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Deleting comment on non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_is_avail(req->params.chan_id,
                              req->params.post_id,
                              req->params.id)) < 0 || !rc) {
        vlogE(TAG_CMD "Deleting unavailable comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_uid(req->params.chan_id,
                         req->params.post_id,
                         req->params.id,
                         &cmt_uid)) < 0 || cmt_uid != uinfo->uid) {
        vlogE(TAG_CMD "Deleting other's comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&cmt_del, 0, sizeof(cmt_del));
    cmt_del.chan_id = req->params.chan_id;
    cmt_del.post_id = req->params.post_id;
    cmt_del.cmt_id  = req->params.id;
    cmt_del.stat    = CMT_DELETED;
    cmt_del.user    = *uinfo;
    cmt_del.upd_at  = time(NULL);

    rc = db_set_cmt_status(&cmt_del);
    if (rc < 0) {
        vlogE(TAG_CMD "Deleting comment in database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] deleted by [%s].",
          cmt_del.chan_id, cmt_del.post_id, cmt_del.cmt_id, cmt_del.user.did);

    {
        DelCmtResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_del_cmt_resp(&resp);
        vlogD(TAG_CMD "Sending delete_comment response");
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_cmt_upd(ndpas->nd->node_id, &cmt_del);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_block_cmt_req(Carrier *c, const char *from, Req *base)
{
    BlockCmtReq *req = (BlockCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    uint64_t cmt_uid;
    CmtInfo cmt_block;
    int rc;

    vlogD(TAG_CMD "Received block_comment request from [%s]: {access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Blocking comment on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Blocking comment on non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_exists(req->params.chan_id,
                            req->params.post_id,
                            req->params.cmt_id)) < 0 || !rc) {
        vlogE(TAG_CMD "Blocking unavailable comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_uid(req->params.chan_id,
                         req->params.post_id,
                         req->params.cmt_id,
                         &cmt_uid)) < 0 || cmt_uid != uinfo->uid) {
        vlogE(TAG_CMD "Blocking other's comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&cmt_block, 0, sizeof(cmt_block));
    cmt_block.chan_id = req->params.chan_id;
    cmt_block.post_id = req->params.post_id;
    cmt_block.cmt_id  = req->params.cmt_id;
    cmt_block.stat    = CMT_BLOCKED;
    cmt_block.user    = *uinfo;
    cmt_block.upd_at  = time(NULL);

    rc = db_set_cmt_status(&cmt_block);
    if (rc < 0) {
        vlogE(TAG_CMD "Blocking comment in database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] blocked by [%s].",
          cmt_block.chan_id, cmt_block.post_id, cmt_block.cmt_id, cmt_block.user.did);

    {
        BlockCmtResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_block_cmt_resp(&resp);
        vlogD(TAG_CMD "Sending block_comment response");
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_cmt_upd(ndpas->nd->node_id, &cmt_block);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_unblock_cmt_req(Carrier *c, const char *from, Req *base)
{
    UnblockCmtReq *req = (UnblockCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    ActiveSuberPerChan *aspc;
    UserInfo *uinfo = NULL;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    uint64_t cmt_uid;
    CmtInfo cmt_unblock;
    int rc;

    vlogD(TAG_CMD "Received unblock_comment request from [%s]: {access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Unblocking comment on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Unblocking comment on non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_exists(req->params.chan_id,
                            req->params.post_id,
                            req->params.cmt_id)) < 0 || !rc) {
        vlogE(TAG_CMD "Unblocking unavailable comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_cmt_uid(req->params.chan_id,
                         req->params.post_id,
                         req->params.cmt_id,
                         &cmt_uid)) < 0 || cmt_uid != uinfo->uid) {
        vlogE(TAG_CMD "Unblocking other's comment");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    memset(&cmt_unblock, 0, sizeof(cmt_unblock));
    cmt_unblock.chan_id = req->params.chan_id;
    cmt_unblock.post_id = req->params.post_id;
    cmt_unblock.cmt_id  = req->params.cmt_id;
    cmt_unblock.stat    = CMT_AVAILABLE;
    cmt_unblock.user    = *uinfo;
    cmt_unblock.upd_at  = time(NULL);

    rc = db_set_cmt_status(&cmt_unblock);
    if (rc < 0) {
        vlogE(TAG_CMD "Unblocking comment in database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] unblocked by [%s].",
          cmt_unblock.chan_id, cmt_unblock.post_id, cmt_unblock.cmt_id, cmt_unblock.user.did);

    {
        UnblockCmtResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_unblock_cmt_resp(&resp);
        vlogD(TAG_CMD "Sending unblock_comment response");
    }

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_cmt_upd(ndpas->nd->node_id, &cmt_unblock);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_post_like_req(Carrier *c, const char *from, Req *base)
{
    PostLikeReq *req = (PostLikeReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    ActiveSuberPerChan *aspc;
    linked_list_iterator_t it;
    Chan *chan = NULL;
    LikeInfo li;
    int rc;

    vlogD(TAG_CMD "Received post_like request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Posting like on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Posting like on non-existent post");
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
        vlogE(TAG_CMD "Posting like on non-existent comment");
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
        vlogE(TAG_CMD "Posting like on liked subject");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_add_like(uinfo->uid, req->params.chan_id, req->params.post_id, req->params.cmt_id, &li.total_cnt);
    if (rc < 0) {
        vlogE(TAG_CMD "Adding like to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Like on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] by [%s].",
          req->params.chan_id, req->params.post_id, req->params.cmt_id, uinfo->did);

    {
        PostLikeResp resp = {
            .tsx_id = req->tsx_id
        };
        resp_marshal = rpc_marshal_post_like_resp(&resp);
        vlogD(TAG_CMD "Sending post_like response.");
    }

    li.chan_id = req->params.chan_id;
    li.post_id = req->params.post_id;
    li.cmt_id  = req->params.cmt_id;
    li.user    = *uinfo;

    list_foreach(chan->aspcs, aspc) {
        NotifDestPerActiveSuber *ndpas;
        linked_hashtable_iterator_t it;

        hashtable_foreach(aspc->as->ndpass, ndpas)
            notify_of_new_like(ndpas->nd->node_id, &li);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_post_unlike_req(Carrier *c, const char *from, Req *base)
{
    PostUnlikeReq *req = (PostUnlikeReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogD(TAG_CMD "Received post_unlike request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Posting unlike on non-existent channel");
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
        vlogE(TAG_CMD "Posting unlike on unliked subject");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_rm_like(uinfo->uid, req->params.chan_id, req->params.post_id, req->params.cmt_id);
    if (rc < 0) {
        vlogE(TAG_CMD "Removing like to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Unlike on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] by [%s].",
          req->params.chan_id, req->params.post_id, req->params.cmt_id, uinfo->did);

    {
        PostUnlikeResp resp = {
            .tsx_id = req->tsx_id
        };
        resp_marshal = rpc_marshal_post_unlike_resp(&resp);
        vlogD(TAG_CMD "Sending post_unlike response.");
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

#define MAX_CONTENT_LEN (ELA_MAX_APP_BULKMSG_LEN - 100 * 1024)
void hdl_get_my_chans_req(Carrier *c, const char *from, Req *base)
{
    GetMyChansReq *req = (GetMyChansReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogD(TAG_CMD "Received get_my_channels request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Getting owned channels while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_chans(&req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting owned channels from database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD(TAG_CMD "Retrieved channel: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, subscribers: %" PRIu64
              ", avatar_length: %zu}",
              cinfo->chan_id, cinfo->name, cinfo->intro, cinfo->subs, cinfo->len);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating owned channels failed");
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
            vlogD(TAG_CMD "Sending get_my_channels response.");
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

            vlogD(TAG_CMD "Sending get_my_channels response.");

            rc = msgq_enq(from, resp_marshal);
            deref(resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_my_chans_meta_req(Carrier *c, const char *from, Req *base)
{
    GetMyChansMetaReq *req = (GetMyChansMetaReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogD(TAG_CMD "Received get_my_channels_metadata request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Getting owned channels metadata while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_chans(&req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting owned channels metadata from database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD(TAG_CMD "Retrieved channel: "
              "{channel_id: %" PRIu64 ", subscribers: %" PRIu64 "}", cinfo->chan_id, cinfo->subs);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating owned channels metadata failed");
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
        vlogD(TAG_CMD "Sending get_my_channels_metadata response.");
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_chans_req(Carrier *c, const char *from, Req *base)
{
    GetChansReq *req = (GetChansReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogD(TAG_CMD "Received get_channels request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_chans(&req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting channels from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD(TAG_CMD "Retrieved channel: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, "
              "owner_name: %s, owner_did: %s, subscribers: %" PRIu64 ", last_update: %" PRIu64
              ", avatar_length: %zu}",
              cinfo->chan_id, cinfo->name, cinfo->intro, cinfo->owner->name,
              cinfo->owner->did, cinfo->subs, cinfo->upd_at, cinfo->len);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating channels failed.");
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
            vlogD(TAG_CMD "Sending get_channels response.");
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

            vlogD(TAG_CMD "Sending get_channels response.");

            rc = msgq_enq(from, resp_marshal);
            deref(resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_chan_dtl_req(Carrier *c, const char *from, Req *base)
{
    GetChanDtlReq *req = (GetChanDtlReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;

    vlogD(TAG_CMD "Received get_channel_detail request from [%s]: "
          "{access_token: %s, id: %" PRIu64 "}", from, req->params.tk, req->params.id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!(chan = chan_get_by_id(req->params.id))) {
        vlogE(TAG_CMD "Getting detail on non-existent channel");
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
        vlogD(TAG_CMD "Sending get_channel_detail response: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, "
              "owner_name: %s, owner_did: %s, subscribers: %" PRIu64
              ", last_update: %" PRIu64 ", avatar_length: %zu}",
              chan->info.chan_id, chan->info.name, chan->info.intro, chan->info.owner->name,
              chan->info.owner->did, chan->info.subs, chan->info.upd_at, chan->info.len);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
}

void hdl_get_sub_chans_req(Carrier *c, const char *from, Req *base)
{
    GetSubChansReq *req = (GetSubChansReq *)base;
    cvector_vector_type(ChanInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    ChanInfo *cinfo;
    int rc;

    vlogD(TAG_CMD "Received get_subscribed_channels request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_sub_chans(uinfo->uid, &req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting subscribed channels from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD(TAG_CMD "Retrieved channel: "
              "{channel_id: %" PRIu64 ", name: %s, introduction: %s, owner_name: %s, "
              "owner_did: %s, subscribers: %" PRIu64 ", last_update: %" PRIu64 ", avatar_length: %zu}",
              cinfo->chan_id, cinfo->name, cinfo->intro, cinfo->owner->name,
              cinfo->owner->did, cinfo->subs, cinfo->upd_at, cinfo->len);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating subscribed channels failed.");
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
            vlogD(TAG_CMD "Sending get_subscribed_channels response.");
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

            vlogD(TAG_CMD "Sending get_subscribed_channels response.");

            rc = msgq_enq(from, resp_marshal);
            deref(resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_posts_req(Carrier *c, const char *from, Req *base)
{
    GetPostsReq *req = (GetPostsReq *)base;
    cvector_vector_type(PostInfo *) pinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    PostInfo *pinfo;
    int rc;

    vlogD(TAG_CMD "Received get_posts request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", by: %" PRIu64
          ", upper_bound: %" PRIu64 ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.qc.by,
          req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!chan_exist_by_id(req->params.chan_id)) {
        vlogE(TAG_CMD "Getting posts from non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_posts(req->params.chan_id, &req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting posts from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(pinfo) {
        cvector_push_back(pinfos, ref(pinfo));
        vlogD(TAG_CMD "Retrieved post: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", status: %s, comments: %" PRIu64
              ", likes: %" PRIu64 ", created_at: %" PRIu64 ", updated_at: %" PRIu64 ", content_length: %zu}",
              pinfo->chan_id, pinfo->post_id, post_stat_str(pinfo->stat), pinfo->cmts,
              pinfo->likes, pinfo->created_at, pinfo->upd_at, pinfo->len);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating posts failed.");
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
            vlogD(TAG_CMD "Sending get_posts response.");
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

            vlogD(TAG_CMD "Sending get_posts response.");

            rc = msgq_enq(from, resp_marshal);
            deref(resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(pinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(pinfos_tmp);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_posts_lac_req(Carrier *c, const char *from, Req *base)
{
    GetPostsLACReq *req = (GetPostsLACReq *)base;
    cvector_vector_type(PostInfo *) pinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    PostInfo *pinfo;
    int rc;

    vlogD(TAG_CMD "Received get_posts_likes_and_comments request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", by: %" PRIu64
          ", upper_bound: %" PRIu64 ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.qc.by,
          req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!chan_exist_by_id(req->params.chan_id)) {
        vlogE(TAG_CMD "Getting posts likes and comments from non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_posts_lac(req->params.chan_id, &req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting posts likes and comments from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(pinfo) {
        cvector_push_back(pinfos, ref(pinfo));
        vlogD(TAG_CMD "Retrieved post likes and comments: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comments: %" PRIu64 ", likes: %" PRIu64 "}",
              pinfo->chan_id, pinfo->post_id, pinfo->cmts, pinfo->likes);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating posts likes and comments failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetPostsLACResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .pinfos  = pinfos
            }
        };
        resp_marshal = rpc_marshal_get_posts_lac_resp(&resp);
        vlogD(TAG_CMD "Sending get_posts_likes_and_comments response.");
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_liked_posts_req(Carrier *c, const char *from, Req *base)
{
    GetLikedPostsReq *req = (GetLikedPostsReq *)base;
    cvector_vector_type(PostInfo *) pinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    PostInfo *pinfo;
    int rc;

    vlogD(TAG_CMD "Received get_liked_posts request from [%s]: "
          "{access_token: %s, by: %" PRIu64 ", upper_bound: %" PRIu64
          ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_liked_posts(uinfo->uid, &req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting liked posts from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(pinfo) {
        cvector_push_back(pinfos, ref(pinfo));
        vlogD(TAG_CMD "Retrieved post: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comments: %" PRIu64
              ", likes: %" PRIu64 ", created_at: %" PRIu64 ", content_length: %zu}",
              pinfo->chan_id, pinfo->post_id, pinfo->cmts, pinfo->likes, pinfo->created_at, pinfo->len);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating posts failed.");
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
            vlogD(TAG_CMD "Sending get_liked_posts response.");
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

            vlogD(TAG_CMD "Sending get_liked_posts response.");

            rc = msgq_enq(from, resp_marshal);
            deref(resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(pinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(pinfos_tmp);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_cmts_req(Carrier *c, const char *from, Req *base)
{
    GetCmtsReq *req = (GetCmtsReq *)base;
    cvector_vector_type(CmtInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    Chan *chan = NULL;
    CmtInfo *cinfo;
    int rc;

    vlogD(TAG_CMD "Received get_comments request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", post_id: %" PRIu64 ", by: %" PRIu64
          ", upper_bound: %" PRIu64 ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id,
          req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!(chan = chan_get_by_id(req->params.chan_id))) {
        vlogE(TAG_CMD "Getting comments from non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Getting comment from non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_cmts(req->params.chan_id, req->params.post_id, &req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting comments from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD(TAG_CMD "Retrieved comment: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comment_id: %" PRIu64
              ",status: %s, refcomment_id: %" PRIu64 ", user_name: %s, user_did: %s, likes: %"
              PRIu64 ", created_at: %" PRIu64 "updated_at: %" PRIu64 ", content_length: %zu}",
              cinfo->chan_id, cinfo->post_id, cinfo->cmt_id, cmt_stat_str(cinfo->stat),
              cinfo->reply_to_cmt, cinfo->user.name, cinfo->user.did, cinfo->likes, cinfo->created_at,
              cinfo->upd_at, cinfo->len);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating comments failed.");
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
            vlogD(TAG_CMD "Sending get_comments response.");
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

            vlogD(TAG_CMD "Sending get_comments response.");

            rc = msgq_enq(from, resp_marshal);
            deref(resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(cinfos_tmp, 0);
            left = MAX_CONTENT_LEN;
        }

        cvector_free(cinfos_tmp);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_cmts_likes_req(Carrier *c, const char *from, Req *base)
{
    GetCmtsLikesReq *req = (GetCmtsLikesReq *)base;
    cvector_vector_type(CmtInfo *) cinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    Chan *chan = NULL;
    CmtInfo *cinfo;
    int rc;

    vlogD(TAG_CMD "Received get_comments_likes request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 ", post_id: %" PRIu64 ", by: %" PRIu64
              ", upper_bound: %" PRIu64 ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.chan_id, req->params.post_id,
          req->params.qc.by, req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!(chan = chan_get_by_id(req->params.chan_id))) {
        vlogE(TAG_CMD "Getting comments likes from non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Getting comment likes from non-existent post");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_cmts_likes(req->params.chan_id, req->params.post_id, &req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting comments likes from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(cinfo) {
        cvector_push_back(cinfos, ref(cinfo));
        vlogD(TAG_CMD "Retrieved comment: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "likes: %" PRIu64 "}",
              cinfo->chan_id, cinfo->post_id, cinfo->cmt_id, cinfo->likes);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating comments likes failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetCmtsLikesResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .cinfos  = cinfos
            }
        };
        resp_marshal = rpc_marshal_get_cmts_likes_resp(&resp);
        vlogD(TAG_CMD "Sending get_comments_likes response.");
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
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

void hdl_get_stats_req(Carrier *c, const char *from, Req *base)
{
    GetStatsReq *req = (GetStatsReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    int total_clients;

    vlogD(TAG_CMD "Received get_statistics request from [%s]: "
          "{access_token: %s}", from, req->params.tk);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    total_clients = db_get_user_count();
    if(total_clients < 0) {
        vlogE(TAG_CMD "DB get user count failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_DB_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        GetStatsResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .did     = feeds_owner_info.did,
                .conn_cs = connecting_clients,
                .total_cs = total_clients,
            }
        };
        resp_marshal = rpc_marshal_get_stats_resp(&resp);
        vlogD(TAG_CMD "Sending get_statistics response: "
              "{did: %s, connecting_clients: %zu, total_clients: %zu}",
              feeds_owner_info.did, connecting_clients, total_clients);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
}

void hdl_sub_chan_req(Carrier *c, const char *from, Req *base)
{
    SubChanReq *req = (SubChanReq *)base;
    ActiveSuberPerChan *aspc = NULL;
    Marshalled *resp_marshal = NULL;
    ActiveSuber *owner = NULL;
    ActiveSuber *as = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogD(TAG_CMD "Received subscribe_channel request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64 "}", from, req->params.tk, req->params.id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.id);
    if (!chan) {
        vlogE(TAG_CMD "Subscribing non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_is_suber(uinfo->uid, req->params.id)) < 0 || rc) {
        vlogE(TAG_CMD "Subscribing subscribed channel");
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
            vlogE(TAG_CMD "Creating channel active subscriber failed.");
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
        vlogE(TAG_CMD "Adding subscription to database failed");
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
    vlogI(TAG_CMD "[%s] subscribed to channel [%" PRIu64 "]", uinfo->did, req->params.id);

    {
        SubChanResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_sub_chan_resp(&resp);
        vlogD(TAG_CMD "Sending subscribe_channel response.");
    }

    if ((owner = as_get(OWNER_USER_ID))) {
        linked_hashtable_iterator_t it;
        NotifDestPerActiveSuber *ndpas;

        hashtable_foreach(owner->ndpass, ndpas)
            notify_of_new_sub(ndpas->nd->node_id, chan->info.chan_id, uinfo);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(owner);
    deref(uinfo);
    deref(chan);
    deref(aspc);
    deref(as);
}

void hdl_unsub_chan_req(Carrier *c, const char *from, Req *base)
{
    UnsubChanReq *req = (UnsubChanReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    int rc;

    vlogD(TAG_CMD "Received unsubscribe_channel request from [%s]: "
          "{access_token: %s, channel_id%" PRIu64 "}", from, req->params.tk, req->params.id);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.id);
    if (!chan) {
        vlogE(TAG_CMD "Unsubscribing non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((rc = db_is_suber(uinfo->uid, req->params.id)) < 0 || !rc) {
        vlogE(TAG_CMD "Unsubscribing non-existent subscription");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_unsub(uinfo->uid, req->params.id);
    if (rc < 0) {
        vlogE(TAG_CMD "Removing subscription from database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    deref(aspc_remove(uinfo->uid, chan));
    --chan->info.subs;
    vlogI(TAG_CMD "[%s] unsubscribed channel [%" PRIu64 "]", uinfo->did, req->params.id);

    {
        UnsubChanResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_unsub_chan_resp(&resp);
        vlogD(TAG_CMD "Sending unsubscribe_channel response.");
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
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
        vlogE(TAG_CMD "Creating Notification Destination failed.");
        return NULL;
    }

    nd->ndpass = linked_list_create(0, NULL);
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

void hdl_enbl_notif_req(Carrier *c, const char *from, Req *base)
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
    QryCriteria qc = {
        .by     = NONE,
        .upper  = 0,
        .lower  = 0,
        .maxcnt = 0
    };
    int rc;

    vlogD(TAG_CMD "Received enable_notification request from [%s]: "
          "{access_token: %s}", from, req->params.tk);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    as = as_get(uinfo->uid);
    if (as && ndpas_exist(as, from)) {
        vlogE(TAG_CMD "Already enabled notification");
        goto success_resp;
    } else if (!as) {
        as = as_create(uinfo->uid);
        if (!as) {
            vlogE(TAG_CMD "Creating active subscriber failed.");
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
            vlogE(TAG_CMD "Creating notification destination failed.");
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
        vlogE(TAG_CMD "Creating notification destination per active subscriber failed.");
        rc = ERR_INTERNAL_ERROR;
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (new_as) {
        DBObjIt *it = NULL;
        ChanInfo *cinfo;

        it = db_iter_sub_chans(uinfo->uid, &qc);
        if (!it) {
            vlogE(TAG_CMD "Getting subscribed channels from database failed.");
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
            vlogD(TAG_CMD "Enabling notification of channel [%" PRIu64 "] for [%s]", chan->info.chan_id, uinfo->did);
            deref(chan);
            if (!aspc) {
                vlogE(TAG_CMD "Creating channel active subscriber failed.");
                ErrResp resp = {
                    .tsx_id = req->tsx_id,
                    .ec     = ERR_INTERNAL_ERROR
                };
                resp_marshal = rpc_marshal_err_resp(&resp);
                deref(cinfo);
                deref(it);
                goto finally;
            }
            cvector_push_back(aspcs, aspc);
        }
        deref(it);
        if (rc < 0) {
            vlogE(TAG_CMD "Iterating subscribed channels failed.");
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }
    }

    ndpas_put(ndpas);

    if (new_as) {
        cvector_foreach(aspcs, i)
            aspc_put(*i);
        as_put(as);
    }
    if (new_nd)
        nd_put(nd);

success_resp:
    {
        EnblNotifResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_enbl_notif_resp(&resp);
        vlogD(TAG_CMD "Sending enable_notification response.");
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    if (aspcs) {
        cvector_foreach(aspcs, i)
            deref(*i);
        cvector_free(aspcs);
    }
    deref(ndpas);
    deref(nd);
    deref(as);
    deref(uinfo);
}

void hdl_get_srv_ver_req(Carrier *c, const char *from, Req *base)
{
    GetSrvVerReq *req = (GetSrvVerReq *)base;
    Marshalled *resp_marshal = NULL;

    vlogD(TAG_CMD "Received get_service_version request from [%s]: ", from);

    GetSrvVerResp resp = {
        .tsx_id = req->tsx_id,
        .result = {
            .version  = FEEDSD_VER,
            .version_code  = FEEDSD_VERCODE,
        }
    };
    resp_marshal = rpc_marshal_get_srv_ver_resp(&resp);
    vlogD(TAG_CMD "get_service_version response: "
          "{version: %s, version_code:%lld}", resp.result.version, resp.result.version_code);

    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
}

void hdl_report_illegal_cmt_req(Carrier *c, const char *from, Req *base)
{
    ReportIllegalCmtReq *req = (ReportIllegalCmtReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    Chan *chan = NULL;
    ActiveSuber *owner = NULL;
    ReportedCmtInfo li;
    int rc;

    vlogD(TAG_CMD "Received report_illegal_cmt request from [%s]: "
          "{access_token: %s, channel_id: %" PRIu64
          ", post_id: %" PRIu64 ", comment_id: %" PRIu64 "}"
          ", reasons: %s",
          from, req->params.tk, req->params.chan_id, req->params.post_id, req->params.cmt_id, req->params.reasons);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chan = chan_get_by_id(req->params.chan_id);
    if (!chan) {
        vlogE(TAG_CMD "Reporting illegal comment on non-existent channel");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_EXIST
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.post_id >= chan->info.next_post_id) {
        vlogE(TAG_CMD "Reporting illegal comment on non-existent post");
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
        vlogE(TAG_CMD "Reporting illegal comment on non-existent comment");
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
        vlogE(TAG_CMD "Reporting illegal comment on liked subject");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_WRONG_STATE
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_add_reported_cmts(req->params.chan_id, req->params.post_id, req->params.cmt_id,
                              uinfo->uid, req->params.reasons);
    if (rc < 0) {
        vlogE(TAG_CMD "Adding reported_comment to database failed");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI(TAG_CMD "Reported illegal on channel [%" PRIu64 "] post [%" PRIu64 "] comment [%" PRIu64 "] by [%s], reasons: %s.",
          req->params.chan_id, req->params.post_id, req->params.cmt_id, uinfo->did, req->params.reasons);

    {
        ReportIllegalCmtResp resp = {
            .tsx_id = req->tsx_id
        };
        resp_marshal = rpc_marshal_report_illegal_cmt_resp(&resp);
        vlogD(TAG_CMD "Sending report_illegal_cmt response.");
    }

    li.chan_id = req->params.chan_id;
    li.post_id = req->params.post_id;
    li.cmt_id  = req->params.cmt_id;
    li.reporter    = *uinfo;

    if ((owner = as_get(OWNER_USER_ID))) {
        linked_hashtable_iterator_t it;
        NotifDestPerActiveSuber *ndpas;

        hashtable_foreach(owner->ndpass, ndpas)
            notify_of_report_cmt(ndpas->nd->node_id, &li);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    deref(uinfo);
    deref(chan);
    deref(owner);
}

void hdl_get_reported_cmts_req(Carrier *c, const char *from, Req *base)
{
    GetReportedCmtsReq *req = (GetReportedCmtsReq *)base;
    cvector_vector_type(ReportedCmtInfo *) rcinfos = NULL;
    Marshalled *resp_marshal = NULL;
    UserInfo *uinfo = NULL;
    DBObjIt *it = NULL;
    Chan *chan = NULL;
    ReportedCmtInfo *rcinfo;
    int rc;

    vlogD(TAG_CMD "Received get_comments request from [%s]: "
          "{access_token: %s, by: %" PRIu64
          ", upper_bound: %" PRIu64 ", lower_bound: %" PRIu64 ", max_count: %" PRIu64 "}",
          from, req->params.tk, req->params.qc.by,
          req->params.qc.upper, req->params.qc.lower, req->params.qc.maxcnt);

    if (!did_is_ready()) {
        vlogE(TAG_CMD "Feeds DID is not ready.");
        return;
    }

    uinfo = create_uinfo_from_access_token(req->params.tk);
    if (!uinfo) {
        vlogE(TAG_CMD "Invalid access token.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_ACCESS_TOKEN_EXP
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!user_id_is_owner(uinfo->uid)) {
        vlogE(TAG_CMD "Get reported owner while not being owner.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    it = db_iter_reported_cmts(&req->params.qc);
    if (!it) {
        vlogE(TAG_CMD "Getting reported_comments from database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    foreach_db_obj(rcinfo) {
        cvector_push_back(rcinfos, ref(rcinfo));
        vlogD(TAG_CMD "Retrieved comment: "
              "{channel_id: %" PRIu64 ", post_id: %" PRIu64 ", comment_id: %" PRIu64
              ", reporter_name: %s, reporter_did: %s, reasons:%s"
              ", created_at: %" PRIu64 "}",
              rcinfo->chan_id, rcinfo->post_id, rcinfo->cmt_id,
              rcinfo->reporter.name, rcinfo->reporter.did, rcinfo->reasons,
              rcinfo->created_at);
    }
    if (rc < 0) {
        vlogE(TAG_CMD "Iterating comments failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    {
        // size_t left = MAX_CONTENT_LEN;
        cvector_vector_type(ReportedCmtInfo *) rcinfos_tmp = NULL;
        int i;

        if (!cvector_size(rcinfos)) {
            GetReportedCmtsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = true,
                    .rcinfos  = rcinfos
                }
            };
            resp_marshal = rpc_marshal_get_reported_cmts_resp(&resp);
            vlogD(TAG_CMD "Sending get_reported_comments empty response.");
            goto finally;
        }

        for (i = 0; i < cvector_size(rcinfos); ++i) {
            // left -= rcinfos[i]->len;
            cvector_push_back(rcinfos_tmp, rcinfos[i]);

            // if (!(!left || i == cvector_size(rcinfos) - 1 || rcinfos[i + 1]->len > left))
            //     continue;

            GetReportedCmtsResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .is_last = i == cvector_size(rcinfos) - 1,
                    .rcinfos  = rcinfos_tmp
                }
            };
            resp_marshal = rpc_marshal_get_reported_cmts_resp(&resp);

            vlogD(TAG_CMD "Sending get_reported_comments response.");

            rc = msgq_enq(from, resp_marshal);
            deref(resp_marshal);
            resp_marshal = NULL;
            if (rc < 0)
                break;

            cvector_set_size(rcinfos_tmp, 0);
            // left = MAX_CONTENT_LEN;
        }

        cvector_free(rcinfos_tmp);
    }

finally:
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
    if (rcinfos) {
        ReportedCmtInfo **i;
        cvector_foreach(rcinfos, i)
            deref(*i);
        cvector_free(rcinfos);
    }
    deref(uinfo);
    deref(chan);
    deref(it);
}

void hdl_unknown_req(Carrier *c, const char *from, Req *base)
{
    Marshalled *resp_marshal;
    ErrResp resp = {
        .tsx_id = base->tsx_id,
        .ec     = ERR_UNKNOWN_METHOD
    };

    vlogD(TAG_CMD "Received %s(unknown) request from [%s]", base->method, from);

    resp_marshal = rpc_marshal_err_resp(&resp);
    if (resp_marshal) {
        msgq_enq(from, resp_marshal);
        deref(resp_marshal);
    }
}

void feeds_deactivate_suber(const char *node_id)
{
    linked_list_iterator_t it;
    NotifDest *nd;
    NotifDestPerActiveSuber *ndpas;

    nd = nd_remove(node_id);
    if (!nd)
        return;

    list_foreach(nd->ndpass, ndpas) {
        ActiveSuber *as = ndpas->as;
        deref(linked_hashtable_remove(as->ndpass, node_id, strlen(node_id)));
        if (linked_hashtable_is_empty(as->ndpass)) {
            linked_hashtable_iterator_t it;
            ActiveSuberPerChan *aspc;

            hashtable_foreach(as->aspcs, aspc)
                deref(linked_list_remove_entry(aspc->chan->aspcs, &aspc->le));
            deref(as_remove(as->uid));
        }
    }

    deref(nd);
}

void hdl_stats_changed_notify()
{
    linked_hashtable_iterator_t it;
    NotifDest *nd;
    NotifDestPerActiveSuber *ndpas;
    int total_clients = db_get_user_count();

    hashtable_foreach(nds, nd) {
        linked_list_iterator_t it;
        list_foreach(nd->ndpass, ndpas) {
            ActiveSuber *as = ndpas->as;
            notify_of_stats_changed(ndpas->nd->node_id, total_clients);
        }
    }
}
