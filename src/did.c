#include <limits.h>
#include <unistd.h>

#include <ela_did.h>
#include <ela_jwt.h>
#include <crystal.h>
#include <qrencode.h>
#include <png.h>

#include "sandbird/sandbird.h"
#include "err.h"
#include "rpc.h"
#include "did.h"
#include "db.h"

#define VC_FRAG "credential"

extern ElaCarrier *carrier;

DIDURL *feeeds_auth_key_url;
UserInfo feeds_owner_info;
DIDDocument *feeds_doc;
char *feeds_storepass;
Credential *feeds_vc;

static char qrcode_path[PATH_MAX];
static DIDStore *feeds_didstore;
static bool http_is_running;
static DID *feeds_did;
static char nonce_str[NONCE_BYTES << 1];
static char feeds_url[1024];
static char feeds_did_str[ELA_MAX_DID_LEN];
static enum {
    OWNER_DECLED = 1,
    DID_IMPED    = 2,
    VC_ISSED     = 4,
    DID_RESOLVED = 8
} state;
static char *payload_buf;

typedef struct {
    UserInfo info;
    char did_buf[ELA_MAX_DID_LEN];
} VCUserInfo;

static
void gen_feeds_url();

static inline
bool state_is_set(int s)
{
    return state & s;
}

static inline
bool state_is_equal(int s)
{
    return state == s;
}

static inline
void state_set(int s)
{
    gen_feeds_url();
    state |= s;
}

static inline
const char *state_str()
{
    if (state_is_set(DID_RESOLVED))
        return "did resolved" ;

    if (state_is_set(VC_ISSED))
        return "credential issued";

    if (state_is_set(DID_IMPED))
        return "did imported";

    if (state_is_set(OWNER_DECLED))
        return "owner declared";

    return "to be configured";
}

static
bool create_id_tsx(DIDAdapter *adapter, const char *payload, const char *memo)
{
    (void)adapter;
    (void)memo;

    payload_buf = strdup(payload);

    return true;
}

static
void gen_feeds_url()
{
    int rc;

    rc = sprintf(feeds_url, "%s://", did_is_ready() ? "feeds" : "feeds_raw");

    if (state_is_set(DID_IMPED))
        sprintf(feeds_url + rc, "%s/", feeds_did_str);

    ela_get_address(carrier, feeds_url + strlen(feeds_url),
                    sizeof(feeds_url) - strlen(feeds_url));

    if (!did_is_ready())
        sprintf(feeds_url + strlen(feeds_url), "/%s", nonce_str);
}

