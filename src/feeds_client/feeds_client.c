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

#include <string.h>
#include <pthread.h>
#include <crystal.h>
#include <ela_did.h>
#include <ela_jwt.h>

#include "feeds_client.h"
#include "cfg.h"

#ifdef DID_TESTNET
static const char *resolver = "http://api.elastos.io:21606";
#else
static const char *resolver = "http://api.elastos.io:20606";
#endif
const char *mnemonic = "advance duty suspect finish space matter squeeze elephant twenty over stick shield";

struct FeedsClient {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    ElaCarrier *carrier;
    bool waiting_response;
    pthread_t carrier_routine_tid;
    DIDStore *store;
    char did[ELA_MAX_DID_LEN];
    char *passwd;
    char *access_token;
    void *resp;
    ErrResp *err;
    void *unmarshal;
};

static
void connection_callback(ElaCarrier *w, ElaConnectionStatus status, void *context)
{
    FeedsClient *fc = (FeedsClient *)context;

    pthread_mutex_lock(&fc->lock);
    pthread_mutex_unlock(&fc->lock);
    pthread_cond_signal(&fc->cond);
}

static
void friend_connection_callback(ElaCarrier *w, const char *friendid,
                                ElaConnectionStatus status, void *context)
{
    FeedsClient *fc = (FeedsClient *)context;

    pthread_mutex_lock(&fc->lock);
    pthread_mutex_unlock(&fc->lock);
    pthread_cond_signal(&fc->cond);
}

