#include <assert.h>

#include <msgpack.h>
#include <crystal.h>
#include <ela_did.h>

#include "rpc.h"
#include "err.h"

#define map_sz     via.map.size
#define map_key(i) via.map.ptr[i].key
#define map_val(i) via.map.ptr[i].val
#define arr_sz     via.array.size
#define arr_val(i) via.array.ptr[i]
#define str_sz     via.str.size
#define str_val    via.str.ptr
#define i64_val    via.i64
#define u64_val    via.u64
#define bin_val    via.bin.ptr
#define bin_sz     via.bin.size
#define bool_val   via.boolean

static msgpack_unpacked msgpack;

static inline
bool map_key_correct(const msgpack_object *map, size_t idx, const char *key)
{
    return idx < map->map_sz &&
           map->map_key(idx).type == MSGPACK_OBJECT_STR &&
           map->map_key(idx).str_sz == strlen(key) &&
           !memcmp(map->map_key(idx).str_val, key, strlen(key));
}

static inline
const msgpack_object *map_nxt_val_str(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_STR) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

static inline
const msgpack_object *map_nxt_val_i64(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_NEGATIVE_INTEGER) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

static inline
const msgpack_object *map_nxt_val_u64(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_POSITIVE_INTEGER) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

static inline
const msgpack_object *map_nxt_val_map(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_MAP) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

static inline
const msgpack_object *map_nxt_val_arr(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_ARRAY) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

static inline
const msgpack_object *map_nxt_val_bin(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_BIN) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

static inline
const msgpack_object *map_nxt_val_bool(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_BOOLEAN) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

static inline
const msgpack_object *map_nxt_val_nil(const msgpack_object *map, size_t *nxt_idx, const char *key)
{
    return (map && map_key_correct(map, *nxt_idx, key) &&
            map->map_val(*nxt_idx).type == MSGPACK_OBJECT_NIL) ?
           &map->map_val((*nxt_idx)++) : NULL;
}

#define map_iter_kvs(map, it)                  \
    do {                                       \
        const msgpack_object *__tmp = (map);   \
        {                                      \
            const msgpack_object *__m = __tmp; \
            size_t __i = 0;                    \
            it                                 \
        }                                      \
    } while (0)

#define map_val_str(key)  map_nxt_val_str(__m, &__i, key)
#define map_val_i64(key)  map_nxt_val_i64(__m, &__i, key)
#define map_val_u64(key)  map_nxt_val_u64(__m, &__i, key)
#define map_val_map(key)  map_nxt_val_map(__m, &__i, key)
#define map_val_arr(key)  map_nxt_val_arr(__m, &__i, key)
#define map_val_bin(key)  map_nxt_val_bin(__m, &__i, key)
#define map_val_bool(key) map_nxt_val_bool(__m, &__i, key)
#define map_val_nil(key)  map_nxt_val_nil(__m, &__i, key)

static inline
const msgpack_object *arr_val_map(const msgpack_object *arr, size_t idx)
{
    return (arr && arr->arr_val(idx).type == MSGPACK_OBJECT_MAP) ?
           &arr->arr_val(idx) : NULL;
}

static inline
size_t str_reserve_spc(const msgpack_object *str)
{
    return str ? str->str_sz + 1 : 0;
}