#define INCHES_PER_METER (100.0/2.54)
static
int qrencode(const char *intext, const char *outfile)
{
    QRcode *qrcode;
    static FILE *fp; // avoid clobbering by setjmp.
    png_structp png_ptr;
    png_infop info_ptr;
    png_colorp palette = NULL;
    png_byte alpha_values[2];
    unsigned char *row, *p, *q;
    int x, y, xx, yy, bit;
    int realwidth;
    int margin = 4;
    int size = 3;
    int dpi = 72;
    unsigned char fg_color[4] = {0, 0, 0, 255};
    unsigned char bg_color[4] = {255, 255, 255, 255};

    qrcode = QRcode_encodeString(intext, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qrcode)
        return -1;

    realwidth = (qrcode->width + margin * 2) * size;
    row = (unsigned char *)malloc((realwidth + 7) / 8);
    if (row == NULL) {
        vlogE("OOM");
        QRcode_free(qrcode);
        return -1;
    }

    fp = fopen(outfile, "wb");
    if (fp == NULL) {
        vlogE("Failed to create file: %s", outfile);
        free(row);
        QRcode_free(qrcode);
        return -1;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        vlogE("Failed to initialize PNG writer.");
        fclose(fp);
        free(row);
        QRcode_free(qrcode);
        return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        vlogE("Failed to initialize PNG write.");
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        free(row);
        QRcode_free(qrcode);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        vlogE("Failed to write PNG image.");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        free(row);
        QRcode_free(qrcode);
        return -1;
    }

    palette = (png_colorp) malloc(sizeof(png_color) * 2);
    if (palette == NULL) {
        vlogE("Failed to allocate memory.");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        free(row);
        QRcode_free(qrcode);
        return -1;
    }
    palette[0].red   = fg_color[0];
    palette[0].green = fg_color[1];
    palette[0].blue  = fg_color[2];
    palette[1].red   = bg_color[0];
    palette[1].green = bg_color[1];
    palette[1].blue  = bg_color[2];
    alpha_values[0] = fg_color[3];
    alpha_values[1] = bg_color[3];
    png_set_PLTE(png_ptr, info_ptr, palette, 2);
    png_set_tRNS(png_ptr, info_ptr, alpha_values, 2, NULL);

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr,
                 realwidth, realwidth,
                 1,
                 PNG_COLOR_TYPE_PALETTE,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_pHYs(png_ptr, info_ptr,
                 dpi * INCHES_PER_METER,
                 dpi * INCHES_PER_METER,
                 PNG_RESOLUTION_METER);
    png_write_info(png_ptr, info_ptr);

    /* top margin */
    memset(row, 0xff, (realwidth + 7) / 8);
    for (y = 0; y < margin * size; y++) {
        png_write_row(png_ptr, row);
    }

    /* data */
    p = qrcode->data;
    for (y = 0; y < qrcode->width; y++) {
        memset(row, 0xff, (realwidth + 7) / 8);
        q = row;
        q += margin * size / 8;
        bit = 7 - (margin * size % 8);
        for (x = 0; x < qrcode->width; x++) {
            for (xx = 0; xx < size; xx++) {
                *q ^= (*p & 1) << bit;
                bit--;
                if(bit < 0) {
                    q++;
                    bit = 7;
                }
            }
            p++;
        }
        for (yy = 0; yy < size; yy++) {
            png_write_row(png_ptr, row);
        }
    }
    /* bottom margin */
    memset(row, 0xff, (realwidth + 7) / 8);
    for (y = 0; y < margin * size; y++) {
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);

    free(palette);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    free(row);
    QRcode_free(qrcode);

    return 0;
}

static
int hdl_http_req(sb_Event *ev)
{
    int status = 501;
    int rc;

    if (ev->type != SB_EV_REQUEST)
        return SB_RES_OK;

    vlogI("Received HTTP request to path[%s]", ev->path);
    vlogI("Return HTTP response: %s", feeds_url);

    rc = qrencode(feeds_url, qrcode_path);
    if (rc < 0)
        goto finally;

    rc = sb_send_header(ev->stream, "Content-Type", "image/png");
    if (rc < 0) {
        vlogE("Sending HTTP header failed.");
        goto finally;
    }

    rc = sb_send_file(ev->stream, qrcode_path);
    if (rc < 0) {
        vlogE("Sending HTTP body failed.");
        goto finally;
    }

    status = 200;

finally:
    sb_send_status(ev->stream, status, status == 200 ? "OK" : "Internal Server Error");

    remove(qrcode_path);

    return SB_RES_OK;
}

static
void *http_server_routine(void *arg)
{
    sb_Server *http = arg;

    while (http_is_running)
        sb_poll_server(http, 10);

    sb_close_server(http);

    return NULL;
}

static
int start_binding_svc(FeedsConfig *fc)
{
    sb_Options opts = {
        .host    = fc->http_ip,
        .port    = fc->http_port,
        .udata   = NULL,
        .handler = hdl_http_req
    };
    sb_Server *http;
    pthread_t tid;
    int rc;

    http = sb_new_server(&opts);
    if (!http) {
        vlogE("Creating HTTP server instance failed.");
        return -1;
    }

    http_is_running = true;
    rc = pthread_create(&tid, NULL, http_server_routine, http);
    if (rc) {
        vlogE("HTTP server failed to start");
        http_is_running = false;
        sb_close_server(http);
        return -1;
    }

    vlogI("HTTP server started.");

    pthread_detach(tid);

    return 0;
}

