#include <limits.h>

#include <crystal.h>
#include <ela_jwt.h>
#include <ela_did.h>
#include <ela_carrier.h>

#include "auth.h"
#include "err.h"
#include "did.h"
#include "db.h"

typedef struct {
    UserInfo info;
    JWS *token;
} AccessTokenUserInfo;

typedef struct {
    UserInfo info;
    char did_buf[ELA_MAX_DID_LEN];
} VCUserInfo;

typedef struct {
    hash_entry_t he;
    char nonce[NONCE_BYTES * 2];
    char sub[ELA_MAX_DID_LEN];
    time_t expat;
} Login;

extern ElaCarrier *carrier;

static hashtable_t *pending_logins;

static inline
Login *pending_login_put(Login *login)
{
    return hashtable_put(pending_logins, &login->he);
}

static inline
Login *pending_login_remove(const char *nonce)
{
    return hashtable_remove(pending_logins, nonce, strlen(nonce));
}

static
Login *login_create(const char *sub)
{
    uint8_t buf[NONCE_BYTES];
    Login *login;

    login = rc_zalloc(sizeof(Login), NULL);
    if (!login) {
        vlogE("OOM.");
        return NULL;
    }

    crypto_random_nonce(buf);
    crypto_nonce_to_str(buf, login->nonce, sizeof(login->nonce));

    strcpy(login->sub, sub);

    login->expat = time(NULL) + 60;

    login->he.data   = login;
    login->he.key    = login->nonce;
    login->he.keylen = strlen(login->nonce);

    return login;
}

static
char *gen_chal(const char *realm, const char *nonce)
{
    char *chal_marshal;
    JWTBuilder *chal;

    chal = DIDDocument_GetJwtBuilder(feeds_doc);
    if (!chal) {
        vlogE("Editting challenge JWT failed.");
        return NULL;
    }

    if (!JWTBuilder_SetSubject(chal, "didauth")) {
        vlogE("Setting subject claim failed.");
        JWTBuilder_Destroy(chal);
        return NULL;
    }

    if (!JWTBuilder_SetClaim(chal, "realm", realm)) {
        vlogE("Setting realm claim failed.");
        JWTBuilder_Destroy(chal);
        return NULL;
    }

    if (!JWTBuilder_SetClaim(chal, "nonce", nonce)) {
        vlogE("Setting nonce claim failed.");
        JWTBuilder_Destroy(chal);
        return NULL;
    }

    if (JWTBuilder_Sign(chal, feeeds_auth_key_url, feeds_storepass)) {
        vlogE("Signing JWT failed.");
        JWTBuilder_Destroy(chal);
        return NULL;
    }

    chal_marshal = (char *)JWTBuilder_Compact(chal);
    JWTBuilder_Destroy(chal);
    if (!chal_marshal) {
        vlogE("Marshalling challenge JWT failed.");
        return NULL;
    }

    return chal_marshal;
}

