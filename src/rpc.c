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

#include <assert.h>

#include <msgpack.h>
#include <crystal.h>
#include <ela_did.h>

#include "rpc.h"
#include "err.h"

#define TAG_RPC "[Feedsd.Rpc ]: "

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
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *nonce;
    const msgpack_object *owner_did;
    DeclOwnerReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            nonce     = map_val_str("nonce");
            owner_did = map_val_str("owner_did");
        });
    });

    if (!nonce || !nonce->str_sz || !owner_did || !owner_did->str_sz) {
        vlogE(TAG_RPC "Invalid declare_owner request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(DeclOwnerReq) + str_reserve_spc(method) +
                    str_reserve_spc(nonce) + str_reserve_spc(owner_did), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id           = tsx_id->u64_val;
    tmp->params.nonce     = strncpy(buf, nonce->str_val, nonce->str_sz);
    buf = (char*)buf + str_reserve_spc(nonce);
    tmp->params.owner_did = strncpy(buf, owner_did->str_val, owner_did->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_imp_did_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *mnemo;
    const msgpack_object *passphrase;
    const msgpack_object *idx;
    ImpDIDReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
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
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id = tsx_id->u64_val;

    if (mnemo)
        tmp->params.mnemo = strncpy(buf, mnemo->str_val, mnemo->str_sz);

    if (passphrase) {
        buf = (char*)buf + str_reserve_spc(mnemo);
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
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *vc;
    IssVCReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            vc = map_val_str("credential");
        });
    });

    if (!vc || !vc->str_sz) {
        vlogE(TAG_RPC "Invalid issue_credential request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(IssVCReq) + str_reserve_spc(method) +
                    str_reserve_spc(vc), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.vc = strncpy(buf, vc->str_val, vc->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_update_vc_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *vc;
    const msgpack_object *tk;
    UpdateVCReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
            vc = map_val_str("credential");
        });
    });

    if (!tk || !tk->str_sz || !vc || !vc->str_sz) {
        vlogE(TAG_RPC "Invalid update_credential request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(UpdateVCReq) + str_reserve_spc(method) +
                    str_reserve_spc(tk) + str_reserve_spc(vc), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);
    buf = (char*)buf + str_reserve_spc(tk);
    tmp->params.vc = strncpy(buf, vc->str_val, vc->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_signin_req_chal_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *iss;
    const msgpack_object *vc_req;
    SigninReqChalReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            iss    = map_val_str("iss");
            vc_req = map_val_bool("credential_required");
        });
    });

    if (!iss || !iss->str_sz || !vc_req) {
        vlogE(TAG_RPC "Invalid signin_request_challenge request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(SigninReqChalReq) + str_reserve_spc(method) +
                    str_reserve_spc(iss), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method        = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id        = tsx_id->u64_val;
    tmp->params.iss    = strncpy(buf, iss->str_val, iss->str_sz);
    tmp->params.vc_req = vc_req->bool_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_signin_conf_chal_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *jws;
    const msgpack_object *vc;
    SigninConfChalReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
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
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id     = tsx_id->u64_val;
    tmp->params.jws = strncpy(buf, jws->str_val, jws->str_sz);

    if (vc) {
        buf = (char*)buf + str_reserve_spc(jws);
        tmp->params.vc = strncpy(buf, vc->str_val, vc->str_sz);
    }

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_create_chan_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *name;
    const msgpack_object *intro;
    const msgpack_object *avatar;
    CreateChanReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk     = map_val_str("access_token");
            name   = map_val_str("name");
            intro  = map_val_str("introduction");
            avatar = map_val_bin("avatar");
        });
    });

    if (!tk || !tk->str_sz || !name || !name->str_sz ||
        !intro || !intro->str_sz || !avatar || !avatar->bin_sz) {
        vlogE(TAG_RPC "Invalid create_channel request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(CreateChanReq) + str_reserve_spc(method) +
                    str_reserve_spc(tk) + str_reserve_spc(name) +
                    str_reserve_spc(intro), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method        = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id        = tsx_id->u64_val;
    tmp->params.tk     = strncpy(buf, tk->str_val, tk->str_sz);
    buf = (char*)buf + str_reserve_spc(tk);
    tmp->params.name   = strncpy(buf, name->str_val, name->str_sz);
    buf = (char*)buf + str_reserve_spc(name);
    tmp->params.intro  = strncpy(buf, intro->str_val, intro->str_sz);
    tmp->params.avatar = (void *)avatar->bin_val;
    tmp->params.sz     = avatar->bin_sz;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_upd_chan_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *name;
    const msgpack_object *intro;
    const msgpack_object *avatar;
    UpdChanReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("id");
            name    = map_val_str("name");
            intro   = map_val_str("introduction");
            avatar  = map_val_bin("avatar");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !name || !name->str_sz || !intro || !intro->str_sz || !avatar || !avatar->bin_sz) {
        vlogE(TAG_RPC "Invalid update_feedinfo request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(UpdChanReq) + str_reserve_spc(method) +
                    str_reserve_spc(tk) + str_reserve_spc(name) +
                    str_reserve_spc(intro), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    buf = (char*)buf + str_reserve_spc(tk);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.name    = strncpy(buf, name->str_val, name->str_sz);
    buf = (char*)buf + str_reserve_spc(name);
    tmp->params.intro   = strncpy(buf, intro->str_val, intro->str_sz);
    tmp->params.avatar  = (void *)avatar->bin_val;
    tmp->params.sz      = avatar->bin_sz;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_pub_post_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *content;
    PubPostReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid publish_post request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(PubPostReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.content = (void *)content->bin_val;
    tmp->params.sz      = content->bin_sz;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_declare_post_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *content;
    const msgpack_object *with_notify;
    DeclarePostReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            content = map_val_bin("content");
            with_notify = map_val_bool("with_notify");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !content || !content->bin_sz) {
        vlogE(TAG_RPC "Invalid declare_post request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(DeclarePostReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.content = (void *)content->bin_val;
    tmp->params.sz      = content->bin_sz;
    tmp->params.with_notify = with_notify->bool_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_notify_post_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    NotifyPostReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
        });
    });

    if (!tk || !tk->str_sz
    || !chan_id || !chan_id_is_valid(chan_id->u64_val)
    || !post_id || !post_id_is_valid(post_id->u64_val)) {
        vlogE(TAG_RPC "Invalid notify_post request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NotifyPostReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_edit_post_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *content;
    EditPostReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("id");
            content = map_val_bin("content");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) || !content || !content->bin_sz) {
        vlogE(TAG_RPC "Invalid edit_post request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(EditPostReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.content = (void *)content->bin_val;
    tmp->params.sz      = content->bin_sz;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_del_post_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    DelPostReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val)) {
        vlogE(TAG_RPC "Invalid delete_post request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(DelPostReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_post_cmt_req(const msgpack_object *req, Req **req_unmarshal)
{
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid post_comment request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostCmtReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
int unmarshal_edit_cmt_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *id;
    const msgpack_object *cmt_id;
    const msgpack_object *content;
    EditCmtReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
            id      = map_val_u64("id");
            cmt_id  = map_val_u64("comment_id");
            content = map_val_bin("content");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) ||
        !id || !cmt_id_is_valid(id->u64_val) ||
        !cmt_id || !content || !content->bin_sz) {
        vlogE(TAG_RPC "Invalid edit_comment request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(EditCmtReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.id      = id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;
    tmp->params.content = (void *)content->bin_val;
    tmp->params.sz      = content->bin_sz;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_del_cmt_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *id;
    DelCmtReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
            id      = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) || !id || !cmt_id_is_valid(id->u64_val)) {
        vlogE(TAG_RPC "Invalid delete_comment request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(DelCmtReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.id      = id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_block_cmt_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *cmt_id;
    BlockCmtReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
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
        !post_id || !post_id_is_valid(post_id->u64_val) || !cmt_id || !cmt_id_is_valid(cmt_id->u64_val)) {
        vlogE(TAG_RPC "Invalid block_comment request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(BlockCmtReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf +  str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_unblock_cmt_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *cmt_id;
    UnblockCmtReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
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
        !post_id || !post_id_is_valid(post_id->u64_val) || !cmt_id || !cmt_id_is_valid(cmt_id->u64_val)) {
        vlogE(TAG_RPC "Invalid block_comment request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(UnblockCmtReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_post_like_req(const msgpack_object *req, Req **req_unmarshal)
{
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid post_like request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostLikeReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_post_unlike_req(const msgpack_object *req, Req **req_unmarshal)
{
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid post_like request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostUnlikeReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_my_channels request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetMyChansReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_my_channels_metadata request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetMyChansMetaReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_channels request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChansReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *id;
    GetChanDtlReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
            id = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !id || !chan_id_is_valid(id->u64_val)) {
        vlogE(TAG_RPC "Invalid get_channel_detail request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChanDtlReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.id = id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_sub_chans_req(const msgpack_object *req, Req **req_unmarshal)
{
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_subscribed_channels request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetSubChansReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_posts request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetPostsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
int unmarshal_get_posts_lac_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetPostsLACReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_posts_likes_and_comments request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetPostsLACReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
int unmarshal_get_liked_posts_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetLikedPostsReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            by      = map_val_u64("by");
            upper   = map_val_u64("upper_bound");
            lower   = map_val_u64("lower_bound");
            maxcnt  = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz || !by || !qry_fld_is_valid(by->u64_val) ||
        !upper || !lower || !maxcnt) {
        vlogE(TAG_RPC "Invalid get_liked_posts request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetLikedPostsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
int unmarshal_get_cmts_req(const msgpack_object *req, Req **req_unmarshal)
{
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_comments request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetCmtsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
int unmarshal_get_cmts_likes_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetCmtsLikesReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid get_comments_likes request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetCmtsLikesReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf +  str_reserve_spc(method);
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
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    GetStatsReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
        });
    });

    if (!tk || !tk->str_sz) {
        vlogE(TAG_RPC "Invalid get_statistics request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetStatsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_sub_chan_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *id;
    SubChanReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
            id = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !id || !chan_id_is_valid(id->u64_val)) {
        vlogE(TAG_RPC "Invalid subscribe_channel request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(SubChanReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.id = id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_unsub_chan_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *id;
    UnsubChanReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
            id = map_val_u64("id");
        });
    });

    if (!tk || !tk->str_sz || !id || !chan_id_is_valid(id->u64_val)) {
        vlogE(TAG_RPC "Invalid unsubscribe_channel request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(UnsubChanReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);
    tmp->params.id = id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_enbl_notif_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    EnblNotifReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk = map_val_str("access_token");
        });
    });

    if (!tk || !tk->str_sz) {
        vlogE(TAG_RPC "Invalid enable_notification request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(EnblNotifReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method    = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id    = tsx_id->u64_val;
    tmp->params.tk = strncpy(buf, tk->str_val, tk->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_set_binary_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *key;
    const msgpack_object *algo;
    const msgpack_object *checksum;
    const msgpack_object *content;
    SetBinaryReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk       = map_val_str("access_token");
            key      = map_val_str("key");
            algo     = map_val_str("algo");
            checksum = map_val_str("checksum");
            content  = map_val_bin("content");
        });
    });

    if (!tk || !tk->str_sz || !key || !key->str_sz) {
        vlogE(TAG_RPC "Invalid set_binary request.");
        return -1;
    }

    int str_size = str_reserve_spc(method)
                 + str_reserve_spc(tk) + str_reserve_spc(key)
                 + str_reserve_spc(algo) + str_reserve_spc(checksum);
    tmp = rc_zalloc(sizeof(*tmp) + str_size, NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method          = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id          = tsx_id->u64_val;
    tmp->params.tk       = strncpy(buf, tk->str_val, tk->str_sz);
    buf = (char*)buf + str_reserve_spc(tk);
    tmp->params.key      = strncpy(buf, key->str_val, key->str_sz);
    buf = (char*)buf + str_reserve_spc(key);
    tmp->params.algo     = strncpy(buf, algo->str_val, algo->str_sz);
    buf = (char*)buf + str_reserve_spc(algo);
    tmp->params.checksum = strncpy(buf, checksum->str_val, checksum->str_sz);
    buf = (char*)buf + str_reserve_spc(checksum);
    if(content) {
        tmp->params.content = (void *)content->bin_val;
        tmp->params.content_sz      = content->bin_sz;
    }

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_binary_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *key;
    GetBinaryReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk       = map_val_str("access_token");
            key      = map_val_str("key");
        });
    });

    if (!tk || !tk->str_sz || !key || !key->str_sz) {
        vlogE(TAG_RPC "Invalid get_binary request.");
        return -1;
    }

    int str_size = str_reserve_spc(method)
                 + str_reserve_spc(tk) + str_reserve_spc(key);
    tmp = rc_zalloc(sizeof(*tmp) + str_size, NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method          = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id          = tsx_id->u64_val;
    tmp->params.tk       = strncpy(buf, tk->str_val, tk->str_sz);
    buf = (char*)buf + str_reserve_spc(tk);
    tmp->params.key      = strncpy(buf, key->str_val, key->str_sz);
    buf = (char*)buf + str_reserve_spc(key);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_srv_ver_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    GetSrvVerReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            map_val_str("access_token");
        });
    });

    int str_size = str_reserve_spc(method);
    tmp = rc_zalloc(sizeof(*tmp) + str_size, NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method          = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id          = tsx_id->u64_val;

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_report_illegal_cmt_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *cmt_id;
    const msgpack_object *reasons;
    ReportIllegalCmtReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            chan_id = map_val_u64("channel_id");
            post_id = map_val_u64("post_id");
            cmt_id  = map_val_u64("comment_id");
            reasons = map_val_str("reasons");
        });
    });

    if (!tk || !tk->str_sz || !chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) || !cmt_id ||
        !reasons || !reasons->str_sz) {
        vlogE(TAG_RPC "Invalid report_illegal_cmt request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(ReportIllegalCmtReq) + str_reserve_spc(method) + str_reserve_spc(tk) + str_reserve_spc(reasons), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method         = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->tsx_id         = tsx_id->u64_val;
    tmp->params.tk      = strncpy(buf, tk->str_val, tk->str_sz);
    buf = (char*)buf + str_reserve_spc(tk);
    tmp->params.chan_id = chan_id->u64_val;
    tmp->params.post_id = post_id->u64_val;
    tmp->params.cmt_id  = cmt_id->u64_val;
    tmp->params.reasons      = strncpy(buf, reasons->str_val, reasons->str_sz);

    *req_unmarshal = (Req *)tmp;
    return 0;
}

static
int unmarshal_get_reported_cmts_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    const msgpack_object *tk;
    const msgpack_object *by;
    const msgpack_object *upper;
    const msgpack_object *lower;
    const msgpack_object *maxcnt;
    GetReportedCmtsReq *tmp;
    void *buf;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("params"), {
            tk      = map_val_str("access_token");
            by      = map_val_u64("by");
            upper   = map_val_u64("upper_bound");
            lower   = map_val_u64("lower_bound");
            maxcnt  = map_val_u64("max_count");
        });
    });

    if (!tk || !tk->str_sz ||
        !by || !qry_fld_is_valid(by->u64_val) || !upper || !lower || !maxcnt) {
        vlogE(TAG_RPC "Invalid get_reported_comments request.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetReportedCmtsReq) + str_reserve_spc(method) + str_reserve_spc(tk), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->method           = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
int unmarshal_unknown_req(const msgpack_object *req, Req **req_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    void *buf;
    Req *tmp;

    assert(req->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(req, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
    });

    tmp = rc_zalloc(sizeof(Req) + str_reserve_spc(method), NULL);
    if(!tmp)
     return -1;

    buf = tmp + 1;
    tmp->method = strncpy(buf, method->str_val, method->str_sz);
    tmp->tsx_id = tsx_id->u64_val;

    *req_unmarshal = tmp;
    return 0;
}

typedef int ReqHdlr(const msgpack_object *req_map, Req **req_unmarshal);
struct RepParser {
    char *method;
    ReqHdlr *parser;
};
static struct RepParser req_parsers_1_0[] = {
    {"declare_owner"               , unmarshal_decl_owner_req         },
    {"import_did"                  , unmarshal_imp_did_req            },
    {"issue_credential"            , unmarshal_iss_vc_req             },
    {"update_credential"           , unmarshal_update_vc_req          },
    {"signin_request_challenge"    , unmarshal_signin_req_chal_req    },
    {"signin_confirm_challenge"    , unmarshal_signin_conf_chal_req   },
    {"create_channel"              , unmarshal_create_chan_req        },
    {"update_feedinfo"             , unmarshal_upd_chan_req           },
    {"publish_post"                , unmarshal_pub_post_req           },
    {"declare_post"                , unmarshal_declare_post_req       },
    {"notify_post"                 , unmarshal_notify_post_req        },
    {"edit_post"                   , unmarshal_edit_post_req          },
    {"delete_post"                 , unmarshal_del_post_req           },
    {"post_comment"                , unmarshal_post_cmt_req           },
    {"edit_comment"                , unmarshal_edit_cmt_req           },
    {"delete_comment"              , unmarshal_del_cmt_req            },
    {"block_comment"               , unmarshal_block_cmt_req          },
    {"unblock_comment"             , unmarshal_unblock_cmt_req        },
    {"post_like"                   , unmarshal_post_like_req          },
    {"post_unlike"                 , unmarshal_post_unlike_req        },
    {"get_my_channels"             , unmarshal_get_my_chans_req       },
    {"get_my_channels_metadata"    , unmarshal_get_my_chans_meta_req  },
    {"get_channels"                , unmarshal_get_chans_req          },
    {"get_channel_detail"          , unmarshal_get_chan_dtl_req       },
    {"get_subscribed_channels"     , unmarshal_get_sub_chans_req      },
    {"get_posts"                   , unmarshal_get_posts_req          },
    {"get_posts_likes_and_comments", unmarshal_get_posts_lac_req      },
    {"get_liked_posts"             , unmarshal_get_liked_posts_req    },
    {"get_comments"                , unmarshal_get_cmts_req           },
    {"get_comments_likes"          , unmarshal_get_cmts_likes_req     },
    {"get_statistics"              , unmarshal_get_stats_req          },
    {"subscribe_channel"           , unmarshal_sub_chan_req           },
    {"unsubscribe_channel"         , unmarshal_unsub_chan_req         },
    {"enable_notification"         , unmarshal_enbl_notif_req         },
    {"set_binary"                  , unmarshal_set_binary_req         },
    {"get_binary"                  , unmarshal_get_binary_req         },
    {"get_service_version"         , unmarshal_get_srv_ver_req        },
    {"report_illegal_comment"      , unmarshal_report_illegal_cmt_req },
    {"get_reported_comments"       , unmarshal_get_reported_cmts_req  },
};

int rpc_unmarshal_req(const void *rpc, size_t len, Req **req)
{
    const msgpack_object *version;
    const msgpack_object *method;
    const msgpack_object *tsx_id;
    char method_str[1024];
    msgpack_object obj;
    struct RepParser *req_parsers;
    int req_parsers_size;
    int rc;
    int i;

    msgpack_unpacked_init(&msgpack);
    if (msgpack_unpack_next(&msgpack, rpc, len, NULL) != MSGPACK_UNPACK_SUCCESS) {
        vlogE(TAG_RPC "Decoding msgpack failed.");
        return -1;
    }

    obj = msgpack.data;
    if (obj.type != MSGPACK_OBJECT_MAP) {
        vlogE(TAG_RPC "Not a msgpack map.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    map_iter_kvs(&obj, {
        version = map_val_str("version");
        method  = map_val_str("method");
        tsx_id  = map_val_u64("id");
    });

    if (!version || !version->str_sz ||
        !method || !method->str_sz ||
        !tsx_id) {
        vlogE(TAG_RPC "No version/method/id field.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    memset(method_str, 0, sizeof(method_str));
    strncpy(method_str, method->str_val, method->str_sz);

    if(memcmp(version->str_val, "1.0", version->str_sz) == 0) {
        req_parsers = req_parsers_1_0;
        req_parsers_size = sizeof(req_parsers_1_0) / sizeof(*req_parsers_1_0);
    } else {
        vlogE(TAG_RPC "Unsupported version field.");
        rc = unmarshal_unknown_req(&obj, req);
        msgpack_unpacked_destroy(&msgpack);
        return -3;
    }

    for (i = 0; i < req_parsers_size; ++i) {
        if (!strcmp(method_str, req_parsers[i].method)) {
            int rc = req_parsers[i].parser(&obj, req);
            msgpack_unpacked_destroy(&msgpack);
            return rc;
        }
    }

    vlogE(TAG_RPC "Not a valid method.");
    rc = unmarshal_unknown_req(&obj, req);
    msgpack_unpacked_destroy(&msgpack);
    return rc < 0 ? -1 : -2;
}

typedef struct {
    NewPostNotif notif;
    PostInfo pi;
} NewPostNotifWithPInfo;

static
int unmarshal_new_post_notif(const msgpack_object *notif, Notif **notif_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *content;
    const msgpack_object *created_at;
    NewPostNotifWithPInfo *tmp;
    void *buf;

    assert(notif->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(notif, {
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid new_post notification.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NewPostNotifWithPInfo) + str_reserve_spc(method) +
                    content->bin_sz, NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->notif.method                   = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
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
        (void)map_val_str("version");
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
        vlogE(TAG_RPC "Invalid new_comment notification.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NewCmtNotifWithCInfo) + str_reserve_spc(method) +
                    str_reserve_spc(user_name) + content->bin_sz, NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->notif.method                     = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->notif.params.cinfo               = &tmp->ci;
    tmp->notif.params.cinfo->chan_id      = chan_id->u64_val;
    tmp->notif.params.cinfo->post_id      = post_id->u64_val;
    tmp->notif.params.cinfo->cmt_id       = id->u64_val;
    tmp->notif.params.cinfo->reply_to_cmt = cmt_id->u64_val;
    tmp->notif.params.cinfo->user.name    = strncpy(buf, user_name->str_val, user_name->str_sz);
    buf = (char*)buf + str_reserve_spc(user_name);
    tmp->notif.params.cinfo->content      = memcpy(buf, content->bin_val, content->bin_sz);
    tmp->notif.params.cinfo->len          = content->bin_sz;
    tmp->notif.params.cinfo->created_at   = created_at->u64_val;

    *notif_unmarshal = (Notif *)&tmp->notif;
    return 0;
}

typedef struct {
    NewLikeNotif notif;
    LikeInfo li;
} NewLikeNotifWithLInfo;

static
int unmarshal_new_like_notif(const msgpack_object *notif, Notif **notif_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *chan_id;
    const msgpack_object *post_id;
    const msgpack_object *cmt_id;
    const msgpack_object *user_name;
    const msgpack_object *user_did;
    const msgpack_object *cnt;
    NewLikeNotifWithLInfo *tmp;
    void *buf;

    assert(notif->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(notif, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        map_iter_kvs(map_val_map("params"), {
            chan_id   = map_val_u64("channel_id");
            post_id   = map_val_u64("post_id");
            cmt_id    = map_val_u64("comment_id");
            user_name = map_val_str("user_name");
            user_did  = map_val_str("user_did");
            cnt       = map_val_u64("total_count");
        });
    });

    if (!chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !post_id || !post_id_is_valid(post_id->u64_val) ||
        !user_name || !user_name->str_sz || !user_did || !user_did->str_sz || !cnt) {
        vlogE(TAG_RPC "Invalid new_like notification.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NewLikeNotifWithLInfo) + str_reserve_spc(method) +
                    str_reserve_spc(user_name) + str_reserve_spc(user_did), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->notif.method               = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->notif.params.li            = &tmp->li;
    tmp->notif.params.li->chan_id   = chan_id->u64_val;
    tmp->notif.params.li->post_id   = post_id->u64_val;
    tmp->notif.params.li->cmt_id    = cmt_id->u64_val;
    tmp->notif.params.li->user.name = strncpy(buf, user_name->str_val, user_name->str_sz);
    buf = (char*)buf + str_reserve_spc(user_name);
    tmp->notif.params.li->user.did  = strncpy(buf, user_did->str_val, user_did->str_sz);
    tmp->notif.params.li->total_cnt = cnt->u64_val;

    *notif_unmarshal = (Notif *)&tmp->notif;
    return 0;
}

typedef struct {
    NewSubNotif notif;
    UserInfo ui;
} NewSubNotifWithUInfo;

static
int unmarshal_new_sub_notif(const msgpack_object *notif, Notif **notif_unmarshal)
{
    const msgpack_object *method;
    const msgpack_object *chan_id;
    const msgpack_object *user_name;
    const msgpack_object *user_did;
    NewSubNotifWithUInfo *tmp;
    void *buf;

    assert(notif->type == MSGPACK_OBJECT_MAP);

    map_iter_kvs(notif, {
        (void)map_val_str("version");
        method  = map_val_str("method");
        map_iter_kvs(map_val_map("params"), {
            chan_id   = map_val_u64("channel_id");
            user_name = map_val_str("user_name");
            user_did  = map_val_str("user_did");
        });
    });

    if (!chan_id || !chan_id_is_valid(chan_id->u64_val) ||
        !user_name || !user_name->str_sz || !user_did || !user_did->str_sz) {
        vlogE(TAG_RPC "Invalid new_subscription notification.");
        return -1;
    }

    tmp = rc_zalloc(sizeof(NewSubNotifWithUInfo) + str_reserve_spc(method) +
                    str_reserve_spc(user_name) + str_reserve_spc(user_did), NULL);
    if (!tmp)
        return -1;

    buf = tmp + 1;
    tmp->notif.method             = strncpy(buf, method->str_val, method->str_sz);
    buf = (char*)buf + str_reserve_spc(method);
    tmp->notif.params.uinfo       = &tmp->ui;
    tmp->notif.params.chan_id     = chan_id->u64_val;
    tmp->notif.params.uinfo->name = strncpy(buf, user_name->str_val, user_name->str_sz);
    buf = (char*)buf + str_reserve_spc(user_name);
    tmp->notif.params.uinfo->did  = strncpy(buf, user_did->str_val, user_did->str_sz);

    *notif_unmarshal = (Notif *)&tmp->notif;
    return 0;
}

typedef int NotifHdlr(const msgpack_object *notif_map, Notif **notif_unmarshal);
static struct {
    char *method;
    NotifHdlr *parser;
} notif_parsers[] = {
    {"new_post"        , unmarshal_new_post_notif },
    {"new_comment"     , unmarshal_new_cmt_notif  },
    {"new_like"        , unmarshal_new_like_notif },
    {"new_subscription", unmarshal_new_sub_notif  }
};

int rpc_unmarshal_notif_or_resp_id(const void *rpc, size_t len, Notif **notif, uint64_t *resp_id)
{
    const msgpack_object *version;
    const msgpack_object *tsx_id;
    const msgpack_object *method;
    char method_str[1024];
    msgpack_object obj;
    int i;

    msgpack_unpacked_init(&msgpack);
    if (msgpack_unpack_next(&msgpack, rpc, len, NULL) != MSGPACK_UNPACK_SUCCESS) {
        vlogE(TAG_RPC "Decoding msgpack failed.");
        return -1;
    }

    obj = msgpack.data;
    if (obj.type != MSGPACK_OBJECT_MAP) {
        vlogE(TAG_RPC "Not a msgpack map.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    map_iter_kvs(&obj, {
        version = map_val_str("version");
        tsx_id  = map_val_u64("id");
        method  = map_val_str("method");
    });

    if (!version || version->str_sz != strlen("1.0") ||
        memcmp(version->str_val, "1.0", version->str_sz) ||
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

    vlogE(TAG_RPC "Not a valid method.");
    msgpack_unpacked_destroy(&msgpack);
    return -1;
}

static
int rpc_unmarshal_err_resp(ErrResp **err)
{
    const msgpack_object *tsx_id;
    const msgpack_object *code;
    ErrResp *tmp;

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
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
        (void)map_val_str("version");
        tsx_id = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            phase       = map_val_str("phase");
            did         = map_val_str("did");
            tsx_payload = map_val_str("transaction_payload");
        });
    });

    if (!phase || !((phase->str_sz == strlen("did_imported") &&
                     !memcmp(phase->str_val, "did_imported", phase->str_sz)) ?
                    (did && tsx_payload && tsx_payload->str_sz) :
                    (!did && !tsx_payload))) {
        vlogE(TAG_RPC "Invalid declare_owner response.");
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
        buf = (char*)buf + str_reserve_spc(phase);
        tmp->result.did         = strncpy(buf, did->str_val, did->str_sz);
        buf = (char*)buf + str_reserve_spc(did);
        tmp->result.tsx_payload = strncpy(buf, tsx_payload->str_val, tsx_payload->str_sz);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_imp_did_resp(ImpDIDResp **resp, ErrResp **err)
{
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
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            did         = map_val_str("did");
            tsx_payload = map_val_str("transaction_payload");
        });
    });

    if (!did || !tsx_payload || !tsx_payload->str_sz) {
        vlogE(TAG_RPC "Invalid import_did response.");
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
    buf = (char*)buf + str_reserve_spc(did);
    tmp->result.tsx_payload = strncpy(buf, tsx_payload->str_val, tsx_payload->str_sz);

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_iss_vc_resp(IssVCResp **resp, ErrResp **err)
{
    const msgpack_object *tsx_id;
    IssVCResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
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

int rpc_unmarshal_update_vc_resp(UpdateVCResp **resp, ErrResp **err)
{
    const msgpack_object *tsx_id;
    UpdateVCResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
    });

    tmp = rc_zalloc(sizeof(UpdateVCResp), NULL);
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
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            vc_req = map_val_bool("credential_required");
            jws    = map_val_str("jws");
            vc     = map_val_str("credential");
        });
    });

    if (!vc_req || !jws || !jws->str_sz || (vc && !vc->str_sz)) {
        vlogE(TAG_RPC "Invalid sigin_request_challenge response.");
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
        buf = (char*)buf + str_reserve_spc(jws);
        tmp->result.vc = strncpy(buf, vc->str_val, vc->str_sz);
    }

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_signin_conf_chal_resp(SigninConfChalResp **resp, ErrResp **err)
{
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
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            tk  = map_val_str("access_token");
            exp = map_val_u64("exp");
        });
    });

    if (!tk || !tk->str_sz || !exp) {
        vlogE(TAG_RPC "Invalid signin_confirm_challenge response.");
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
    const msgpack_object *tsx_id;
    const msgpack_object *chan_id;
    CreateChanResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            chan_id = map_val_u64("id");
        });
    });

    if (!chan_id) {
        vlogE(TAG_RPC "Invalid create_channel response.");
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
    const msgpack_object *tsx_id;
    const msgpack_object *post_id;
    PubPostResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            post_id = map_val_u64("id");
        });
    });

    if (!post_id) {
        vlogE(TAG_RPC "Invalid publish_post response.");
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

int rpc_unmarshal_declare_post_resp(DeclarePostResp **resp, ErrResp **err)
{
    const msgpack_object *tsx_id;
    const msgpack_object *post_id;
    DeclarePostResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            post_id = map_val_u64("id");
        });
    });

    if (!post_id) {
        vlogE(TAG_RPC "Invalid declare_post response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(DeclarePostResp), NULL);
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
    const msgpack_object *tsx_id;
    const msgpack_object *cmt_id;
    PostCmtResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            cmt_id = map_val_u64("id");
        });
    });

    if (!cmt_id) {
        vlogE(TAG_RPC "Invalid post_comment response.");
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
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    PostLikeResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE(TAG_RPC "Invalid post_like response.");
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

int rpc_unmarshal_post_unlike_resp(PostUnlikeResp **resp, ErrResp **err)
{
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    PostUnlikeResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE(TAG_RPC "Invalid post_unlike response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(PostUnlikeResp), NULL);
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
    const msgpack_object *tsx_id;
    const msgpack_object *is_last;
    const msgpack_object *chans;
    GetMyChansResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            is_last = map_val_bool("is_last");
            chans   = map_val_arr("channels");
        });
    });

    if (!is_last || !chans) {
        vlogE(TAG_RPC "Invalid get_my_channels response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetMyChansResp), get_my_chans_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id         = tsx_id->u64_val;
    tmp->result.is_last = is_last->bool_val;

    for (i = 0; i < chans->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *name;
        const msgpack_object *intro;
        const msgpack_object *subs;
        const msgpack_object *avatar;
        ChanInfo *ci;
        void *buf;

        map_iter_kvs(arr_val_map(chans, i), {
            chan_id = map_val_u64("id");
            name    = map_val_str("name");
            intro   = map_val_str("introduction");
            subs    = map_val_u64("subscribers");
            avatar  = map_val_bin("avatar");
        });

        if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz ||
            !subs || !avatar || !avatar->bin_sz) {
            vlogE(TAG_RPC "Invalid get_my_channels response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(ChanInfo) + str_reserve_spc(name) +
                       str_reserve_spc(intro) + avatar->bin_sz, NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = ci + 1;
        ci->chan_id = chan_id->u64_val;
        ci->name    = strncpy(buf, name->str_val, name->str_sz);
        buf = (char*)buf + str_reserve_spc(name);
        ci->intro   = strncpy(buf, intro->str_val, intro->str_sz);
        buf = (char*)buf + str_reserve_spc(intro);
        ci->subs    = subs->u64_val;
        ci->avatar  = memcpy(buf, avatar->bin_val, avatar->bin_sz);
        ci->len     = avatar->bin_sz;

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
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    GetMyChansMetaResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        result  = map_val_arr("result");
    });

    if (!result) {
        vlogE(TAG_RPC "Invalid get_my_channels_metadata response: invalid result.");
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
            vlogE(TAG_RPC "Invalid get_my_channels_metadata response: invalid result element.");
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
    const msgpack_object *tsx_id;
    const msgpack_object *is_last;
    const msgpack_object *chans;
    GetChansResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            is_last = map_val_bool("is_last");
            chans   = map_val_arr("channels");
        });
    });

    if (!is_last || !chans) {
        vlogE(TAG_RPC "Invalid get_channels response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChansResp), get_chans_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id         = tsx_id->u64_val;
    tmp->result.is_last = is_last->bool_val;

    for (i = 0; i < chans->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *name;
        const msgpack_object *intro;
        const msgpack_object *owner_name;
        const msgpack_object *owner_did;
        const msgpack_object *subs;
        const msgpack_object *upd_at;
        const msgpack_object *avatar;
        ChanInfoWithOwner *ci;
        void *buf;

        map_iter_kvs(arr_val_map(chans, i), {
            chan_id    = map_val_u64("id");
            name       = map_val_str("name");
            intro      = map_val_str("introduction");
            owner_name = map_val_str("owner_name");
            owner_did  = map_val_str("owner_did");
            subs       = map_val_u64("subscribers");
            upd_at     = map_val_u64("last_update");
            avatar     = map_val_bin("avatar");
        });

        if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz ||
            !owner_name || !owner_name->str_sz || !owner_did || !owner_did->str_sz ||
            !subs || !upd_at || !avatar || !avatar->bin_sz) {
            vlogE(TAG_RPC "Invalid get_channels response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(ChanInfoWithOwner) + str_reserve_spc(name) +
                       str_reserve_spc(intro) + str_reserve_spc(owner_name) +
                       str_reserve_spc(owner_did) + avatar->bin_sz, NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = ci + 1;
        ci->ci.chan_id     = chan_id->u64_val;
        ci->ci.name        = strncpy(buf, name->str_val, name->str_sz);
        buf = (char*)buf + str_reserve_spc(name);
        ci->ci.intro       = strncpy(buf, intro->str_val, intro->str_sz);
        buf = (char*)buf + str_reserve_spc(intro);
        ci->ci.owner       = &ci->owner;
        ci->ci.owner->name = strncpy(buf, owner_name->str_val, owner_name->str_sz);
        buf = (char*)buf + str_reserve_spc(owner_name);
        ci->ci.owner->did  = strncpy(buf, owner_did->str_val, owner_did->str_sz);
        buf = (char*)buf + str_reserve_spc(owner_did);
        ci->ci.subs        = subs->u64_val;
        ci->ci.upd_at      = upd_at->u64_val;
        ci->ci.avatar      = memcpy(buf, avatar->bin_val, avatar->bin_sz);
        ci->ci.len         = avatar->bin_sz;

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
    const msgpack_object *tsx_id;
    const msgpack_object *chan_id;
    const msgpack_object *name;
    const msgpack_object *intro;
    const msgpack_object *owner_name;
    const msgpack_object *owner_did;
    const msgpack_object *subs;
    const msgpack_object *upd_at;
    const msgpack_object *avatar;
    GetChanDtlRespWithChanInfo *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            chan_id    = map_val_u64("id");
            name       = map_val_str("name");
            intro      = map_val_str("introduction");
            owner_name = map_val_str("owner_name");
            owner_did  = map_val_str("owner_did");
            subs       = map_val_u64("subscribers");
            upd_at     = map_val_u64("last_update");
            avatar     = map_val_bin("avatar");
        });
    });

    if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz ||
        !owner_name || !owner_name->str_sz || !owner_did || !owner_did->str_sz ||
        !subs || !upd_at || !avatar || !avatar->bin_sz) {
        vlogE(TAG_RPC "Invalid get_channel_detail response.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetChanDtlRespWithChanInfo) + str_reserve_spc(name) +
                    str_reserve_spc(intro) + str_reserve_spc(owner_name) +
                    str_reserve_spc(owner_did) + avatar->bin_sz, NULL);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    buf = tmp + 1;
    tmp->resp.tsx_id                    = tsx_id->u64_val;
    tmp->resp.result.cinfo              = &tmp->ci.ci;
    tmp->resp.result.cinfo->chan_id     = chan_id->u64_val;
    tmp->resp.result.cinfo->name        = strncpy(buf, name->str_val, name->str_sz);
    buf = (char*)buf + str_reserve_spc(name);
    tmp->resp.result.cinfo->intro       = strncpy(buf, intro->str_val, intro->str_sz);
    buf = (char*)buf + str_reserve_spc(intro);
    tmp->resp.result.cinfo->owner       = &tmp->ci.owner;
    tmp->resp.result.cinfo->owner->name = strncpy(buf, owner_name->str_val, owner_name->str_sz);
    buf = (char*)buf + str_reserve_spc(owner_name);
    tmp->resp.result.cinfo->owner->did  = strncpy(buf, owner_did->str_val, owner_did->str_sz);
    buf = (char*)buf + str_reserve_spc(owner_did);
    tmp->resp.result.cinfo->subs        = subs->u64_val;
    tmp->resp.result.cinfo->upd_at      = upd_at->u64_val;
    tmp->resp.result.cinfo->avatar      = memcpy(buf, avatar->bin_val, avatar->bin_sz);
    tmp->resp.result.cinfo->len         = avatar->bin_sz;

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
    const msgpack_object *tsx_id;
    const msgpack_object *is_last;
    const msgpack_object *chans;
    GetSubChansResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            is_last = map_val_bool("is_last");
            chans   = map_val_arr("channels");
        });
    });

    if (!is_last || !chans) {
        vlogE(TAG_RPC "Invalid get_subscribed_channels response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetSubChansResp), get_sub_chans_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id         = tsx_id->u64_val;
    tmp->result.is_last = is_last->bool_val;

    for (i = 0; i < chans->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *name;
        const msgpack_object *intro;
        const msgpack_object *owner_name;
        const msgpack_object *owner_did;
        const msgpack_object *subs;
        const msgpack_object *upd_at;
        const msgpack_object *avatar;
        ChanInfoWithOwner *ci;
        void *buf;

        map_iter_kvs(arr_val_map(chans, i), {
            chan_id    = map_val_u64("id");
            name       = map_val_str("name");
            intro      = map_val_str("introduction");
            owner_name = map_val_str("owner_name");
            owner_did  = map_val_str("owner_did");
            subs       = map_val_u64("subscribers");
            upd_at     = map_val_u64("last_update");
            avatar     = map_val_bin("avatar");
        });

        if (!chan_id || !name || !name->str_sz || !intro || !intro->str_sz ||
            !owner_name || !owner_name->str_sz || !owner_did || !owner_did->str_sz ||
            !subs || !upd_at || !avatar || !avatar->bin_sz) {
            vlogE(TAG_RPC "Invalid get_subscribed_channels response: invalid result element.");
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        ci = rc_zalloc(sizeof(ChanInfoWithOwner) + str_reserve_spc(name) +
                       str_reserve_spc(intro) + str_reserve_spc(owner_name) +
                       str_reserve_spc(owner_did) + avatar->bin_sz, NULL);
        if (!ci) {
            msgpack_unpacked_destroy(&msgpack);
            deref(tmp);
            return -1;
        }

        buf = ci + 1;
        ci->ci.chan_id     = chan_id->u64_val;
        ci->ci.name        = strncpy(buf, name->str_val, name->str_sz);
        buf = (char*)buf + str_reserve_spc(name);
        ci->ci.intro       = strncpy(buf, intro->str_val, intro->str_sz);
        buf = (char*)buf + str_reserve_spc(intro);
        ci->ci.owner       = &ci->owner;
        ci->ci.owner->name = strncpy(buf, owner_name->str_val, owner_name->str_sz);
        buf = (char*)buf + str_reserve_spc(owner_name);
        ci->ci.owner->did  = strncpy(buf, owner_did->str_val, owner_did->str_sz);
        buf = (char*)buf + str_reserve_spc(owner_did);
        ci->ci.subs        = subs->u64_val;
        ci->ci.upd_at      = upd_at->u64_val;
        ci->ci.avatar      = memcpy(buf, avatar->bin_val, avatar->bin_sz);
        ci->ci.len         = avatar->bin_sz;

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
    const msgpack_object *tsx_id;
    const msgpack_object *is_last;
    const msgpack_object *posts;
    GetPostsResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            is_last = map_val_bool("is_last");
            posts   = map_val_arr("posts");
        });
    });

    if (!is_last || !posts) {
        vlogE(TAG_RPC "Invalid get_posts response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetPostsResp), get_posts_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id         = tsx_id->u64_val;
    tmp->result.is_last = is_last->bool_val;

    for (i = 0; i < posts->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *post_id;
        const msgpack_object *content;
        const msgpack_object *cmts;
        const msgpack_object *likes;
        const msgpack_object *created_at;
        PostInfo *pi;
        void *buf;

        map_iter_kvs(arr_val_map(posts, i), {
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
            vlogE(TAG_RPC "Invalid get_posts response: invalid result element.");
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
void get_liked_posts_resp_dtor(void *obj)
{
    GetLikedPostsResp *resp = obj;
    PostInfo **pi;

    cvector_foreach(resp->result.pinfos, pi)
        deref(*pi);
    cvector_free(resp->result.pinfos);
}

int rpc_unmarshal_get_liked_posts_resp(GetLikedPostsResp **resp, ErrResp **err)
{
    const msgpack_object *tsx_id;
    const msgpack_object *is_last;
    const msgpack_object *posts;
    GetLikedPostsResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            is_last = map_val_bool("is_last");
            posts   = map_val_arr("posts");
        });
    });

    if (!is_last || !posts) {
        vlogE(TAG_RPC "Invalid get_liked_posts response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetLikedPostsResp), get_liked_posts_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id         = tsx_id->u64_val;
    tmp->result.is_last = is_last->bool_val;

    for (i = 0; i < posts->arr_sz; ++i) {
        const msgpack_object *chan_id;
        const msgpack_object *post_id;
        const msgpack_object *content;
        const msgpack_object *cmts;
        const msgpack_object *likes;
        const msgpack_object *created_at;
        PostInfo *pi;
        void *buf;

        map_iter_kvs(arr_val_map(posts, i), {
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
            vlogE(TAG_RPC "Invalid get_liked_posts response: invalid result element.");
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
    const msgpack_object *tsx_id;
    const msgpack_object *is_last;
    const msgpack_object *cmts;
    GetCmtsResp *tmp;
    int i;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            is_last = map_val_bool("is_last");
            cmts    = map_val_arr("comments");
        });
    });

    if (!is_last || !cmts) {
        vlogE(TAG_RPC "Invalid get_comments response: invalid result.");
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp = rc_zalloc(sizeof(GetCmtsResp), get_cmts_resp_dtor);
    if (!tmp) {
        msgpack_unpacked_destroy(&msgpack);
        return -1;
    }

    tmp->tsx_id         = tsx_id->u64_val;
    tmp->result.is_last = is_last->bool_val;

    for (i = 0; i < cmts->arr_sz; ++i) {
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

        map_iter_kvs(arr_val_map(cmts, i), {
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
            vlogE(TAG_RPC "Invalid get_comments response: invalid result element.");
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
        buf = (char*)buf + str_reserve_spc(user_name);
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
    const msgpack_object *tsx_id;
    const msgpack_object *did;
    const msgpack_object *conn_cs;
    const msgpack_object *total_cs;
    GetStatsResp *tmp;
    void *buf;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        map_iter_kvs(map_val_map("result"), {
            did     = map_val_str("did");
            conn_cs = map_val_u64("connecting_clients");
            total_cs = map_val_u64("total_clients");
        });
    });

    if (!did || !did->str_sz || !conn_cs || !total_cs) {
        vlogE(TAG_RPC "Invalid get_statistics response.");
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
    tmp->result.total_cs = total_cs->u64_val;

    *resp = tmp;

    msgpack_unpacked_destroy(&msgpack);
    return 0;
}

int rpc_unmarshal_sub_chan_resp(SubChanResp **resp, ErrResp **err)
{
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    SubChanResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE(TAG_RPC "Invalid subscribe_channel response.");
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
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    UnsubChanResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE(TAG_RPC "Invalid unsubscribe_channel response.");
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
    const msgpack_object *tsx_id;
    const msgpack_object *result;
    EnblNotifResp *tmp;

    if (!rpc_unmarshal_err_resp(err)) {
        msgpack_unpacked_destroy(&msgpack);
        return 0;
    }

    map_iter_kvs(&msgpack.data, {
        (void)map_val_str("version");
        tsx_id  = map_val_u64("id");
        result  = map_val_nil("result");
    });

    if (!result) {
        vlogE(TAG_RPC "Invalid enable_notification response.");
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

static inline
void pack_kv_bin_withzero(msgpack_packer* pk, const char *k, const void *bin, size_t sz)
{
    assert(pk);
    assert(k);

    if(bin == NULL)
        sz = 0;

    pack_str(pk, k);
    msgpack_pack_bin(pk, sz);
    msgpack_pack_bin_body(pk, bin, sz);
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_update_vc_req(const UpdateVCReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "update_credential");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 1, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_str(pk, "credential", req->params.vc);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_update_vc_resp(const UpdateVCResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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

Marshalled *rpc_marshal_post_upd_notif(const PostUpdNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "post_update");
        pack_kv_map(pk, "params", 8, {
            pack_kv_u64(pk, "channel_id", notif->params.pinfo->chan_id);
            pack_kv_u64(pk, "id", notif->params.pinfo->post_id);
            pack_kv_u64(pk, "status", notif->params.pinfo->stat);
            notif->params.pinfo->stat == POST_DELETED ? pack_kv_nil(pk, "content") :
                                                        pack_kv_bin(pk, "content", notif->params.pinfo->content,
                                                                    notif->params.pinfo->len);
            pack_kv_u64(pk, "comments", notif->params.pinfo->cmts);
            pack_kv_u64(pk, "likes", notif->params.pinfo->likes);
            pack_kv_u64(pk, "created_at", notif->params.pinfo->created_at);
            pack_kv_u64(pk, "updated_at", notif->params.pinfo->upd_at);
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "new_comment");
        pack_kv_map(pk, "params", 8, {
            pack_kv_u64(pk, "channel_id", notif->params.cinfo->chan_id);
            pack_kv_u64(pk, "post_id", notif->params.cinfo->post_id);
            pack_kv_u64(pk, "id", notif->params.cinfo->cmt_id);
            pack_kv_u64(pk, "comment_id", notif->params.cinfo->reply_to_cmt);
            pack_kv_str(pk, "user_name", notif->params.cinfo->user.name);
            pack_kv_str(pk, "user_did", notif->params.cinfo->user.did);
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

Marshalled *rpc_marshal_cmt_upd_notif(const CmtUpdNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "comment_update");
        pack_kv_map(pk, "params", 11, {
            pack_kv_u64(pk, "channel_id", notif->params.cinfo->chan_id);
            pack_kv_u64(pk, "post_id", notif->params.cinfo->post_id);
            pack_kv_u64(pk, "id", notif->params.cinfo->cmt_id);
            pack_kv_u64(pk, "status", notif->params.cinfo->stat);
            pack_kv_u64(pk, "comment_id", notif->params.cinfo->reply_to_cmt);
            pack_kv_str(pk, "user_name", notif->params.cinfo->user.name);
            pack_kv_str(pk, "user_did", notif->params.cinfo->user.did);
            notif->params.cinfo->stat == CMT_AVAILABLE ? pack_kv_bin(pk, "content", notif->params.cinfo->content,
                                                                     notif->params.cinfo->len) :
                                                         pack_kv_nil(pk, "content");
            pack_kv_u64(pk, "likes", notif->params.cinfo->likes);
            pack_kv_u64(pk, "created_at", notif->params.cinfo->created_at);
            pack_kv_u64(pk, "updated_at", notif->params.cinfo->upd_at);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_new_like_notif(const NewLikeNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "new_like");
        pack_kv_map(pk, "params", 6, {
            pack_kv_u64(pk, "channel_id", notif->params.li->chan_id);
            pack_kv_u64(pk, "post_id", notif->params.li->post_id);
            pack_kv_u64(pk, "comment_id", notif->params.li->cmt_id);
            pack_kv_str(pk, "user_name", notif->params.li->user.name);
            pack_kv_str(pk, "user_did", notif->params.li->user.did);
            pack_kv_u64(pk, "total_count", notif->params.li->total_cnt);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_new_sub_notif(const NewSubNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "new_subscription");
        pack_kv_map(pk, "params", 3, {
            pack_kv_u64(pk, "channel_id", notif->params.chan_id);
            pack_kv_str(pk, "user_name", notif->params.uinfo->name);
            pack_kv_str(pk, "user_did", notif->params.uinfo->did);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_chan_upd_notif(const ChanUpdNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "feedinfo_update");
        pack_kv_map(pk, "params", 6, {
            pack_kv_u64(pk, "id", notif->params.cinfo->chan_id);
            pack_kv_str(pk, "name", notif->params.cinfo->name);
            pack_kv_str(pk, "introduction", notif->params.cinfo->intro);
            pack_kv_u64(pk, "subscribers", notif->params.cinfo->subs);
            pack_kv_u64(pk, "last_update", notif->params.cinfo->upd_at);
            pack_kv_bin(pk, "avatar", notif->params.cinfo->avatar, notif->params.cinfo->len);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_stats_changed_notif(const StatsChangedNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "statistics_changed");
        pack_kv_map(pk, "params", 1, {
            pack_kv_u64(pk, "total_clients", notif->params.total_cs);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_report_cmt_notif(const ReportCmtNotif *notif)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "report_illegal_comment");
        pack_kv_map(pk, "params", 6, {
            pack_kv_u64(pk, "channel_id", notif->params.li->chan_id);
            pack_kv_u64(pk, "post_id", notif->params.li->post_id);
            pack_kv_u64(pk, "comment_id", notif->params.li->cmt_id);
            pack_kv_str(pk, "reporter_name", notif->params.li->reporter.name);
            pack_kv_str(pk, "reporter_did", notif->params.li->reporter.did);
            pack_kv_str(pk, "reasons", notif->params.li->reasons);
            pack_kv_u64(pk, "created_at", notif->params.li->created_at);
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
    return rpc_marshal_err(resp->tsx_id, resp->ec, err_strerror(resp->ec));
}

Marshalled *rpc_marshal_create_chan_req(const CreateChanReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "create_channel");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 4, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_str(pk, "name", req->params.name);
            pack_kv_str(pk, "introduction", req->params.intro);
            pack_kv_bin(pk, "avatar", req->params.avatar, req->params.sz);
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
        pack_kv_str(pk, "version", "1.0");
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

Marshalled *rpc_marshal_upd_chan_resp(const UpdChanResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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

Marshalled *rpc_marshal_declare_post_req(const DeclarePostReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "declarelish_post");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 4, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_bin(pk, "content", req->params.content, req->params.sz);
            pack_kv_bool(pk, "with_notify", req->params.with_notify);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_declare_post_resp(const DeclarePostResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
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

Marshalled *rpc_marshal_notify_post_req(const NotifyPostReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "notifylish_post");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 4, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_u64(pk, "post_id", req->params.post_id);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_notify_post_resp(const NotifyPostResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_edit_post_resp(const EditPostResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_del_post_resp(const DelPostResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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

Marshalled *rpc_marshal_edit_cmt_resp(const EditCmtResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_del_cmt_resp(const DelCmtResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_block_cmt_resp(const BlockCmtResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_unblock_cmt_resp(const UnblockCmtResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_post_unlike_req(const PostUnlikeReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "post_unlike");
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

Marshalled *rpc_marshal_post_unlike_resp(const PostUnlikeResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_bool(pk, "is_last", resp->result.is_last);
            pack_kv_arr(pk, "channels", cvector_size(resp->result.cinfos), {
                cvector_foreach(resp->result.cinfos, cinfo) {
                    pack_map(pk, 5, {
                        pack_kv_u64(pk, "id", (*cinfo)->chan_id);
                        pack_kv_str(pk, "name", (*cinfo)->name);
                        pack_kv_str(pk, "introduction", (*cinfo)->intro);
                        pack_kv_u64(pk, "subscribers", (*cinfo)->subs);
                        pack_kv_bin(pk, "avatar", (*cinfo)->avatar, (*cinfo)->len);
                    });
                }
            });
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_bool(pk, "is_last", resp->result.is_last);
            pack_kv_arr(pk, "channels", cvector_size(resp->result.cinfos), {
                cvector_foreach(resp->result.cinfos, cinfo) {
                    pack_map(pk, 8, {
                        pack_kv_u64(pk, "id", (*cinfo)->chan_id);
                        pack_kv_str(pk, "name", (*cinfo)->name);
                        pack_kv_str(pk, "introduction", (*cinfo)->intro);
                        pack_kv_str(pk, "owner_name", (*cinfo)->owner->name);
                        pack_kv_str(pk, "owner_did", (*cinfo)->owner->did);
                        pack_kv_u64(pk, "subscribers", (*cinfo)->subs);
                        pack_kv_u64(pk, "last_update", (*cinfo)->upd_at);
                        pack_kv_bin(pk, "avatar", (*cinfo)->avatar, (*cinfo)->len);
                    });
                }
            });
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 8, {
            pack_kv_u64(pk, "id", resp->result.cinfo->chan_id);
            pack_kv_str(pk, "name", resp->result.cinfo->name);
            pack_kv_str(pk, "introduction", resp->result.cinfo->intro);
            pack_kv_str(pk, "owner_name", resp->result.cinfo->owner->name);
            pack_kv_str(pk, "owner_did", resp->result.cinfo->owner->did);
            pack_kv_u64(pk, "subscribers", resp->result.cinfo->subs);
            pack_kv_u64(pk, "last_update", resp->result.cinfo->upd_at);
            pack_kv_bin(pk, "avatar", resp->result.cinfo->avatar, resp->result.cinfo->len);
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_bool(pk, "is_last", resp->result.is_last);
            pack_kv_arr(pk, "channels", cvector_size(resp->result.cinfos), {
                cvector_foreach(resp->result.cinfos, cinfo) {
                    pack_map(pk, 8, {
                        pack_kv_u64(pk, "id", (*cinfo)->chan_id);
                        pack_kv_str(pk, "name", (*cinfo)->name);
                        pack_kv_str(pk, "introduction", (*cinfo)->intro);
                        pack_kv_str(pk, "owner_name", (*cinfo)->owner->name);
                        pack_kv_str(pk, "owner_did", (*cinfo)->owner->did);
                        pack_kv_u64(pk, "subscribers", (*cinfo)->subs);
                        pack_kv_u64(pk, "last_update", (*cinfo)->upd_at);
                        pack_kv_bin(pk, "avatar", (*cinfo)->avatar, (*cinfo)->len);
                    });
                }
            });
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_bool(pk, "is_last", resp->result.is_last);
            pack_kv_arr(pk, "posts", cvector_size(resp->result.pinfos), {
                cvector_foreach(resp->result.pinfos, pinfo) {
                    pack_map(pk, 8, {
                        pack_kv_u64(pk, "channel_id", (*pinfo)->chan_id);
                        pack_kv_u64(pk, "id", (*pinfo)->post_id);
                        pack_kv_u64(pk, "status", (*pinfo)->stat);
                        (*pinfo)->stat == POST_DELETED ? pack_kv_nil(pk, "content") :
                                                         pack_kv_bin(pk, "content", (*pinfo)->content, (*pinfo)->len);
                        pack_kv_u64(pk, "comments", (*pinfo)->cmts);
                        pack_kv_u64(pk, "likes", (*pinfo)->likes);
                        pack_kv_u64(pk, "created_at", (*pinfo)->created_at);
                        pack_kv_u64(pk, "updated_at", (*pinfo)->upd_at);
                    });
                }
            });
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_posts_lac_resp(const GetPostsLACResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    PostInfo **pinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 1, {
            pack_kv_arr(pk, "posts", cvector_size(resp->result.pinfos), {
                cvector_foreach(resp->result.pinfos, pinfo) {
                    pack_map(pk, 4, {
                        pack_kv_u64(pk, "channel_id", (*pinfo)->chan_id);
                        pack_kv_u64(pk, "post_id", (*pinfo)->post_id);
                        pack_kv_u64(pk, "comments", (*pinfo)->cmts);
                        pack_kv_u64(pk, "likes", (*pinfo)->likes);
                    });
                }
            });
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_liked_posts_req(const GetLikedPostsReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "get_liked_posts");
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

Marshalled *rpc_marshal_get_liked_posts_resp(const GetLikedPostsResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    PostInfo **pinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_bool(pk, "is_last", resp->result.is_last);
            pack_kv_arr(pk, "posts", cvector_size(resp->result.pinfos), {
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_bool(pk, "is_last", resp->result.is_last);
            pack_kv_arr(pk, "comments", cvector_size(resp->result.cinfos), {
                cvector_foreach(resp->result.cinfos, cinfo) {
                    pack_map(pk, 11, {
                        pack_kv_u64(pk, "channel_id", (*cinfo)->chan_id);
                        pack_kv_u64(pk, "post_id", (*cinfo)->post_id);
                        pack_kv_u64(pk, "id", (*cinfo)->cmt_id);
                        pack_kv_u64(pk, "status", (*cinfo)->stat);
                        pack_kv_u64(pk, "comment_id", (*cinfo)->reply_to_cmt);
                        pack_kv_str(pk, "user_name", (*cinfo)->user.name);
                        pack_kv_str(pk, "user_did", (*cinfo)->user.did);
                        (*cinfo)->stat == CMT_AVAILABLE ? pack_kv_bin(pk, "content", (*cinfo)->content, (*cinfo)->len) :
                                                          pack_kv_nil(pk, "content");
                        pack_kv_u64(pk, "likes", (*cinfo)->likes);
                        pack_kv_u64(pk, "created_at", (*cinfo)->created_at);
                        pack_kv_u64(pk, "updated_at", (*cinfo)->upd_at);
                    });
                }
            });
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_cmts_likes_resp(const GetCmtsLikesResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    CmtInfo **cinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 1, {
            pack_kv_arr(pk, "comments", cvector_size(resp->result.cinfos), {
                cvector_foreach(resp->result.cinfos, cinfo) {
                    pack_map(pk, 4, {
                        pack_kv_u64(pk, "channel_id", (*cinfo)->chan_id);
                        pack_kv_u64(pk, "post_id", (*cinfo)->post_id);
                        pack_kv_u64(pk, "id", (*cinfo)->cmt_id);
                        pack_kv_u64(pk, "likes", (*cinfo)->likes);
                    });
                }
            });
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 3, {
            pack_kv_str(pk, "did", resp->result.did);
            pack_kv_u64(pk, "connecting_clients", resp->result.conn_cs);
            pack_kv_u64(pk, "total_clients", resp->result.total_cs);
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
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
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_srv_ver_req(const GetSrvVerReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
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

Marshalled *rpc_marshal_get_srv_ver_resp(const GetSrvVerResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_str(pk, "version", resp->result.version);
            pack_kv_i64(pk, "version_code", resp->result.version_code);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_report_illegal_cmt_req(const ReportIllegalCmtReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "report_illegal_comment");
        pack_kv_u64(pk, "id", req->tsx_id);
        pack_kv_map(pk, "params", 5, {
            pack_kv_str(pk, "access_token", req->params.tk);
            pack_kv_u64(pk, "channel_id", req->params.chan_id);
            pack_kv_u64(pk, "post_id", req->params.post_id);
            pack_kv_u64(pk, "comment_id", req->params.cmt_id);
            pack_kv_str(pk, "reasons", req->params.reasons);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_report_illegal_cmt_resp(const ReportIllegalCmtResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_nil(pk, "result");
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *rpc_marshal_get_reported_cmts_req(const GetReportedCmtsReq *req)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 4, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_str(pk, "method", "get_comments");
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

Marshalled *rpc_marshal_get_reported_cmts_resp(const GetReportedCmtsResp *resp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);
    ReportedCmtInfo **rcinfo;

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", resp->tsx_id);
        pack_kv_map(pk, "result", 2, {
            pack_kv_bool(pk, "is_last", resp->result.is_last);
            pack_kv_arr(pk, "comments", cvector_size(resp->result.rcinfos), {
                cvector_foreach(resp->result.rcinfos, rcinfo) {
                    pack_map(pk, 11, {
                        pack_kv_u64(pk, "channel_id", (*rcinfo)->chan_id);
                        pack_kv_u64(pk, "post_id", (*rcinfo)->post_id);
                        pack_kv_u64(pk, "comment_id", (*rcinfo)->cmt_id);
                        pack_kv_str(pk, "reporter_name", (*rcinfo)->reporter.name);
                        pack_kv_str(pk, "reporter_did", (*rcinfo)->reporter.did);
                        pack_kv_str(pk, "reasons", (*rcinfo)->reasons);
                        pack_kv_u64(pk, "created_at", (*rcinfo)->created_at);
                    });
                }
            });
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *marshal_set_binary_resp(const Resp *resp)
{
    SetBinaryResp *wrap_resp = (SetBinaryResp*)resp;

    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", wrap_resp->tsx_id);
        pack_kv_map(pk, "result", 1, {
            pack_kv_str(pk, "key", wrap_resp->result.key);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

Marshalled *marshal_get_binary_resp(const Resp *resp)
{
    GetBinaryResp *wrap_resp = (GetBinaryResp*)resp;

    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", wrap_resp->tsx_id);
        pack_kv_map(pk, "result", 4, {
            pack_kv_str(pk, "key", wrap_resp->result.key);
            pack_kv_str(pk, "algo", wrap_resp->result.algo);
            pack_kv_str(pk, "checksum", wrap_resp->result.checksum);
            pack_kv_bin_withzero(pk, "content", wrap_resp->result.content, wrap_resp->result.content_sz);
        });
    });
    deref(wrap_resp->result.content);

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}

typedef Marshalled *RespHdlr(const Resp *resp);
struct RespSerializer {
    char *method;
    RespHdlr *serializer;
};
static struct RespSerializer resp_serializers_1_0[] = {
    {"set_binary"              , marshal_set_binary_resp        },
    {"get_binary"              , marshal_get_binary_resp        },
};

Marshalled *rpc_marshal_resp(const char* method, const Resp *resp)
{
    struct RespSerializer *resp_serializers;
    int resp_serializers_size;
    int i;

    resp_serializers = resp_serializers_1_0;
    resp_serializers_size = sizeof(resp_serializers_1_0) / sizeof(*resp_serializers_1_0);
    for (i = 0; i < resp_serializers_size; ++i) {
        if (!strcmp(method, resp_serializers[i].method)) {
            return resp_serializers[i].serializer(resp);
        }
    }

    vlogE(TAG_RPC "RespSerializer: not a valid method.");
    return NULL;
}

Marshalled *rpc_marshal_err(uint64_t tsx_id, int64_t errcode, const char *errdesp)
{
    msgpack_sbuffer *buf = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buf, msgpack_sbuffer_write);
    MarshalledIntl *m = rc_zalloc(sizeof(MarshalledIntl), mintl_dtor);

    pack_map(pk, 3, {
        pack_kv_str(pk, "version", "1.0");
        pack_kv_u64(pk, "id", tsx_id);
        pack_kv_map(pk, "error", 2, {
            pack_kv_i64(pk, "code", errcode);
            pack_kv_str(pk, "message", errdesp);
        });
    });

    m->m.data = buf->data;
    m->m.sz   = buf->size;
    m->buf    = buf;

    msgpack_packer_free(pk);

    return &m->m;
}