static inline
void stop_binding_svc()
{
    http_is_running = false;
}

static
void oinfo_clear()
{
    if (feeds_owner_info.did)
        free(feeds_owner_info.did);

    if (feeds_owner_info.name)
        free(feeds_owner_info.name);

    if (feeds_owner_info.email)
        free(feeds_owner_info.email);

    memset(&feeds_owner_info, 0, sizeof(feeds_owner_info));
}

void did_deinit()
{
    stop_binding_svc();

    if (feeds_vc)
        Credential_Destroy(feeds_vc);

    if (feeds_doc)
        DIDDocument_Destroy(feeds_doc);

    if (feeds_storepass)
        free(feeds_storepass);

    if (feeds_didstore)
        DIDStore_Close(feeds_didstore);

    oinfo_clear();
}

static
int oinfo_init(const UserInfo *ui)
{
    feeds_owner_info.uid = ui->uid;
    feeds_owner_info.did = strdup(ui->did);
    feeds_owner_info.name = strdup(ui->name);
    feeds_owner_info.email = strdup(ui->email);

    if (!feeds_owner_info.did || !feeds_owner_info.name || !feeds_owner_info.email) {
        oinfo_clear();
        return -1;
    }

    return 0;
}

int oinfo_upd(const UserInfo *ui)
{
    char *name = strdup(ui->name);
    char *email = strdup(ui->email);

    if (!name) {
        vlogE("OOM");
        return -1;
    }

    if (!email) {
        vlogE("OOM");
        free(name);
        return -1;
    }

    free(feeds_owner_info.name);
    free(feeds_owner_info.email);

    feeds_owner_info.name = name;
    feeds_owner_info.email = email;

    return 0;
}

static
int load_feeds_doc(DID *did, void *context)
{
    (void)context;

    if (!did)
        return -1;

    feeds_doc = DIDStore_LoadDID(feeds_didstore, did);
    return -1;
}

static
void *resolve_did_routine(void *arg)
{
    DIDDocument *doc;

    (void)arg;

    while (!(doc = DID_Resolve(feeds_did, false))) ;

    state_set(DID_RESOLVED);
    vlogI("Feeds DID is resolved, ready to serve.");

    DIDDocument_Destroy(doc);
    return NULL;
}

static
void resolve_did()
{
    pthread_t tid;
    int rc;

    rc = pthread_create(&tid, NULL, resolve_did_routine, NULL);
    if (rc) {
        vlogE("Failed to start DID resolving routine");
        return;
    }

    vlogI("Start resolving DID");

    pthread_detach(tid);
}