void hdl_signin_req_chal_req(ElaCarrier *c, const char *from, Req *base)
{
    SigninReqChalReq *req = (SigninReqChalReq *)base;
    Marshalled *resp_marshal = NULL;
    char nid[ELA_MAX_ID_LEN + 1];
    Login *login = NULL;
    char *chal = NULL;
    char *vc = NULL;

    vlogI("Received signin_request_challenge request from [%s].", from);
    vlogD("  iss: %s", req->params.iss);
    vlogD("  credential_required: %s", req->params.vc_req ? "true" : "false");

    if (did_is_binding()) {
        vlogE("Feeds is in setup mode.");
        return;
    }

    if (strlen(req->params.iss) >= ELA_MAX_DID_LEN) {
        vlogE("Invalid iss in signin_request_challenge.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    login = login_create(req->params.iss);
    if (!login) {
        vlogE("Creating login object failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    chal = gen_chal(ela_get_nodeid(c, nid, sizeof(nid)), login->nonce);
    if (!chal) {
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (req->params.vc_req &&
        !(vc = (char *)Credential_ToJson(feeds_vc, true))) {
        vlogE("Feeds VC to string failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    pending_login_put(login);
    vlogI("User[%s] requests to login[%s]", req->params.iss, login->nonce);

    {
        SigninReqChalResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .vc_req = false,
                .jws    = chal,
                .vc     = vc
            }
        };
        resp_marshal = rpc_marshal_signin_req_chal_resp(&resp);
        vlogI("Sending signin_request_challenge response.");
        vlogD("  credential_required: %s", resp.result.vc_req ? "true" : "false");
        vlogD("  jws: %s", resp.result.jws);
        vlogD("  credential: %s", resp.result.vc ? resp.result.vc : "nil");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (chal)
        free(chal);
    if (vc)
        free(vc);
    deref(login);
}

static
bool chal_resp_is_valid(JWS *chan_resp)
{
    char nid[ELA_MAX_ID_LEN + 1];
    char signer_did[ELA_MAX_DID_LEN];
    const char *vp_str;
    Presentation *vp;
    Login *login;

    if (!(vp_str = JWS_GetClaimAsJson(chan_resp, "presentation"))) {
        vlogE("Invalid challenge response: missing presentation.");
        return false;
    }

    if (!(vp = Presentation_FromJson(vp_str))) {
        vlogE("Invalid challenge response: unmarshalling presentation failed.");
        return false;
    }

    if (!Presentation_IsValid(vp)) {
        vlogE("Invalid challenge response presentation.");
        Presentation_Destroy(vp);
        return false;
    }

    if (strcmp(DID_ToString(Presentation_GetSigner(vp), signer_did, sizeof(signer_did)),
               JWS_GetIssuer(chan_resp))) {
        vlogE("Invalid challenge response presentation signer and jws issuer mismatch.");
        Presentation_Destroy(vp);
        return false;
    }

    if (strcmp(Presentation_GetRealm(vp), ela_get_nodeid(carrier, nid, sizeof(nid)))) {
        vlogE("Invalid challenge response realm.");
        Presentation_Destroy(vp);
        return false;
    }

    if (!(login = pending_login_remove(Presentation_GetNonce(vp)))) {
        vlogE("Invalid challenge response nonce.");
        Presentation_Destroy(vp);
        return false;
    }

    if (strcmp(signer_did, login->sub)) {
        vlogE("Invalid challenge response signer.");
        Presentation_Destroy(vp);
        deref(login);
        return false;
    }

    Presentation_Destroy(vp);
    deref(login);
    return true;
}

static
char *gen_access_token(UserInfo *uinfo)
{
    JWTBuilder *token;
    char *marshal;

    token = DIDDocument_GetJwtBuilder(feeds_doc);
    if (!token) {
        vlogE("Editting JWT failed.");
        return NULL;
    }

    if (!JWTBuilder_SetSubject(token, uinfo->did)) {
        vlogE("Setting access token subject failed.");
        JWTBuilder_Destroy(token);
        return NULL;
    }

    if (!JWTBuilder_SetExpiration(token, time(NULL) + 3600 * 24 * 60)) {
        vlogE("Setting access token expiration failed.");
        JWTBuilder_Destroy(token);
        return NULL;
    }

    if (!JWTBuilder_SetClaimWithIntegar(token, "uid", uinfo->uid)) {
        vlogE("Setting access token uid failed.");
        JWTBuilder_Destroy(token);
        return NULL;
    }

    if (!JWTBuilder_SetClaim(token, "name", uinfo->name)) {
        vlogE("Setting access token name failed.");
        JWTBuilder_Destroy(token);
        return NULL;
    }

    if (!JWTBuilder_SetClaim(token, "email", uinfo->email)) {
        vlogE("Setting access token email failed.");
        JWTBuilder_Destroy(token);
        return NULL;
    }

    if (JWTBuilder_Sign(token, feeeds_auth_key_url, feeds_storepass)) {
        vlogE("Signing access token failed.");
        JWTBuilder_Destroy(token);
        return NULL;
    }

    marshal = (char *)JWTBuilder_Compact(token);
    JWTBuilder_Destroy(token);

    if (!marshal)
        vlogE("Marshalling access token failed.");

    return marshal;
}

static
void vcuinfo_dtor(void *obj)
{
    VCUserInfo *ui = obj;

    if (ui->info.name)
        free(ui->info.name);

    if (ui->info.email)
        free(ui->info.email);
}

static
UserInfo *create_uinfo_from_vc(const char *did, Credential *vc)
{
    VCUserInfo *uinfo;

    uinfo = rc_zalloc(sizeof(VCUserInfo), vcuinfo_dtor);
    if (!uinfo) {
        vlogE("OOM.");
        return NULL;
    }

    strcpy(uinfo->did_buf, did);
    uinfo->info.did = uinfo->did_buf;
    uinfo->info.name = (char *)(vc ? Credential_GetProperty(vc, "name") : strdup("NA"));
    uinfo->info.email = (char *)(vc ? Credential_GetProperty(vc, "email") : strdup("NA"));

    if (!uinfo->info.name || !uinfo->info.email) {
        deref(uinfo);
        return NULL;
    }

    return &uinfo->info;
}

void hdl_signin_conf_chal_req(ElaCarrier *c, const char *from, Req *base)
{
    SigninConfChalReq *req = (SigninConfChalReq *)base;
    Marshalled *resp_marshal = NULL;
    char did[ELA_MAX_DID_LEN];
    char *access_token = NULL;
    UserInfo *uinfo = NULL;
    JWS *chal_resp = NULL;
    Credential *vc = NULL;
    int rc;

    vlogI("Received signin_request_challenge request from [%s].", from);
    vlogD("  jws: %s", req->params.jws);
    vlogD("  credential: %s", req->params.vc ? req->params.vc : "nil");

    if (did_is_binding()) {
        vlogE("Feeds is in setup mode.");
        return;
    }

    chal_resp = JWTParser_Parse(req->params.jws);
    if (!chal_resp) {
        vlogE("Invalid jws in signin_confirm_challenge.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!chal_resp_is_valid(chal_resp)) {
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if ((vc = Credential_FromJson(req->params.vc, NULL)) &&
        (strcmp(JWS_GetIssuer(chal_resp),
                DID_ToString(Credential_GetOwner(vc), did, sizeof(did))) ||
         !Credential_IsValid(vc))) {
        vlogE("Invalid credential in signin_confirm_challenge.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    uinfo = create_uinfo_from_vc(JWS_GetIssuer(chal_resp), vc);
    if (!uinfo) {
        vlogE("Creating user info from credential failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!strcmp(uinfo->did, feeds_owner_info.did) && (oinfo_upd(uinfo) < 0)) {
        vlogE("Updating owner info failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    rc = db_upsert_user(uinfo, &uinfo->uid);
    if (rc < 0) {
        vlogE("Upsert user info to database failed.");
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    access_token = gen_access_token(uinfo);
    if (!access_token) {
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vlogI("User[%s] has logged in", uinfo->did);

    {
        SigninConfChalResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .tk  = access_token,
                .exp = time(NULL) + 3600 * 24 * 60
            }
        };
        resp_marshal = rpc_marshal_signin_conf_chal_resp(&resp);
        vlogI("Sending signin_confirm_challenge response.");
        vlogD("  access_token: %s", resp.result.tk);
        vlogD("  exp: %" PRIu64, resp.result.exp);
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (access_token)
        free(access_token);
    if (vc)
        Credential_Destroy(vc);
    if (chal_resp)
        JWS_Destroy(chal_resp);
    deref(uinfo);
}

static
void atuinfo_dtor(void *obj)
{
    AccessTokenUserInfo *usr = (AccessTokenUserInfo *)obj;

    JWS_Destroy(usr->token);
}

static
bool access_token_is_valid(JWS *token)
{
    DIDURL *keyurl = NULL;
    bool valid = false;

    keyurl = DIDURL_FromString(JWS_GetKeyId(token), NULL);
    if (!keyurl) {
        vlogE("Getting access token signing key URL failed.");
        goto finally;
    }

    if (!DIDURL_Equals(keyurl, feeeds_auth_key_url)) {
        vlogE("Getting access token signing key URL mismatch.");
        goto finally;
    }

    if (JWS_GetExpiration(token) < time(NULL)) {
        vlogE("Access token has expired.");
        goto finally;
    }

    valid = true;

finally:
    if (keyurl)
        DIDURL_Destroy(keyurl);

    return valid;
}

UserInfo *create_uinfo_from_access_token(const char *token_marshal)
{
    AccessTokenUserInfo *uinfo = NULL;
    JWS *token = NULL;

    token = JWTParser_Parse(token_marshal);
    if (!token) {
        vlogE("Parsing access token failed.");
        return NULL;
    }

    if (!access_token_is_valid(token))
        goto finally;

    uinfo = rc_zalloc(sizeof(AccessTokenUserInfo), atuinfo_dtor);
    if (!uinfo)
        goto finally;

    uinfo->info.did   = (char *)JWS_GetSubject(token);
    uinfo->info.uid   = JWS_GetClaimAsInteger(token, "uid");
    uinfo->info.name  = (char *)JWS_GetClaim(token, "name");
    uinfo->info.email = (char *)JWS_GetClaim(token, "email");
    uinfo->token      = token;

    token = NULL;

finally:
    if (token)
        JWS_Destroy(token);

    return uinfo ? &uinfo->info : NULL;
}

void auth_deinit()
{
    if (pending_logins)
        deref(pending_logins);
}

int auth_init()
{
    pending_logins = hashtable_create(8, 0, NULL, NULL);
    if (!pending_logins) {
        vlogE("Creating pending logins failed.");
        return -1;
    }

    vlogI("Auth module initialized.");

    return 0;
}

void auth_expire_login()
{
    hashtable_iterator_t it;

    hashtable_iterate(pending_logins, &it);
    while(hashtable_iterator_has_next(&it)) {
        Login *login;
        int rc;

        rc = hashtable_iterator_next(&it, NULL, NULL, (void **)&login);
        if (rc <= 0)
            break;

        if (login->expat < time(NULL)) {
            vlogI("Login[%s] has expired.", login->nonce);
            hashtable_iterator_remove(&it);
        }

        deref(login);
    }
}