static
void console(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

static
void on_receiving_message(ElaCarrier *carrier, const char *from,
                          const void *msg, size_t len, int64_t timestamp,
                          bool offline, void *context)
{
    FeedsClient *fc = (FeedsClient *)context;
    int (*unmarshal)(void **, ErrResp **) = fc->unmarshal;
    Notif *notif;
    uint64_t id;
    int rc;

    rc = rpc_unmarshal_notif_or_resp_id(msg, len, &notif, &id);
    if (rc < 0)
        return;

    if (notif) {
        if (!strcmp(notif->method, "new_post")) {
            NewPostNotif *n = (NewPostNotif *)notif;
            console("New post:");
            console("  channel_id: %llu, id: %llu, content: %s, created_at: %llu",
                    n->params.pinfo->chan_id, n->params.pinfo->post_id,
                    n->params.pinfo->content, n->params.pinfo->created_at);
        } else if (!strcmp(notif->method, "new_comment")) {
            NewCmtNotif *n = (NewCmtNotif *)notif;
            console("New comment:");
            console("  channel_id: %llu, post_id: %llu, id: %llu, comment_id: %llu, user_name: %s, content: %s, created_at: %llu",
                    n->params.cinfo->chan_id,
                    n->params.cinfo->post_id,
                    n->params.cinfo->cmt_id,
                    n->params.cinfo->reply_to_cmt,
                    n->params.cinfo->user.name,
                    n->params.cinfo->content,
                    n->params.cinfo->created_at);
        } else {
            NewLikesNotif *n = (NewLikesNotif *)notif;
            console("New like:");
            console("  channel_id: %llu, post_id: %llu, comment_id: %llu, count: %llu",
                    n->params.chan_id,
                    n->params.post_id,
                    n->params.cmt_id,
                    n->params.cnt);
        }
        deref(notif);
        return;
    }

    pthread_mutex_lock(&fc->lock);
    if (fc->waiting_response) {
        if (!unmarshal(&fc->resp, &fc->err))
            pthread_cond_signal(&fc->cond);
    }
    pthread_mutex_unlock(&fc->lock);
}

static
void *carrier_routine(void *arg)
{
    ela_run((ElaCarrier *)arg, 10);
    return NULL;
}

static
void feeds_client_destructor(void *obj)
{
    FeedsClient *fc = (FeedsClient *)obj;

    pthread_mutex_destroy(&fc->lock);
    pthread_cond_destroy(&fc->cond);

    if (fc->carrier)
        ela_kill(fc->carrier);

    if (fc->store)
        DIDStore_Close(fc->store);

    if (fc->passwd)
        free(fc->passwd);

    if (fc->access_token)
        free(fc->access_token);

    deref(fc->resp);
    deref(fc->err);
}

static
bool create_id_tsx(DIDAdapter *adapter, const char *payload, const char *memo)
{
    (void)adapter;
    (void)memo;
    (void)strdup(payload);

    return true;
}

static
DIDDocument* merge_to_localcopy(DIDDocument *chaincopy, DIDDocument *localcopy)
{
    if (!chaincopy && !localcopy)
        return NULL;

    if (!chaincopy)
        return chaincopy;

    return localcopy;
}

FeedsClient *feeds_client_create(FeedsCLIConfig *opts)
{
    DIDAdapter adapter = {
        .createIdTransaction = create_id_tsx
    };
    ElaCallbacks callbacks;
    FeedsClient *fc;
    int rc;

    DIDBackend_InitializeDefault(resolver, opts->didcache_dir);

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.connection_status = connection_callback;
    callbacks.friend_connection = friend_connection_callback;
    callbacks.friend_message = on_receiving_message;

    fc = rc_zalloc(sizeof(FeedsClient), feeds_client_destructor);
    if (!fc)
        return NULL;

    fc->store = DIDStore_Open(opts->didstore_dir, &adapter);
    if (!fc->store) {
        deref(fc);
        return NULL;
    }

    if (!DIDStore_ContainsPrivateIdentity(fc->store)) {
        //DIDDocument *doc;
        //DID *did;
        DIDStore_InitPrivateIdentity(fc->store, opts->didstore_passwd, mnemonic, "secret", "english", true);
        //for (int i = 0; i < 24; ++i) {
        //    char did[ELA_MAX_DID_LEN];
        //    DIDDocument *doc;
        //    doc = DIDStore_NewDIDByIndex(fc->store, opts->didstore_passwd, i, NULL);
        //    printf("%d. %s\n", i, DID_ToString(DIDDocument_GetSubject(doc), did, sizeof(did)));
        //    DIDDocument_Destroy(doc);
        //}
        //doc = DIDStore_NewDIDByIndex(fc->store, opts->didstore_passwd, 0, NULL);

        //did = DID_FromString("did:elastos:ijUnD4KeRpeBUFmcEDCbhxMTJRzUYCQCZM");
        DIDStore_Synchronize(fc->store, opts->didstore_passwd, merge_to_localcopy);
        //doc = DID_Resolve(did, true);
        //DIDStore_StoreDID(fc->store, doc, NULL);
        //DIDDocument_Destroy(doc);
        //DID_Destroy(did);
    }

    strcpy(fc->did, "did:elastos:ieaA5VMWydQmVJtM5daW5hoTQpcuV38mHM");
    fc->passwd = strdup(opts->didstore_passwd);

    pthread_mutex_init(&fc->lock, NULL);
    pthread_cond_init(&fc->cond, NULL);

    fc->carrier = ela_new(&opts->carrier_opts, &callbacks, fc);
    if (!fc->carrier) {
        deref(fc);
        return NULL;
    }

    rc = pthread_create(&fc->carrier_routine_tid, NULL, carrier_routine, fc->carrier);
    if (rc < 0) {
        deref(fc);
        return NULL;
    }

    return fc;
}

void feeds_client_wait_until_online(FeedsClient *fc)
{
    pthread_mutex_lock(&fc->lock);
    while (!ela_is_ready(fc->carrier))
        pthread_cond_wait(&fc->cond, &fc->lock);
    pthread_mutex_unlock(&fc->lock);
}

void feeds_client_delete(FeedsClient *fc)
{
    pthread_t tid = fc->carrier_routine_tid;

    deref(fc);
    pthread_join(tid, NULL);
}

ElaCarrier *feeds_client_get_carrier(FeedsClient *fc)
{
    return fc->carrier;
}

int feeds_client_friend_add(FeedsClient *fc, const char *address, const char *hello)
{
    char node_id[ELA_MAX_ID_LEN + 1];
    int rc = 0;

    ela_get_id_by_address(address, node_id, sizeof(node_id));
    if (!ela_is_friend(fc->carrier, node_id)) {
        rc = ela_add_friend(fc->carrier, address, hello);
        if (rc < 0)
            return -1;
    }

    feeds_client_wait_until_friend_connected(fc, node_id);

    return rc;
}

int feeds_client_wait_until_friend_connected(FeedsClient *fc, const char *friend_node_id)
{
    ElaFriendInfo info;
    int rc;

    pthread_mutex_lock(&fc->lock);
    while (1) {
        rc = ela_get_friend_info(fc->carrier, friend_node_id, &info);
        if (rc < 0)
            break;

        if (info.status != ElaConnectionStatus_Connected)
            pthread_cond_wait(&fc->cond, &fc->lock);
        else
            break;
    }
    pthread_mutex_unlock(&fc->lock);

    return rc;
}

int feeds_client_friend_remove(FeedsClient *fc, const char *user_id)
{
    return ela_remove_friend(fc->carrier, user_id);
}

int transaction_start(FeedsClient *fc, const char *svc_addr, const void *req, size_t len,
                      void **resp, ErrResp **err)
{
    int rc;

    pthread_mutex_lock(&fc->lock);

    fc->waiting_response = true;
    rc = ela_send_friend_message(fc->carrier, svc_addr, req, len, NULL);
    if (rc < 0) {
        fc->waiting_response = false;
        pthread_mutex_unlock(&fc->lock);
        return -1;
    }

    while (!fc->resp && !fc->err)
        pthread_cond_wait(&fc->cond, &fc->lock);

    *resp = fc->resp;
    *err = fc->err;

    fc->resp = NULL;
    fc->err = NULL;
    fc->waiting_response = false;

    pthread_mutex_unlock(&fc->lock);

    return 0;
}

int feeds_client_decl_owner(FeedsClient *fc, const char *svc_node_id, DeclOwnerResp **resp, ErrResp **err)
{
    DeclOwnerReq req = {
        .method = "declare_owner",
        .tsx_id = 1,
        .params = {
            .nonce = "abc",
            .owner_did = fc->did
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_decl_owner_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_decl_owner_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_imp_did(FeedsClient *fc, const char *svc_node_id, ImpDIDResp **resp, ErrResp **err)
{
    ImpDIDReq req = {
        .method = "import_did",
        .tsx_id = 1,
        .params = {
            .mnemo = (char *)mnemonic,
            .passphrase = "secret",
            .idx = 0
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_imp_did_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_imp_did_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

static
char *iss_vc(FeedsClient *fc, const char *sub)
{
    DID *did = DID_FromString(fc->did);
    Issuer *iss = Issuer_Create(did, NULL, fc->store);
    DID *owner = DID_FromString(sub);
    DIDURL *url = DIDURL_NewByDid(owner, "credential");
    const char *types[] = {"BasicProfileCredential"};
    const char *propdata = "{\"name\":\"Jay Holtslander\"}";
    Credential *vc = Issuer_CreateCredentialByString(iss, owner, url, types, 1, propdata,
                                         time(NULL) + 3600 * 24 * 365 * 20, fc->passwd);
    char *vc_str = (char *)Credential_ToJson(vc, true);
    DID_Destroy(did);
    Issuer_Destroy(iss);
    DID_Destroy(owner);
    DIDURL_Destroy(url);
    Credential_Destroy(vc);

    return vc_str;
}

int feeds_client_iss_vc(FeedsClient *fc, const char *svc_node_id, const char *sub,
                        IssVCResp **resp, ErrResp **err)
{
    IssVCReq req = {
        .method = "issue_credential",
        .tsx_id = 1,
        .params = {
            .vc = iss_vc(fc, sub)
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_iss_vc_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_iss_vc_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_signin1(FeedsClient *fc, const char *svc_node_id, SigninReqChalResp **resp, ErrResp **err)
{
    SigninReqChalReq req = {
        .method = "signin_request_challenge",
        .tsx_id = 1,
        .params = {
            .iss = fc->did,
            .vc_req = true
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_signin_req_chal_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_signin_req_chal_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

static
char *gen_jws(FeedsClient *fc, const char *realm, const char *nonce)
{
    DID *did = DID_FromString(fc->did);
    Presentation *vp = Presentation_Create(did, NULL, fc->store, fc->passwd, nonce, realm, 0);
    DIDDocument *doc = DIDStore_LoadDID(fc->store, did);
    JWTBuilder *b = DIDDocument_GetJwtBuilder(doc);
    JWTBuilder_SetClaimWithJson(b, "presentation", Presentation_ToJson(vp, true));
    JWTBuilder_Sign(b, NULL, fc->passwd);
    char *str = (char *)JWTBuilder_Compact(b);
    JWTBuilder_Destroy(b);
    Presentation_Destroy(vp);

    return str;
}

static
char *gen_vc(FeedsClient *fc)
{
    DID *did = DID_FromString(fc->did);
    Issuer *iss = Issuer_Create(did, NULL, fc->store);
    DIDURL *url = DIDURL_NewByDid(did, "credential");
    const char *types[] = {"BasicProfileCredential"};
    const char *propdata = "{\"name\":\"Jay Holtslander\",\"email\":\"djd@aaa.com\"}";
    Credential *vc = Issuer_CreateCredentialByString(iss, did, url, types, 1, propdata,
                                                     time(NULL) + 3600 * 24 * 365 * 20, fc->passwd);
    char *vc_str = (char *)Credential_ToJson(vc, true);
    DID_Destroy(did);
    Issuer_Destroy(iss);
    DIDURL_Destroy(url);
    Credential_Destroy(vc);

    return vc_str;
}

int feeds_client_signin2(FeedsClient *fc, const char *svc_node_id,
                         const char *realm, const char *nonce, SigninConfChalResp **resp, ErrResp **err)
{
    SigninConfChalReq req = {
        .method = "signin_confirm_challenge",
        .tsx_id = 1,
        .params = {
            .jws = gen_jws(fc, realm, nonce),
            .vc = gen_vc(fc)
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_signin_conf_chal_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_signin_conf_chal_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    if (!rc)
        fc->access_token = strdup((*resp)->result.tk);
    deref(marshal);
    return rc;
}

int feeds_client_create_channel(FeedsClient *fc, const char *svc_node_id, const char *name,
                                const char *intro, CreateChanResp **resp, ErrResp **err)
{
    CreateChanReq req = {
        .method = "create_channel",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .name = (char *)name,
            .intro = (char *)intro
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_create_chan_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_create_chan_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_publish_post(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                              const char *content, PubPostResp **resp, ErrResp **err)
{
    PubPostReq req = {
        .method = "publish_post",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .chan_id = channel_id,
            .content = (char *)content,
            .sz = strlen(content) + 1
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_pub_post_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_pub_post_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_post_comment(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                              uint64_t post_id, uint64_t comment_id, const char *content,
                              PostCmtResp **resp, ErrResp **err)
{
    PostCmtReq req = {
        .method = "post_comment",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .chan_id = channel_id,
            .post_id = post_id,
            .cmt_id = comment_id,
            .content = (char *)content,
            .sz = strlen(content) + 1
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_post_cmt_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_post_cmt_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_post_like(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                           uint64_t post_id, uint64_t comment_id, PostLikeResp **resp, ErrResp **err)
{
    PostLikeReq req = {
        .method = "post_like",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .chan_id = channel_id,
            .post_id = post_id,
            .cmt_id = comment_id
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_post_like_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_post_like_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_post_unlike(FeedsClient *fc, const char *svc_node_id, uint64_t channel_id,
                             uint64_t post_id, uint64_t comment_id, PostUnlikeResp **resp, ErrResp **err)
{
    PostUnlikeReq req = {
        .method = "post_unlike",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .chan_id = channel_id,
            .post_id = post_id,
            .cmt_id = comment_id
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_post_unlike_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_post_unlike_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_my_channels(FeedsClient *fc, const char *svc_node_id, QryFld qf,
                                 uint64_t upper, uint64_t lower, uint64_t maxcnt,
                                 GetMyChansResp **resp, ErrResp **err)
{
    GetMyChansReq req = {
        .method = "get_my_channels",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .qc = {
                .by = qf,
                .upper = upper,
                .lower = lower,
                .maxcnt = maxcnt
            }
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_my_chans_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_my_chans_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_my_channels_metadata(FeedsClient *fc, const char *svc_node_id, QryFld qf,
                                          uint64_t upper, uint64_t lower, uint64_t maxcnt,
                                          GetMyChansMetaResp **resp, ErrResp **err)
{
    GetMyChansMetaReq req = {
        .method = "get_my_channels_metadata",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .qc = {
                .by = qf,
                .upper = upper,
                .lower = lower,
                .maxcnt = maxcnt
            }
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_my_chans_meta_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_my_chans_meta_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_channels(FeedsClient *fc, const char *svc_node_id, QryFld qf,
                              uint64_t upper, uint64_t lower, uint64_t maxcnt,
                              GetChansResp **resp, ErrResp **err)
{
    GetChansReq req = {
        .method = "get_channels",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .qc = {
                .by = qf,
                .upper = upper,
                .lower = lower,
                .maxcnt = maxcnt
            }
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_chans_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_chans_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_channel_detail(FeedsClient *fc, const char *svc_node_id, uint64_t id,
                                    GetChanDtlResp **resp, ErrResp **err)
{
    GetChanDtlReq req = {
        .method = "get_channel_detail",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .id = id
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_chan_dtl_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_chan_dtl_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_subscribed_channels(FeedsClient *fc, const char *svc_node_id, QryFld qf,
                                         uint64_t upper, uint64_t lower, uint64_t maxcnt,
                                         GetSubChansResp **resp, ErrResp **err)
{
    GetSubChansReq req = {
        .method = "get_subscribed_channels",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .qc = {
                .by = qf,
                .upper = upper,
                .lower = lower,
                .maxcnt = maxcnt
            }
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_sub_chans_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_sub_chans_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_posts(FeedsClient *fc, const char *svc_node_id, uint64_t cid, QryFld qf,
                           uint64_t upper, uint64_t lower, uint64_t maxcnt, GetPostsResp **resp, ErrResp **err)
{
    GetPostsReq req = {
        .method = "get_posts",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .chan_id = cid,
            .qc = {
                .by = qf,
                .upper = upper,
                .lower = lower,
                .maxcnt = maxcnt
            }
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_posts_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_posts_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_liked_posts(FeedsClient *fc, const char *svc_node_id, QryFld qf, uint64_t upper,
                                 uint64_t lower, uint64_t maxcnt, GetLikedPostsResp **resp, ErrResp **err)
{
    GetLikedPostsReq req = {
        .method = "get_liked_posts",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .qc = {
                .by = qf,
                .upper = upper,
                .lower = lower,
                .maxcnt = maxcnt
            }
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_liked_posts_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_liked_posts_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_comments(FeedsClient *fc, const char *svc_node_id, uint64_t cid, uint64_t pid,
                              QryFld qf, uint64_t upper, uint64_t lower, uint64_t maxcnt,
                              GetCmtsResp **resp, ErrResp **err)
{
    GetCmtsReq req = {
        .method = "get_comments",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .chan_id = cid,
            .post_id = pid,
            .qc = {
                .by = qf,
                .upper = upper,
                .lower = lower,
                .maxcnt = maxcnt
            }
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_cmts_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_cmts_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_get_statistics(FeedsClient *fc, const char *svc_node_id, GetStatsResp **resp, ErrResp **err)
{
    GetStatsReq req = {
        .method = "get_statistics",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_get_stats_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_get_stats_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_subscribe_channel(FeedsClient *fc, const char *svc_node_id, uint64_t id,
                                   SubChanResp **resp, ErrResp **err)
{
    SubChanReq req = {
        .method = "subscribe_channel",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .id = id
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_sub_chan_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_sub_chan_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_unsubscribe_channel(FeedsClient *fc, const char *svc_node_id, uint64_t id,
                                     UnsubChanResp **resp, ErrResp **err)
{
    UnsubChanReq req = {
        .method = "unsubscribe_channel",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
            .id = id
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_unsub_chan_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_unsub_chan_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}

int feeds_client_enable_notification(FeedsClient *fc, const char *svc_node_id,
                                     EnblNotifResp **resp, ErrResp **err)
{
    EnblNotifReq req = {
        .method = "enable_notification",
        .tsx_id = 1,
        .params = {
            .tk = fc->access_token,
        }
    };
    Marshalled *marshal;
    int rc;

    marshal = rpc_marshal_enbl_notif_req(&req);
    if (!marshal)
        return -1;

    fc->unmarshal = rpc_unmarshal_enbl_notif_resp;
    rc = transaction_start(fc, svc_node_id, marshal->data, marshal->sz, (void **)resp, err);
    deref(marshal);
    return rc;
}