int did_init(FeedsConfig *cfg)
{
    static DIDAdapter adapter = {
        .createIdTransaction = create_id_tsx
    };
    uint8_t nonce[NONCE_BYTES];
    DIDURL *vc_url = NULL;
    UserInfo *ui = NULL;
    int rc;

    sprintf(qrcode_path, "%s/qrcode.png", cfg->data_dir);

    crypto_random_nonce(nonce);
    crypto_nonce_to_str(nonce, nonce_str, sizeof(nonce_str));
    gen_feeds_url();

    feeds_storepass = strdup(cfg->didstore_passwd);
    if (!feeds_storepass) {
        vlogE("OOM.");
        goto failure;
    }

    feeds_didstore = DIDStore_Open(cfg->didstore_dir, &adapter);
    if (!feeds_didstore) {
        vlogE("Opening DID store failed: %s", DIDError_GetMessage());
        goto failure;
    }

    rc = db_get_owner(&ui);
    if (rc < 0) {
        vlogE("Getting owner from database failed.");
        goto failure;
    }

    if (!ui) {
        rc = start_binding_svc(cfg);
        goto finally;
    }

    rc = oinfo_init(ui);
    if (rc < 0) {
        vlogE("Initializing feeds owner info failed.");
        goto failure;
    }

    state_set(OWNER_DECLED);
    vlogI("Owner declared: [%s]", feeds_owner_info.did);

    if (!DIDStore_ContainsPrivateIdentity(feeds_didstore)) {
        rc = start_binding_svc(cfg);
        goto finally;
    }

    DIDStore_ListDIDs(feeds_didstore, DID_FILTER_HAS_PRIVATEKEY, load_feeds_doc, NULL);
    if (!feeds_doc) {
        rc = start_binding_svc(cfg);
        goto finally;
    }

    feeds_did = DIDDocument_GetSubject(feeds_doc);
    DID_ToString(feeds_did, feeds_did_str, sizeof(feeds_did_str));
    feeeds_auth_key_url = DIDDocument_GetDefaultPublicKey(feeds_doc);

    state_set(DID_IMPED);
    vlogI("DID imported: [%s]", feeds_did_str);

    vc_url = DIDURL_NewByDid(feeds_did, VC_FRAG);
    if (!vc_url) {
        vlogE("Getting VC URL failed: %s", DIDError_GetMessage());
        goto failure;
    }

    feeds_vc = DIDStore_LoadCredential(feeds_didstore, feeds_did, vc_url);
    if (!feeds_vc) {
        rc = start_binding_svc(cfg);
        goto finally;
    }

    state_set(VC_ISSED);
    vlogI("Credential issued.");
    resolve_did();

    rc = start_binding_svc(cfg);
    goto finally;

failure:
    rc = -1;

finally:
    if (rc < 0)
        did_deinit();
    if (vc_url)
        DIDURL_Destroy(vc_url);
    deref(ui);

    return rc;
}

bool did_is_ready()
{
    return state_is_equal(OWNER_DECLED | DID_IMPED | VC_ISSED | DID_RESOLVED);
}

static
char *gen_tsx_payload()
{
    DIDStore_PublishDID(feeds_didstore, feeds_storepass, feeds_did,
                        feeeds_auth_key_url, true);
    return payload_buf;
}

static
void clear_tsx_payload()
{
    if (payload_buf) {
        free(payload_buf);
        payload_buf = NULL;
    }
}

void hdl_decl_owner_req(ElaCarrier *c, const char *from, Req *base)
{
    DeclOwnerReq *req = (DeclOwnerReq *)base;
    Marshalled *resp_marshal = NULL;
    UserInfo ui = {
        .uid   = OWNER_USER_ID,
        .did   = req->params.owner_did,
        .name  = "NA",
        .email = "NA"
    };

    vlogD("Received declare_owner request from [%s]: "
          "{nonce: %s, owner_did: %s}", from, req->params.nonce, req->params.owner_did);

    if (!state_is_set(OWNER_DECLED)) {
        if (oinfo_init(&ui) < 0) {
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }

        if (db_upsert_user(&ui, &ui.uid) < 0) {
            oinfo_clear();
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }

        vlogI("Owner declared: [%s].", req->params.owner_did);
        state_set(OWNER_DECLED);

        {
            DeclOwnerResp resp = {
                .tsx_id = req->tsx_id,
                .result = {
                    .phase       = "owner_declared",
                    .did         = NULL,
                    .tsx_payload = NULL
                }
            };
            resp_marshal = rpc_marshal_decl_owner_resp(&resp);
            vlogD("Sending declare_owner response to [%s]: "
                  "{phase: %s, did: nil, transaction_payload: nil}", from, resp.result.phase);
        }
    } else if (strcmp(req->params.owner_did, feeds_owner_info.did)) {
        vlogE("Owner mismatch. Expected: [%s], actual: [%s]", feeds_owner_info.did, req->params.owner_did);
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_NOT_AUTHORIZED
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
    } else if (!state_is_set(DID_IMPED)) {
        DeclOwnerResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .phase       = "owner_declared",
                .did         = NULL,
                .tsx_payload = NULL
            }
        };
        resp_marshal = rpc_marshal_decl_owner_resp(&resp);
        vlogD("Sending declare_owner response to [%s]: "
              "{phase: %s, did: nil, transaction_payload: nil}", from, resp.result.phase);
    } else if (!state_is_set(VC_ISSED)) {
        DeclOwnerResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .phase       = "did_imported",
                .did         = feeds_did_str,
                .tsx_payload = gen_tsx_payload()
            }
        };
        resp_marshal = rpc_marshal_decl_owner_resp(&resp);
        vlogD("Sending declare_owner response to [%s]: "
              "{phase: %s, did: %s, transaction_payload: %s}",
              from, resp.result.phase, resp.result.did, resp.result.tsx_payload);
        clear_tsx_payload();
    } else {
        DeclOwnerResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .phase       = "credential_issued",
                .did         = NULL,
                .tsx_payload = NULL
            }
        };
        resp_marshal = rpc_marshal_decl_owner_resp(&resp);
        vlogD("Sending declare_owner response to [%s]: "
              "{phase: %s, did: nil, transaction_payload: nil}", from, resp.result.phase);
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
}