static
int unmarshal_decl_owner_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *nonce;
    const msgpack_object *owner_did;
    DeclOwnerReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            nonce     = map_val_str("nonce");
            owner_did = map_val_str("owner_did");
        });
    });

    if (!nonce || !nonce->str_sz || !owner_did || !owner_did->str_sz) {
        vlogE("Invalid declare_owner request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(DeclOwnerReq) + str_reserve_spc(method) +
                    str_reserve_spc(nonce) + str_reserve_spc(owner_did), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.nonce     = strncpy(buf, nonce->str_val, nonce->str_sz);
    buf += str_reserve_spc(nonce);
    tmp->params.owner_did = strncpy(buf, owner_did->str_val, owner_did->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_imp_did_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *mnemo;
    const msgpack_object *passphrase;
    const msgpack_object *idx;
    ImpDIDReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            mnemo      = map_val_str("mnemonic");
            passphrase = map_val_str("passphrase");
            idx        = map_val_u64("index");
        });
    });

    tmp = rc_zalloc(sizeof(ImpDIDReq) + str_reserve_spc(method) +
                    str_reserve_spc(mnemo) + str_reserve_spc(passphrase), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id = tsx_id->u64_val;

    if (mnemo)
        tmp->params.mnemo = strncpy(buf, mnemo->str_val, mnemo->str_sz);

    if (passphrase) {
        buf += str_reserve_spc(mnemo);
        tmp->params.passphrase = strncpy(buf, passphrase->str_val, passphrase->str_sz);
    }

    if (idx)
        tmp->params.idx = idx->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_iss_vc_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *vc;
    IssVCReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            vc = map_val_str("credential");
        });
    });

    if (!vc || !vc->str_sz) {
        vlogE("Invalid issue_credential request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(IssVCReq) + str_reserve_spc(method) +
                    str_reserve_spc(vc), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.vc = strncpy(buf, vc->str_val, vc->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_signin_req_chal_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *iss;
    const msgpack_object *vc_req;
    SigninReqChalReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            iss    = map_val_str("iss");
            vc_req = map_val_bool("credential_required");
        });
    });

    if (!iss || !iss->str_sz || !vc_req) {
        vlogE("Invalid signin_request_challenge request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(SigninReqChalReq) + str_reserve_spc(method) +
                    str_reserve_spc(iss), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method        = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id        = tsx_id->u64_val;
    tmp->params.iss    = strncpy(buf, iss->str_val, iss->str_sz);
    tmp->params.vc_req = vc_req->bool_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_signin_conf_chal_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *jws;
    const msgpack_object *vc;
    SigninConfChalReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            jws = map_val_str("jws");
            vc  = map_val_str("credential");
        });
    });

    if (!jws || !jws->str_sz || (vc && !vc->str_sz))
        return -1;

    tmp = rc_zalloc(sizeof(SigninConfChalReq) + str_reserve_spc(method) +
                    str_reserve_spc(jws) + str_reserve_spc(vc), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method     = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id     = tsx_id->u64_val;
    tmp->params.jws = strncpy(buf, jws->str_val, jws->str_sz);

    if (vc) {
        buf += str_reserve_spc(jws);
        tmp->params.vc = strncpy(buf, vc->str_val, vc->str_sz);
    }

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_create_chan_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *name;
    const msgpack_object *intro;
    CreateChanReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk    = map_val_str("access_token");
            name  = map_val_str("name");
            intro = map_val_str("introduction");
        });
    });

    if (!tk || !tk->str_sz || !name || !name->str_sz || !intro || !intro->str_sz) {
        vlogE("Invalid create_channel request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(CreateChanReq) + str_reserve_spc(method) +
                    str_reserve_spc(tk) + str_reserve_spc(name) +
                    str_reserve_spc(intro), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method       = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id       = tsx_id->u64_val;
    tmp->params.tk    = strncpy(buf, tk->str_val, tk->str_sz);
    buf += str_reserve_spc(tk);
    tmp->params.name  = strncpy(buf, name->str_val, name->str_sz);
    buf += str_reserve_spc(name);
    tmp->params.intro = strncpy(buf, intro->str_val, intro->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_pub_post_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *content;
    PubPostReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            content = map_val_bin("content");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !content || !content->bin_sz) {
        vlogE("Invalid publish_post request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(PubPostReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.content = (void *)content->bin_val;
    tmp->params.sz      = content->bin_sz;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_post_cmt_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *cmt_id;
    const msgpack_object *content;
    PostCmtReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
            cmt_id  = map_val_u64("comment_id");
            content = map_val_bin("content");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) ||
        !cmt_id || !content || !content->bin_sz) {
        vlogE("Invalid post_comment request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostCmtReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;
    tmp->params.content = (void *)content->bin_val;
    tmp->params.sz      = content->bin_sz;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_post_like_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *cmt_id;
    PostLikeReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
            cmt_id  = map_val_u64("comment_id");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) || !cmt_id) {
        vlogE("Invalid post_like request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostLikeReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_my_chans_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetMyChansReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk     = map_val_str("access_token");
            by     = map_val_u64("by");
            upper  = map_val_u64("upper_bound");
            lower  = map_val_u64("lower_bound");
            maxcnt = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz || !by || !qry_fld_is_valid(by->u64_val) || !upper || !lower || !maxcnt) {
        vlogE("Invalid get_my_channels request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetMyChansReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.tk        = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.qc.by     = by->u64_val;
    tmp->params.qc.upper  = upper->u64_val;
    tmp->params.qc.lower  = lower->u64_val;
    tmp->params.qc.maxcnt = maxcnt->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_my_chans_meta_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetMyChansMetaReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk     = map_val_str("access_token");
            by     = map_val_u64("by");
            upper  = map_val_u64("upper_bound");
            lower  = map_val_u64("lower_bound");
            maxcnt = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz || !by || !qry_fld_is_valid(by->u64_val) || !upper || !lower || !maxcnt) {
        vlogE("Invalid get_my_channels_metadata request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetMyChansMetaReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.tk        = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.qc.by     = by->u64_val;
    tmp->params.qc.upper  = upper->u64_val;
    tmp->params.qc.lower  = lower->u64_val;
    tmp->params.qc.maxcnt = maxcnt->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_chans_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetChansReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk     = map_val_str("access_token");
            by     = map_val_u64("by");
            upper  = map_val_u64("upper_bound");
            lower  = map_val_u64("lower_bound");
            maxcnt = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz || !by || !qry_fld_is_valid(by->u64_val) || !upper || !lower || !maxcnt) {
        vlogE("Invalid get_channels request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChansReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.tk        = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.qc.by     = by->u64_val;
    tmp->params.qc.upper  = upper->u64_val;
    tmp->params.qc.lower  = lower->u64_val;
    tmp->params.qc.maxcnt = maxcnt->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_chan_dtl_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *id;
    GetChanDtlReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
            id = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !id || !chan_id_is_valid(id->u64_val)) {
        vlogE("Invalid get_channel_detail request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChanDtlReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.id = id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_sub_chans_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetSubChansReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk     = map_val_str("access_token");
            by     = map_val_u64("by");
            upper  = map_val_u64("upper_bound");
            lower  = map_val_u64("lower_bound");
            maxcnt = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz || !by || !qry_fld_is_valid(by->u64_val) || !upper || !lower || !maxcnt) {
        vlogE("Invalid get_subscribed_channels request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetSubChansReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.tk        = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.qc.by     = by->u64_val;
    tmp->params.qc.upper  = upper->u64_val;
    tmp->params.qc.lower  = lower->u64_val;
    tmp->params.qc.maxcnt = maxcnt->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_posts_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetPostsReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            by      = map_val_u64("by");
            upper   = map_val_u64("upper_bound");
            lower   = map_val_u64("lower_bound");
            maxcnt  = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !by || !qry_fld_is_valid(by->u64_val) || !upper || !lower || !maxcnt) {
        vlogE("Invalid get_posts request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetPostsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.tk        = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id   = chan_id->u64_val;
    tmp->params.qc.by     = by->u64_val;
    tmp->params.qc.upper  = upper->u64_val;
    tmp->params.qc.lower  = lower->u64_val;
    tmp->params.qc.maxcnt = maxcnt->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_cmts_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetCmtsReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
            by      = map_val_u64("by");
            upper   = map_val_u64("upper_bound");
            lower   = map_val_u64("lower_bound");
            maxcnt  = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) ||
        !by || !qry_fld_is_valid(by->u64_val) || !upper || !lower || !maxcnt) {
        vlogE("Invalid get_comments request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetCmtsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = &tmp->params + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.tk        = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id   = chan_id->u64_val;
    tmp->params.post_id   = post_id->u64_val;
    tmp->params.qc.by     = by->u64_val;
    tmp->params.qc.upper  = upper->u64_val;
    tmp->params.qc.lower  = lower->u64_val;
    tmp->params.qc.maxcnt = maxcnt->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_stats_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    GetStatsReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
        });
    });

    if (!tk || !tk->str_sz) {
        vlogE("Invalid get_statistics request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetStatsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_sub_chan_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *id;
    SubChanReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
            id = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !id || !chan_id_is_valid(id->u64_val)) {
        vlogE("Invalid subscribe_channel request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(SubChanReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.id = id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_unsub_chan_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *id;
    UnsubChanReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
            id = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !id || !chan_id_is_valid(id->u64_val)) {
        vlogE("Invalid unsubscribe_channel request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(UnsubChanReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.id = id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_enbl_notif_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    EnblNotifReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
        });
    });

    if (!tk || !tk->str_sz) {
        vlogE("Invalid enable_notification request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(EnblNotifReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

typedef int ReqHdlr(const msgpack_object *req_map, Req **req_unmarshal);
static struct {
    char *method;
    ReqHdlr *parser;
} req_parsers[] = {
    {"declare_owner"           , unmarshal_decl_owner_req       },
    {"import_did"              , unmarshal_imp_did_req          },
    {"issue_credential"        , unmarshal_iss_vc_req           },
    {"signin_request_challenge", unmarshal_signin_req_chal_req  },
    {"signin_confirm_challenge", unmarshal_signin_conf_chal_req },
    {"create_channel"          , unmarshal_create_chan_req      },
    {"publish_post"            , unmarshal_pub_post_req         },
    {"post_comment"            , unmarshal_post_cmt_req         },
    {"post_like"               , unmarshal_post_like_req        },
    {"get_my_channels"         , unmarshal_get_my_chans_req     },
    {"get_my_channels_metadata", unmarshal_get_my_chans_meta_req},
    {"get_channels"            , unmarshal_get_chans_req        },
    {"get_channel_detail"      , unmarshal_get_chan_dtl_req     },
    {"get_subscribed_channels" , unmarshal_get_sub_chans_req    },
    {"get_posts"               , unmarshal_get_posts_req        },
    {"get_comments"            , unmarshal_get_cmts_req         },
    {"get_statistics"          , unmarshal_get_stats_req        },
    {"subscribe_channel"       , unmarshal_sub_chan_req         },
    {"unsubscribe_channel"     , unmarshal_unsub_chan_req       },
    {"enable_notification"     , unmarshal_enbl_notif_req       },
};

int rpc_unmarshal_req(const void *rpc, size_t len, Req **req)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    char method_str[1024];
    msgpack_object obj;
    int i;

    msgpack_unpacked_init(&msgpack);
    if (msgpack_unpack_next(&msgpack, rpc, len, NULL) != MSGPACK_UNPACK_SUCCESS) {
        vlogE("Decoding msgpack failed.");
        return -1;
    }

    obj = msgpack.data;
    if (obj.type != MSGPACK_OBJECT_MAP) {
        vlogE("Not a msgpack map.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    map_iter_kvs(&obj, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
    });

    if (!jsonrpc || jsonrpc->str_sz != strlen("2.0") ||
        memcmp(jsonrpc->str_val, "2.0", jsonrpc->str_sz) ||
        !method || !method->str_sz || !tsx_id) {
        vlogE("No jsonrpc/method/id field.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    memset(method_str, 0, sizeof(method_str));
    strncpy(method_str, method->str_val, method->str_sz);

    for (i = 0; i < sizeof(req_parsers) / sizeof(req_parsers[0]); ++i) {
        if (!strcmp(method_str, req_parsers[i].method)) {
            int rc = req_parsers[i].parser(&obj, req);
            msgpack_unpacked_destroy(&msgpack);
            return rc;
        }
    }

    vlogE("Not a valid method.");
    msgpack_unpacked_destroy(&msgpack);
    return -1;
}

typedef struct {
    NewPostNotif notif;
    PostInfo pi;
} NewPostNotifWithPInfo;

static
int unmarshal_new_post_notif(const msgpack_object *notif, Notif **notif_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *content;
    const msgpack_object *created_at;
    NewPostNotifWithPInfo *tmp;
    void *buf;

    assert(notif->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(notif, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        map_iter_kvs(map_val_map("params"), {
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("id");
            content = map_val_bin("content");
            created_at = map_val_u64("created_at");
        });
    });

    if (!chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) ||
        !content || !content->bin_sz || !created_at) {
        vlogE("Invalid new_post notification.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NewPostNotifWithPInfo) + str_reserve_spc(method) +
                    content->bin_sz, NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->notif.method                   = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->notif.params.pinfo             = &tmp->pi;
    tmp->notif.params.pinfo->chan_id    = chan_id->u64_val;
    tmp->notif.params.pinfo->post_id    = post_id->u64_val;
    tmp->notif.params.pinfo->content    = memcpy(buf, content->bin_val, content->bin_sz);
    tmp->notif.params.pinfo->len        = content->bin_sz;
    tmp->notif.params.pinfo->created_at = created_at->u64_val;

    *notif_unmarshal = (Notif *)&tmp->notif;
    return 0;
}

typedef struct {
    NewCmtNotif notif;
    CmtInfo ci;
} NewCmtNotifWithCInfo;

static
int unmarshal_new_cmt_notif(const msgpack_object *notif, Notif **notif_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *id;
    const msgpack_object *cmt_id;
    const msgpack_object *user_name;
    const msgpack_object *content;
    const msgpack_object *created_at;
    NewCmtNotifWithCInfo *tmp;
    void *buf;

    assert(notif->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(notif, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        map_iter_kvs(map_val_map("params"), {
            chan_id    = map_val_u64("channel_id");
            post_id    = map_val_u64("post_id");
            id         = map_val_u64("id");
            cmt_id     = map_val_u64("comment_id");
            user_name  = map_val_str("user_name");
            content    = map_val_bin("content");
            created_at = map_val_u64("created_at");
        });
    });

    if (!chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) ||
        !id || !cmt_id_is_valid(id->u64_val) ||
        !cmt_id || !user_name || !user_name->str_sz ||
        !content || !content->bin_sz || !created_at) {
        vlogE("Invalid new_comment notification.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NewCmtNotifWithCInfo) + str_reserve_spc(method) +
                    str_reserve_spc(user_name) + content->bin_sz, NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->notif.method                     = strncpy(buf, method->str_val, method->str_sz);
    buf += str_reserve_spc(method);
    tmp->notif.params.cinfo               = &tmp->ci;
    tmp->notif.params.cinfo->chan_id      = chan_id->u64_val;
    tmp->notif.params.cinfo->post_id      = post_id->u64_val;
    tmp->notif.params.cinfo->cmt_id       = id->u64_val;
    tmp->notif.params.cinfo->reply_to_cmt = cmt_id->u64_val;
    tmp->notif.params.cinfo->user.name    = strncpy(buf, user_name->str_val, user_name->str_sz);
    buf += str_reserve_spc(user_name);
    tmp->notif.params.cinfo->content      = memcpy(buf, content->bin_val, content->bin_sz);
    tmp->notif.params.cinfo->len          = content->bin_sz;
    tmp->notif.params.cinfo->created_at   = created_at->u64_val;

    *notif_unmarshal = (Notif *)&tmp->notif;
    return 0;
}

static
int unmarshal_new_likes_notif(const msgpack_object *notif, Notif **notif_unmarshal)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *method;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *cmt_id;
    const msgpack_object *cnt;
    NewLikesNotif *tmp;
    void *buf;

    assert(notif->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(notif, {
        jsonrpc = map_val_str("jsonrpc");
        method  = map_val_str("method");
        map_iter_kvs(map_val_map("params"), {
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
            cmt_id  = map_val_u64("comment_id");
            cnt     = map_val_u64("count");
        });
    });

    if (!chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) || !cnt) {
        vlogE("Invalid new_likes notification.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NewLikesNotif) + str_reserve_spc(method), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;
    tmp->params.cnt     = cnt->u64_val;

    *notif_unmarshal = (Notif *)tmp;
    return 0;
}

typedef int NotifHdlr(const msgpack_object *notif_map, Notif **notif_unmarshal);
static struct {
    char *method;
    NotifHdlr *parser;
} notif_parsers[] = {
    {"new_post"   , unmarshal_new_post_notif },
    {"new_comment", unmarshal_new_cmt_notif  },
    {"new_likes"  , unmarshal_new_likes_notif},
};

int rpc_unmarshal_notif_or_resp_id(const void *rpc, size_t len, Notif **notif, uint64_t *resp_id)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *method;
    char method_str[1024];
    msgpack_object obj;
    int i;

    msgpack_unpacked_init(&msgpack);
    if (msgpack_unpack_next(&msgpack, rpc, len, NULL) != MSGPACK_UNPACK_SUCCESS) {
        vlogE("Decoding msgpack failed.");
        return -1;
    }

    obj = msgpack.data;
    if (obj.type != MSGPACK_OBJECT_MAP) {
        vlogE("Not a msgpack map.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    map_iter_kvs(&obj, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        method  = map_val_str("method");
    });

    if (!jsonrpc || jsonrpc->str_sz != strlen("2.0") ||
        memcmp(jsonrpc->str_val, "2.0", jsonrpc->str_sz) ||
        !!tsx_id + !!method != 1) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    if (tsx_id) {
        *notif = NULL;
        *resp_id = tsx_id->u64_val;
        return 0;
    }

    memset(method_str, 0, sizeof(method_str));
    strncpy(method_str, method->str_val, method->str_sz);

    for (i = 0; i < sizeof(notif_parsers) / sizeof(notif_parsers[0]); ++i) {
        if (!strcmp(method_str, notif_parsers[i].method)) {
            int rc = notif_parsers[i].parser(&obj, notif);
            msgpack_unpacked_destroy(&msgpack);
            return rc;
        }
    }

    vlogE("Not a valid method.");
    msgpack_unpacked_destroy(&msgpack);
    return -1;
}

static
int rpc_unmarshal_err_resp(ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *code;
    ErrResp *tmp;

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("error"), {
            code = map_val_i64("code");
        });
    });

    if (!code)
        return -1;

    tmp = rc_zalloc(sizeof(ErrResp), NULL);
    if (!tmp)
        return -1;

    tmp->tsx_id = tsx_id->u64_val;
    tmp->ec     = code->i64_val;

    *err = tmp;
    return 0;
}

int rpc_unmarshal_decl_owner_resp(DeclOwnerResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *phase;
    const msgpack_object *did;
    const msgpack_object *tsx_payload;
    DeclOwnerResp *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            phase       = map_val_str("phase");
            did         = map_val_str("did");
            tsx_payload = map_val_str("transaction_payload");
        });
    });

    if (!phase || !(!strcmp(phase->str_val, "did_imported") ?
                    (did && tsx_payload && tsx_payload->str_sz) :
                    (!did && !tsx_payload))) {
        vlogE("Invalid declare_owner response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(DeclOwnerResp) + str_reserve_spc(phase) +
                    str_reserve_spc(did) + str_reserve_spc(tsx_payload), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    buf = tmp + 1;
    tmp->tsx_id       = tsx_id->u64_val;
    tmp->result.phase = strncpy(buf, phase->str_val, phase->str_sz);

    if (did) {
        buf += str_reserve_spc(phase);
        tmp->result.did         = strncpy(buf, did->str_val, did->str_sz);
        buf += str_reserve_spc(did);
        tmp->result.tsx_payload = strncpy(buf, tsx_payload->str_val, tsx_payload->str_sz);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_imp_did_resp(ImpDIDResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *did;
    const msgpack_object *tsx_payload;
    ImpDIDResp *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            did         = map_val_str("did");
            tsx_payload = map_val_str("transaction_payload");
        });
    });

    if (!did || !tsx_payload || !tsx_payload->str_sz) {
        vlogE("Invalid import_did response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(ImpDIDResp) + str_reserve_spc(did) +
                    str_reserve_spc(tsx_payload), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    buf = tmp + 1;
    tmp->tsx_id             = tsx_id->u64_val;
    tmp->result.did         = strncpy(buf, did->str_val, did->str_sz);
    buf += str_reserve_spc(did);
    tmp->result.tsx_payload = strncpy(buf, tsx_payload->str_val, tsx_payload->str_sz);

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_iss_vc_resp(IssVCResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    IssVCResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
    });

    tmp = rc_zalloc(sizeof(IssVCResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_signin_req_chal_resp(SigninReqChalResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *vc_req;
    const msgpack_object *jws;
    const msgpack_object *vc;
    SigninReqChalResp *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            vc_req = map_val_bool("credential_required");
            jws    = map_val_str("jws");
            vc     = map_val_str("credential");
        });
    });

    if (!vc_req || !jws || !jws->str_sz || (vc && !vc->str_sz)) {
        vlogE("Invalid sigin_request_challenge response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(SigninReqChalResp) + str_reserve_spc(jws) +
                    str_reserve_spc(vc), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    buf = tmp + 1;
    tmp->tsx_id        = tsx_id->u64_val;
    tmp->result.vc_req = vc_req->bool_val;
    tmp->result.jws    = strncpy(buf, jws->str_val, jws->str_sz);

    if (vc) {
        buf += str_reserve_spc(jws);
        tmp->result.vc = strncpy(buf, vc->str_val, vc->str_sz);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_signin_conf_chal_resp(SigninConfChalResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *exp;
    SigninConfChalResp *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            tk  = map_val_str("access_token");
            exp = map_val_u64("exp");
        });
    });

    if (!tk || !tk->str_sz || !exp) {
        vlogE("Invalid signin_confirm_challenge response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(SigninConfChalResp) + str_reserve_spc(tk), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    buf = tmp + 1;
    tmp->tsx_id     = tsx_id->u64_val;
    tmp->result.tk  = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->result.exp = exp->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_create_chan_resp(CreateChanResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *chan_id;
    CreateChanResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            chan_id = map_val_u64("id");
        });
    });

    if (!chan_id) {
        vlogE("Invalid create_channel response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(CreateChanResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id    = tsx_id->u64_val;
    tmp->result.id = chan_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_pub_post_resp(PubPostResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *post_id;
    PubPostResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            post_id = map_val_u64("id");
        });
    });

    if (!post_id) {
        vlogE("Invalid publish_post response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(PubPostResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id    = tsx_id->u64_val;
    tmp->result.id = post_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_post_cmt_resp(PostCmtResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *cmt_id;
    PostCmtResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            cmt_id = map_val_u64("id");
        });
    });

    if (!cmt_id) {
        vlogE("Invalid post_comment response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostCmtResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id    = tsx_id->u64_val;
    tmp->result.id = cmt_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_post_like_resp(PostLikeResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    PostLikeResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE("Invalid post_like response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostLikeResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

static
void get_my_chans_resp_dtor(void *obj)
{
    GetMyChansResp *resp = obj;
    ChanInfo **ci;

    cvector_foreach(resp->result.cinfos, ci)
        deref(*ci);
    cvector_free(resp->result.cinfos);
}

int rpc_unmarshal_get_my_chans_resp(GetMyChansResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    GetMyChansResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_arr("result");
    });

    if (!result) {
        vlogE("Invalid get_my_channels response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetMyChansResp), get_my_chans_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    for (i = 0; i < result->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *name;
        const msgpack_object *intro;
        const msgpack_object *subs;
        ChanInfo *ci;
        void *buf;

        map_iter_kvs(arr_val_map(result, i), {
            chan_id = map_val_u64("id");
            name    = map_val_str("name");
            intro   = map_val_str("introduction");
            subs    = map_val_u64("subscribers");
        });

        if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz || !subs) {
            vlogE("Invalid get_my_channels response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(ChanInfo) + str_reserve_spc(name) + str_reserve_spc(intro), NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = ci + 1;
        ci->chan_id = chan_id->u64_val;
        ci->name    = strncpy(buf, name->str_val, name->str_sz);
        buf += str_reserve_spc(name);
        ci->intro   = strncpy(buf, intro->str_val, intro->str_sz);
        ci->subs    = subs->u64_val;

        cvector_push_back(tmp->result.cinfos, ci);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

static
void get_my_chans_meta_resp_dtor(void *obj)
{
    GetMyChansMetaResp *resp = obj;
    ChanInfo **ci;

    cvector_foreach(resp->result.cinfos, ci)
        deref(*ci);
    cvector_free(resp->result.cinfos);
}

int rpc_unmarshal_get_my_chans_meta_resp(GetMyChansMetaResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    GetMyChansMetaResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_arr("result");
    });

    if (!result) {
        vlogE("Invalid get_my_channels_metadata response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetMyChansMetaResp), get_my_chans_meta_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    for (i = 0; i < result->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *subs;
        ChanInfo *ci;

        map_iter_kvs(arr_val_map(result, i), {
            chan_id = map_val_u64("id");
            subs    = map_val_u64("subscribers");
        });

        if (!chan_id || !subs) {
            vlogE("Invalid get_my_channels_metadata response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(ChanInfo), NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci->chan_id = chan_id->u64_val;
        ci->subs    = subs->u64_val;

        cvector_push_back(tmp->result.cinfos, ci);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

typedef struct {
    ChanInfo ci;
    UserInfo owner;
} ChanInfoWithOwner;

static
void get_chans_resp_dtor(void *obj)
{
    GetChansResp *resp = obj;
    ChanInfo **ci;

    cvector_foreach(resp->result.cinfos, ci) {
        ChanInfoWithOwner *ciwo = (ChanInfoWithOwner *)(*ci);
        deref(ciwo);
    }
    cvector_free(resp->result.cinfos);
}

int rpc_unmarshal_get_chans_resp(GetChansResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    GetChansResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_arr("result");
    });

    if (!result) {
        vlogE("Invalid get_channels response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChansResp), get_chans_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    for (i = 0; i < result->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *name;
        const msgpack_object *intro;
        const msgpack_object *owner_name;
        const msgpack_object *owner_did;
        const msgpack_object *subs;
        const msgpack_object *upd_at;
        ChanInfoWithOwner *ci;
        void *buf;

        map_iter_kvs(arr_val_map(result, i), {
            chan_id    = map_val_u64("id");
            name       = map_val_str("name");
            intro      = map_val_str("introduction");
            owner_name = map_val_str("owner_name");
            owner_did  = map_val_str("owner_did");
            subs       = map_val_u64("subscribers");
            upd_at     = map_val_u64("last_update");
        });

        if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz ||
            !owner_name || !owner_name->str_sz || !owner_did || !owner_did->str_sz ||
            !subs || !upd_at) {
            vlogE("Invalid get_channels response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(ChanInfoWithOwner) + str_reserve_spc(name) +
                       str_reserve_spc(intro) + str_reserve_spc(owner_name) +
                       str_reserve_spc(owner_did), NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = ci + 1;
        ci->ci.chan_id     = chan_id->u64_val;
        ci->ci.name        = strncpy(buf, name->str_val, name->str_sz);
        buf += str_reserve_spc(name);
        ci->ci.intro       = strncpy(buf, intro->str_val, intro->str_sz);
        buf += str_reserve_spc(intro);
        ci->ci.owner       = &ci->owner;
        ci->ci.owner->name = strncpy(buf, owner_name->str_val, owner_name->str_sz);
        buf += str_reserve_spc(owner_name);
        ci->ci.owner->did  = strncpy(buf, owner_did->str_val, owner_did->str_sz);
        ci->ci.subs        = subs->u64_val;
        ci->ci.upd_at      = upd_at->u64_val;

        cvector_push_back(tmp->result.cinfos, &ci->ci);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

typedef struct {
    GetChanDtlResp resp;
    ChanInfoWithOwner ci;
} GetChanDtlRespWithChanInfo;

int rpc_unmarshal_get_chan_dtl_resp(GetChanDtlResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *chan_id;
    const msgpack_object *name;
    const msgpack_object *intro;
    const msgpack_object *owner_name;
    const msgpack_object *owner_did;
    const msgpack_object *subs;
    const msgpack_object *upd_at;
    GetChanDtlRespWithChanInfo *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            chan_id    = map_val_u64("id");
            name       = map_val_str("name");
            intro      = map_val_str("introduction");
            owner_name = map_val_str("owner_name");
            owner_did  = map_val_str("owner_did");
            subs       = map_val_u64("subscribers");
            upd_at     = map_val_u64("last_update");
        });
    });

    if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz ||
        !owner_name || !owner_name->str_sz || !owner_did || !owner_did->str_sz ||
        !subs || !upd_at) {
        vlogE("Invalid get_channel_detail response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChanDtlRespWithChanInfo) + str_reserve_spc(name) +
                    str_reserve_spc(intro) + str_reserve_spc(owner_name) +
                    str_reserve_spc(owner_did), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    buf = tmp + 1;
    tmp->resp.tsx_id                    = tsx_id->u64_val;
    tmp->resp.result.cinfo              = &tmp->ci.ci;
    tmp->resp.result.cinfo->chan_id     = chan_id->u64_val;
    tmp->resp.result.cinfo->name        = strncpy(buf, name->str_val, name->str_sz);
    buf += str_reserve_spc(name);
    tmp->resp.result.cinfo->intro       = strncpy(buf, intro->str_val, intro->str_sz);
    buf += str_reserve_spc(intro);
    tmp->resp.result.cinfo->owner       = &tmp->ci.owner;
    tmp->resp.result.cinfo->owner->name = strncpy(buf, owner_name->str_val, owner_name->str_sz);
    buf += str_reserve_spc(owner_name);
    tmp->resp.result.cinfo->owner->did  = strncpy(buf, owner_did->str_val, owner_did->str_sz);
    tmp->resp.result.cinfo->subs        = subs->u64_val;
    tmp->resp.result.cinfo->upd_at      = upd_at->u64_val;

    *resp = &tmp->resp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

static
void get_sub_chans_resp_dtor(void *obj)
{
    GetSubChansResp *resp = obj;
    ChanInfo **ci;

    cvector_foreach(resp->result.cinfos, ci) {
        ChanInfoWithOwner *ciwo = (ChanInfoWithOwner *)(*ci);
        deref(ciwo);
    }
    cvector_free(resp->result.cinfos);
}

int rpc_unmarshal_get_sub_chans_resp(GetSubChansResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    GetSubChansResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_arr("result");
    });

    if (!result) {
        vlogE("Invalid get_subscribed_channels response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetSubChansResp), get_sub_chans_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    for (i = 0; i < result->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *name;
        const msgpack_object *intro;
        const msgpack_object *owner_name;
        const msgpack_object *owner_did;
        const msgpack_object *subs;
        const msgpack_object *upd_at;
        ChanInfoWithOwner *ci;
        void *buf;

        map_iter_kvs(arr_val_map(result, i), {
            chan_id    = map_val_u64("id");
            name       = map_val_str("name");
            intro      = map_val_str("introduction");
            owner_name = map_val_str("owner_name");
            owner_did  = map_val_str("owner_did");
            subs       = map_val_u64("subscribers");
            upd_at     = map_val_u64("last_update");
        });

        if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz ||
            !owner_name || !owner_name->str_sz || !owner_did || !owner_did->str_sz ||
            !subs || !upd_at) {
            vlogE("Invalid get_subscribed_channels response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(ChanInfoWithOwner) + str_reserve_spc(name) +
                       str_reserve_spc(intro) + str_reserve_spc(owner_name) +
                       str_reserve_spc(owner_did), NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = ci + 1;
        ci->ci.chan_id     = chan_id->u64_val;
        ci->ci.name        = strncpy(buf, name->str_val, name->str_sz);
        buf += str_reserve_spc(name);
        ci->ci.intro       = strncpy(buf, intro->str_val, intro->str_sz);
        buf += str_reserve_spc(intro);
        ci->ci.owner       = &ci->owner;
        ci->ci.owner->name = strncpy(buf, owner_name->str_val, owner_name->str_sz);
        buf += str_reserve_spc(owner_name);
        ci->ci.owner->did  = strncpy(buf, owner_did->str_val, owner_did->str_sz);
        ci->ci.subs        = subs->u64_val;
        ci->ci.upd_at      = upd_at->u64_val;

        cvector_push_back(tmp->result.cinfos, &ci->ci);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

static
void get_posts_resp_dtor(void *obj)
{
    GetPostsResp *resp = obj;
    PostInfo **pi;

    cvector_foreach(resp->result.pinfos, pi)
        deref(*pi);
    cvector_free(resp->result.pinfos);
}

int rpc_unmarshal_get_posts_resp(GetPostsResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    GetPostsResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_arr("result");
    });

    if (!result) {
        vlogE("Invalid get_posts response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetPostsResp), get_posts_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    for (i = 0; i < result->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *post_id;
        const msgpack_object *content;
        const msgpack_object *cmts;
        const msgpack_object *likes;
        const msgpack_object *created_at;
        PostInfo *pi;
        void *buf;

        map_iter_kvs(arr_val_map(result, i), {
            chan_id    = map_val_u64("channel_id");
            post_id    = map_val_u64("id");
            content    = map_val_bin("content");
            cmts       = map_val_u64("comments");
            likes      = map_val_u64("likes");
            created_at = map_val_u64("created_at");
        });

        if (!chan_id || !chan_id_is_valid(chan_id->u64_val) ||
            !post_id || !post_id_is_valid(post_id->u64_val) ||
            !content || !content->bin_sz || !cmts || !likes || !created_at) {
            vlogE("Invalid get_posts response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        pi = rc_zalloc(sizeof(PostInfo) + content->bin_sz, NULL);
        if (!pi) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = pi + 1;
        pi->chan_id    = chan_id->u64_val;
        pi->post_id    = post_id->u64_val;
        pi->content    = memcpy(buf, content->bin_val, content->bin_sz);
        pi->len        = content->bin_sz;
        pi->cmts       = cmts->u64_val;
        pi->likes      = likes->u64_val;
        pi->created_at = created_at->u64_val;

        cvector_push_back(tmp->result.pinfos, pi);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

static
void get_cmts_resp_dtor(void *obj)
{
    GetCmtsResp *resp = obj;
    CmtInfo **ci;

    cvector_foreach(resp->result.cinfos, ci)
        deref(*ci);
    cvector_free(resp->result.cinfos);
}

int rpc_unmarshal_get_cmts_resp(GetCmtsResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    GetCmtsResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_arr("result");
    });

    if (!result) {
        vlogE("Invalid get_comments response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetCmtsResp), get_cmts_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    for (i = 0; i < result->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *post_id;
        const msgpack_object *id;
        const msgpack_object *cmt_id;
        const msgpack_object *user_name;
        const msgpack_object *content;
        const msgpack_object *likes;
        const msgpack_object *created_at;
        CmtInfo *ci;
        void *buf;

        map_iter_kvs(arr_val_map(result, i), {
            chan_id    = map_val_u64("channel_id");
            post_id    = map_val_u64("post_id");
            id         = map_val_u64("id");
            cmt_id     = map_val_u64("comment_id");
            user_name  = map_val_str("user_name");
            content    = map_val_bin("content");
            likes      = map_val_u64("likes");
            created_at = map_val_u64("created_at");
        });

        if (!chan_id || !chan_id_is_valid(chan_id->u64_val) ||
            !post_id || !post_id_is_valid(post_id->u64_val) ||
            !id || !cmt_id_is_valid(id->u64_val) ||
            !user_name || !user_name->str_sz ||
            !content || !content->bin_sz || !likes || !created_at) {
            vlogE("Invalid get_comments response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(CmtInfo) + str_reserve_spc(user_name) +
                       content->bin_sz, NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = ci + 1;
        ci->chan_id      = chan_id->u64_val;
        ci->post_id      = post_id->u64_val;
        ci->cmt_id       = id->u64_val;
        ci->reply_to_cmt = cmt_id->u64_val;
        ci->user.name    = strncpy(buf, user_name->str_val, user_name->str_sz);
        buf += str_reserve_spc(user_name);
        ci->content      = memcpy(buf, content->bin_val, content->bin_sz);
        ci->len          = content->bin_sz;
        ci->likes        = likes->u64_val;
        ci->created_at   = created_at->u64_val;

        cvector_push_back(tmp->result.cinfos, ci);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_get_stats_resp(GetStatsResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *did;
    const msgpack_object *conn_cs;
    GetStatsResp *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            did     = map_val_str("did");
            conn_cs = map_val_u64("connecting_clients");
        });
    });

    if (!did || !did->str_sz || !conn_cs) {
        vlogE("Invalid get_statistics response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetStatsResp) + str_reserve_spc(did), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    buf = tmp + 1;
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->result.did     = strncpy(buf, did->str_val, did->str_sz);
    tmp->result.conn_cs = conn_cs->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_sub_chan_resp(SubChanResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    SubChanResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE("Invalid subscribe_channel response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(SubChanResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_unsub_chan_resp(UnsubChanResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    UnsubChanResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE("Invalid unsubscribe_channel response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(UnsubChanResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_enbl_notif_resp(EnblNotifResp **resp, ErrResp **err)
{
    const msgpack_object *jsonrpc;
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    EnblNotifResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        jsonrpc = map_val_str("jsonrpc");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE("Invalid enable_notification response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(EnblNotifResp), NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id = tsx_id->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

static inline
void pack_str(msgpack_packer* pk, const char *str)
{
    assert(pk);
    assert(str);

    msgpack_pack_str(pk, strlen(str));
    msgpack_pack_str_body(pk, str, strlen(str));
}

static inline
void pack_kv_str(msgpack_packer* pk, const char *k, const char *v)
{
    assert(pk);
    assert(k);

    if (v) {
        pack_str(pk, k);
        pack_str(pk, v);
    }
}

static inline
void pack_kv_i64(msgpack_packer* pk, const char *k, int64_t v)
{
    assert(pk);
    assert(k);

    pack_str(pk, k);
    msgpack_pack_int64(pk, v);
}

static inline
void pack_kv_u64(msgpack_packer* pk, const char *k, uint64_t v)
{
    assert(pk);
    assert(k);

    pack_str(pk, k);
    msgpack_pack_uint64(pk, v);
}

static inline
void pack_kv_nil(msgpack_packer* pk, const char *k)
{
    assert(pk);
    assert(k);

    pack_str(pk, k);
    msgpack_pack_nil(pk);
}

static inline
void pack_kv_bool(msgpack_packer* pk, const char *k, bool b)
{
    assert(pk);
    assert(k);

    pack_str(pk, k);
    b ? msgpack_pack_true(pk) : msgpack_pack_false(pk);
}

static inline
void pack_kv_bin(msgpack_packer* pk, const char *k, const void *bin, size_t sz)
{
    assert(pk);
    assert(k);

    if (bin && sz) {
        pack_str(pk, k);
        msgpack_pack_bin(pk, sz);
        msgpack_pack_bin_body(pk, bin, sz);
    }
}

#define pack_map(pk, kvs, set_kvs)     \
    do {                               \
        if (kvs) {                     \
            msgpack_pack_map(pk, kvs); \
            set_kvs                    \
        }                              \
    } while (0)

#define pack_arr(pk, elems, set_elems) \
    do {                               \
        msgpack_pack_array(pk, elems); \
        set_elems                      \
    } while (0)

#define pack_kv_map(pk, k, map_kvs, set_kvs) \
    do {                                     \
        if (map_kvs) {                       \
            pack_str(pk, k);                 \
            pack_map(pk, map_kvs, set_kvs);  \
        }                                    \
    } while (0)

#define pack_kv_arr(pk, k, elems, set_elems) \
    do {                                     \
        pack_str(pk, k);                     \
        pack_arr(pk, elems, set_elems);      \
    } while (0)

typedef struct {
    Marshalled m;
    msgpack_sbuffer *buf;
} MarshalledIntl;

static
void mintl_dtor(void *obj)
{
    MarshalledIntl *m = obj;

    if (m->buf)
        msgpack_sbuffer_free(m->buf);
}

Marshalled *rpc_marshal_decl_owner_req(const DeclOwnerReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "declare_owner");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 2, {
            pack_kv_str(pk, "nonce", req->params.nonce);
            pack_kv_str(pk, "owner_did", req->params.owner_did);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_decl_owner_resp(const DeclOwnerResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", resp->result.did ? 3 : 1, {
            pack_kv_str(pk, "phase", resp->result.phase);
            pack_kv_str(pk, "did", resp->result.did);
            pack_kv_str(pk, "transaction_payload", resp->result.tsx_payload);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_imp_did_req(const ImpDIDReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    int params_cnt = !!req->params.mnemo + !!req->params.passphrase + !!req->params.idx;

    pack_map(pk, params_cnt ? 4 : 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "import_did");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", params_cnt, {
            pack_kv_str(pk, "mnemonic", req->params.mnemo);
            pack_kv_str(pk, "passphrase", req->params.passphrase);
            pack_kv_u64(pk, "index", req->params.idx);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_imp_did_resp(const ImpDIDResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_str(pk, "did", resp->result.did);
            pack_kv_str(pk, "transaction_payload", resp->result.tsx_payload);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_iss_vc_req(const IssVCReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "issue_credential");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 1, {
            pack_kv_str(pk, "credential", req->params.vc);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_iss_vc_resp(const IssVCResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_signin_req_chal_req(const SigninReqChalReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "signin_request_challenge");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 2, {
            pack_kv_str(pk, "iss", req->params.iss);
            pack_kv_bool(pk, "credential_required", req->params.vc_req);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_signin_req_chal_resp(const SigninReqChalResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", resp->result.vc ? 3 : 2, {
            pack_kv_bool(pk, "credential_required", resp->result.vc_req);
            pack_kv_str(pk, "jws", resp->result.jws);
            pack_kv_str(pk, "credential", resp->result.vc);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_signin_conf_chal_req(const SigninConfChalReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "signin_confirm_challenge");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", req->params.vc ? 2 : 1, {
            pack_kv_str(pk, "jws", req->params.jws);
            pack_kv_str(pk, "credential", req->params.vc);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_signin_conf_chal_resp(const SigninConfChalResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_str(pk, "access_token", resp->result.tk);
            pack_kv_u64(pk, "exp", resp->result.exp);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_new_post_notif(const NewPostNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "new_post");
        pack_kv_map(pk, "params", 4, {
            pack_kv_u64(pk, "channel_id", notif->params.pinfo->chan_id);
            pack_kv_u64(pk, "id", notif->params.pinfo->post_id);
            pack_kv_bin(pk, "content", notif->params.pinfo->content, notif->params.pinfo->len);
            pack_kv_u64(pk, "created_at", notif->params.pinfo->created_at);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_new_cmt_notif(const NewCmtNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "new_comment");
        pack_kv_map(pk, "params", 7, {
            pack_kv_u64(pk, "channel_id", notif->params.cinfo->chan_id);
            pack_kv_u64(pk, "post_id", notif->params.cinfo->post_id);
            pack_kv_u64(pk, "id", notif->params.cinfo->cmt_id);
            pack_kv_u64(pk, "comment_id", notif->params.cinfo->reply_to_cmt);
            pack_kv_str(pk, "user_name", notif->params.cinfo->user.name);
            pack_kv_bin(pk, "content", notif->params.cinfo->content, notif->params.cinfo->len);
            pack_kv_u64(pk, "created_at", notif->params.cinfo->created_at);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_new_likes_notif(const NewLikesNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "new_likes");
        pack_kv_map(pk, "params", 4, {
            pack_kv_u64(pk, "channel_id", notif->params.chan_id);
            pack_kv_u64(pk, "post_id", notif->params.post_id);
            pack_kv_u64(pk, "comment_id", notif->params.cmt_id);
            pack_kv_u64(pk, "count", notif->params.cnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_err_resp(const ErrResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "error", 2, {
            pack_kv_i64(pk, "code", resp->ec);
            pack_kv_str(pk, "message", err_strerror(resp->ec));
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_create_chan_req(const CreateChanReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "create_channel");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 3, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_str(pk, "name", req->params.name);
            pack_kv_str(pk, "introduction", req->params.intro);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_create_chan_resp(const CreateChanResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 1, {
            pack_kv_u64(pk, "id", resp->result.id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_pub_post_req(const PubPostReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "publish_post");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 3, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_bin(pk, "content", req->params.content, req->params.sz);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_pub_post_resp(const PubPostResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 1, {
            pack_kv_u64(pk, "id", resp->result.id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_post_cmt_req(const PostCmtReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "post_comment");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 5, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_u64(pk, "post_id", req->params.post_id);
            pack_kv_u64(pk, "comment_id", req->params.cmt_id);
            pack_kv_bin(pk, "content", req->params.content, req->params.sz);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_post_cmt_resp(const PostCmtResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 1, {
            pack_kv_u64(pk, "id", resp->result.id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_post_like_req(const PostLikeReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "post_like");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 4, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_u64(pk, "post_id", req->params.post_id);
            pack_kv_u64(pk, "comment_id", req->params.cmt_id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_post_like_resp(const PostLikeResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_my_chans_req(const GetMyChansReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_my_channels");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 5, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "by", req->params.qc.by);
            pack_kv_u64(pk, "upper_bound", req->params.qc.upper);
            pack_kv_u64(pk, "lower_bound", req->params.qc.lower);
            pack_kv_u64(pk, "max_count", req->params.qc.maxcnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_my_chans_resp(const GetMyChansResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    ChanInfo **cinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_arr(pk, "result", cvector_size(resp->result.cinfos), {
            cvector_foreach(resp->result.cinfos, cinfo) {
                pack_map(pk, 4, {
                    pack_kv_u64(pk, "id", (*cinfo)->chan_id);
                    pack_kv_str(pk, "name", (*cinfo)->name);
                    pack_kv_str(pk, "introduction", (*cinfo)->intro);
                    pack_kv_u64(pk, "subscribers", (*cinfo)->subs);
                });
            }
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_my_chans_meta_req(const GetMyChansMetaReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_my_channels_metadata");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 5, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "by", req->params.qc.by);
            pack_kv_u64(pk, "upper_bound", req->params.qc.upper);
            pack_kv_u64(pk, "lower_bound", req->params.qc.lower);
            pack_kv_u64(pk, "max_count", req->params.qc.maxcnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_my_chans_meta_resp(const GetMyChansMetaResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    ChanInfo **cinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_arr(pk, "result", cvector_size(resp->result.cinfos), {
            cvector_foreach(resp->result.cinfos, cinfo) {
                pack_map(pk, 2, {
                    pack_kv_u64(pk, "id", (*cinfo)->chan_id);
                    pack_kv_u64(pk, "subscribers", (*cinfo)->subs);
                });
            }
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_chans_req(const GetChansReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_channels");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 5, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "by", req->params.qc.by);
            pack_kv_u64(pk, "upper_bound", req->params.qc.upper);
            pack_kv_u64(pk, "lower_bound", req->params.qc.lower);
            pack_kv_u64(pk, "max_count", req->params.qc.maxcnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_chans_resp(const GetChansResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    ChanInfo **cinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_arr(pk, "result", cvector_size(resp->result.cinfos), {
            cvector_foreach(resp->result.cinfos, cinfo) {
                pack_map(pk, 7, {
                    pack_kv_u64(pk, "id", (*cinfo)->chan_id);
                    pack_kv_str(pk, "name", (*cinfo)->name);
                    pack_kv_str(pk, "introduction", (*cinfo)->intro);
                    pack_kv_str(pk, "owner_name", (*cinfo)->owner->name);
                    pack_kv_str(pk, "owner_did", (*cinfo)->owner->did);
                    pack_kv_u64(pk, "subscribers", (*cinfo)->subs);
                    pack_kv_u64(pk, "last_update", (*cinfo)->upd_at);
                });
            }
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_chan_dtl_req(const GetChanDtlReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_channel_detail");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 2, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "id", req->params.id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_chan_dtl_resp(const GetChanDtlResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 7, {
            pack_kv_u64(pk, "id", resp->result.cinfo->chan_id);
            pack_kv_str(pk, "name", resp->result.cinfo->name);
            pack_kv_str(pk, "introduction", resp->result.cinfo->intro);
            pack_kv_str(pk, "owner_name", resp->result.cinfo->owner->name);
            pack_kv_str(pk, "owner_did", resp->result.cinfo->owner->did);
            pack_kv_u64(pk, "subscribers", resp->result.cinfo->subs);
            pack_kv_u64(pk, "last_update", resp->result.cinfo->upd_at);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_sub_chans_req(const GetSubChansReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_subscribed_channels");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 5, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "by", req->params.qc.by);
            pack_kv_u64(pk, "upper_bound", req->params.qc.upper);
            pack_kv_u64(pk, "lower_bound", req->params.qc.lower);
            pack_kv_u64(pk, "max_count", req->params.qc.maxcnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_sub_chans_resp(const GetSubChansResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    ChanInfo **cinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_arr(pk, "result", cvector_size(resp->result.cinfos), {
            cvector_foreach(resp->result.cinfos, cinfo) {
                pack_map(pk, 7, {
                    pack_kv_u64(pk, "id", (*cinfo)->chan_id);
                    pack_kv_str(pk, "name", (*cinfo)->name);
                    pack_kv_str(pk, "introduction", (*cinfo)->intro);
                    pack_kv_str(pk, "owner_name", (*cinfo)->owner->name);
                    pack_kv_str(pk, "owner_did", (*cinfo)->owner->did);
                    pack_kv_u64(pk, "subscribers", (*cinfo)->subs);
                    pack_kv_u64(pk, "last_update", (*cinfo)->upd_at);
                });
            }
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_posts_req(const GetPostsReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_posts");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 6, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_u64(pk, "by", req->params.qc.by);
            pack_kv_u64(pk, "upper_bound", req->params.qc.upper);
            pack_kv_u64(pk, "lower_bound", req->params.qc.lower);
            pack_kv_u64(pk, "max_count", req->params.qc.maxcnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_posts_resp(const GetPostsResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    PostInfo **pinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_arr(pk, "result", cvector_size(resp->result.pinfos), {
            cvector_foreach(resp->result.pinfos, pinfo) {
                pack_map(pk, 6, {
                    pack_kv_u64(pk, "channel_id", (*pinfo)->chan_id);
                    pack_kv_u64(pk, "id", (*pinfo)->post_id);
                    pack_kv_bin(pk, "content", (*pinfo)->content, (*pinfo)->len);
                    pack_kv_u64(pk, "comments", (*pinfo)->cmts);
                    pack_kv_u64(pk, "likes", (*pinfo)->likes);
                    pack_kv_u64(pk, "created_at", (*pinfo)->created_at);
                });
            }
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_cmts_req(const GetCmtsReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_comments");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 7, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_u64(pk, "post_id", req->params.post_id);
            pack_kv_u64(pk, "by", req->params.qc.by);
            pack_kv_u64(pk, "upper_bound", req->params.qc.upper);
            pack_kv_u64(pk, "lower_bound", req->params.qc.lower);
            pack_kv_u64(pk, "max_count", req->params.qc.maxcnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_cmts_resp(const GetCmtsResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    CmtInfo **cinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_arr(pk, "result", cvector_size(resp->result.cinfos), {
            cvector_foreach(resp->result.cinfos, cinfo) {
                pack_map(pk, 8, {
                    pack_kv_u64(pk, "channel_id", (*cinfo)->chan_id);
                    pack_kv_u64(pk, "post_id", (*cinfo)->post_id);
                    pack_kv_u64(pk, "id", (*cinfo)->cmt_id);
                    pack_kv_u64(pk, "comment_id", (*cinfo)->reply_to_cmt);
                    pack_kv_str(pk, "user_name", (*cinfo)->user.name);
                    pack_kv_bin(pk, "content", (*cinfo)->content, (*cinfo)->len);
                    pack_kv_u64(pk, "likes", (*cinfo)->likes);
                    pack_kv_u64(pk, "created_at", (*cinfo)->created_at);
                });
            }
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_stats_req(const GetStatsReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "get_statistics");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 1, {
            pack_kv_str(pk, "access_token", req->params.tk);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_stats_resp(const GetStatsResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_str(pk, "did", resp->result.did);
            pack_kv_u64(pk, "connecting_clients", resp->result.conn_cs);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_sub_chan_req(const SubChanReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "subscribe_channel");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 2, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "id", req->params.id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_sub_chan_resp(const SubChanResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_unsub_chan_req(const UnsubChanReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "unsubscribe_channel");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 2, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "id", req->params.id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_unsub_chan_resp(const UnsubChanResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_enbl_notif_req(const EnblNotifReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_str(pk, "method", "enable_notification");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 1, {
            pack_kv_str(pk, "access_token", req->params.tk);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_enbl_notif_resp(const EnblNotifResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "jsonrpc", "2.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}