void hdl_imp_did_req(ElaCarrier *c, const char *from, Req *base)
{
    ImpDIDReq *req = (ImpDIDReq *)base;
    Marshalled *resp_marshal = NULL;
    char *mnemo_gen = NULL;
    int rc;

    vlogD("Received import_did request from [%s]: "
          "{mnemonic: %s, passphrase: %s, index: %" PRIu64 "}",
          from, req->params.mnemo ? req->params.mnemo : "nil",
          req->params.passphrase ? req->params.passphrase : "nil",
          req->params.idx);

    if (!state_is_equal(OWNER_DECLED)) {
        vlogE("Importing DID in a wrong state. Current state: %s", state_str());
        return;
    }

    if (!req->params.mnemo) {
        mnemo_gen = (char *)Mnemonic_Generate("english");
        if (!mnemo_gen) {
            vlogE("Generating mnemonic failed: %s", DIDError_GetMessage());
            ErrResp resp = {
                .tsx_id = req->tsx_id,
                .ec     = ERR_INTERNAL_ERROR
            };
            resp_marshal = rpc_marshal_err_resp(&resp);
            goto finally;
        }
    }

    rc = DIDStore_InitPrivateIdentity(feeds_didstore, feeds_storepass,
                                      req->params.mnemo ? req->params.mnemo : mnemo_gen,
                                      req->params.passphrase ? req->params.passphrase : "",
                                      "english", true);
    if (rc < 0) {
        vlogE("Initializing DID store private identity failed: %s", DIDError_GetMessage());
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    feeds_doc = DIDStore_NewDIDByIndex(feeds_didstore, feeds_storepass, req->params.idx, NULL);
    if (!feeds_doc) {
        vlogE("Newing DID in DID store failed: %s", DIDError_GetMessage());
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    feeds_did = DIDDocument_GetSubject(feeds_doc);
    DID_ToString(feeds_did, feeds_did_str, sizeof(feeds_did_str));
    feeeds_auth_key_url = DIDDocument_GetDefaultPublicKey(feeds_doc);

    vlogI("DID imported: [%s].", feeds_did_str);
    state_set(DID_IMPED);

    {
        ImpDIDResp resp = {
            .tsx_id = req->tsx_id,
            .result = {
                .did         = feeds_did_str,
                .tsx_payload = gen_tsx_payload()
            }
        };
        resp_marshal = rpc_marshal_imp_did_resp(&resp);
        vlogD("Sending import_did response to [%s]: {did: %s, transaction_payload: %s}",
              from, resp.result.did, resp.result.tsx_payload);
        clear_tsx_payload();
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (mnemo_gen)
        Mnemonic_Free(mnemo_gen);
}

static DIDDocument *local_resolver(DID *did)
{
    return DID_Equals(did, feeds_did) ? DIDStore_LoadDID(feeds_didstore, feeds_did) : NULL;
}

static inline
bool Credential_IsValid_ResolveSubLocally(Credential *cred)
{
    bool is_valid;

    DIDBackend_SetLocalResolveHandle(local_resolver);
    is_valid = Credential_IsValid(cred);
    if (!is_valid)
        vlogE("Resolving VC locally failed: %s", DIDError_GetMessage());
    DIDBackend_SetLocalResolveHandle(NULL);

    return is_valid;
}

void hdl_iss_vc_req(ElaCarrier *c, const char *from, Req *base)
{
    IssVCReq *req = (IssVCReq *)base;
    Marshalled *resp_marshal = NULL;
    char iss_did[ELA_MAX_DID_LEN];
    DIDURL *vc_url = NULL;
    Credential *vc = NULL;

    vlogD("Received issue_credential request from [%s]: "
          "{credential: %s}", from, req->params.vc);

    if (!state_is_equal(OWNER_DECLED | DID_IMPED)) {
        vlogE("Issuing credential in a wrong state. Current state: %s", state_str());
        return;
    }

    vc_url = DIDURL_NewByDid(feeds_did, VC_FRAG);
    if (!vc_url) {
        vlogE("Getting VC URL failed: %s", DIDError_GetMessage());
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INTERNAL_ERROR
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    vc = Credential_FromJson(req->params.vc, feeds_did);
    if (!vc) {
        vlogE("Unmarshalling credential failed: %s", DIDError_GetMessage());
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!Credential_IsValid_ResolveSubLocally(vc)) {
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!DIDURL_Equals(Credential_GetId(vc), vc_url)) {
        char vc_url_str[ELA_MAX_DIDURL_LEN];
        char vc_id[ELA_MAX_DIDURL_LEN];

        vlogE("Credential ID mismatch. Expected: [%s], actual: [%s]",
              DIDURL_ToString(vc_url, vc_url_str, sizeof(vc_url_str), true),
              DIDURL_ToString(Credential_GetId(vc), vc_id, sizeof(vc_id), true));

        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (!DID_Equals(Credential_GetOwner(vc), feeds_did)) {
        char vc_owner_did[ELA_MAX_DID_LEN];

        vlogE("Credential owner mismatch. Expected: [%s], actual: [%s]",
              feeds_did_str, DID_ToString(Credential_GetOwner(vc), vc_owner_did, sizeof(vc_owner_did)));

        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (strcmp(DID_ToString(Credential_GetIssuer(vc), iss_did, sizeof(iss_did)), feeds_owner_info.did)) {
        vlogE("Credential issuer mismatch. Expected: [%s], actual: [%s]", feeds_owner_info.did, iss_did);
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    if (DIDStore_StoreCredential(feeds_didstore, vc) < 0) {
        vlogE("Storing credential failed: %s", DIDError_GetMessage());
        ErrResp resp = {
            .tsx_id = req->tsx_id,
            .ec     = ERR_INVALID_PARAMS
        };
        resp_marshal = rpc_marshal_err_resp(&resp);
        goto finally;
    }

    feeds_vc = vc;
    vc = NULL;

    state_set(VC_ISSED);
    vlogI("Credential issued.");
    resolve_did();

    {
        IssVCResp resp = {
            .tsx_id = req->tsx_id,
        };
        resp_marshal = rpc_marshal_iss_vc_resp(&resp);
        vlogD("Sending issue_credential response.");
    }

finally:
    if (resp_marshal) {
        ela_send_friend_message(c, from, resp_marshal->data, resp_marshal->sz, NULL);
        deref(resp_marshal);
    }
    if (vc)
        Credential_Destroy(vc);
    if (vc_url)
        DIDURL_Destroy(vc_url);
}
