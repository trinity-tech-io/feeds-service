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

#ifndef __RPC_H__
#define __RPC_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "cvector.h"
#include "obj.h"

typedef char *AccessToken;

typedef struct {
    uint64_t tsx_id;
    int64_t  ec;
} ErrResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    char     params[0];
} Req;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        char *nonce;
        char *owner_did;
    } params;
} DeclOwnerReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        char *phase;
        char *did;
        char *tsx_payload;
    } result;
} DeclOwnerResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        char    *mnemo;
        char    *passphrase;
        uint64_t idx;
    } params;
} ImpDIDReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        char *did;
        char *tsx_payload;
    } result;
} ImpDIDResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        char *vc;
    } params;
} IssVCReq;

typedef struct {
    uint64_t tsx_id;
} IssVCResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        char *iss;
        bool  vc_req;
    } params;
} SigninReqChalReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool  vc_req;
        char *jws;
        char *vc;
    } result;
} SigninReqChalResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        char *jws;
        char *vc;
    } params;
} SigninConfChalReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    exp;
    } result;
} SigninConfChalResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        char       *name;
        char       *intro;
        void       *avatar;
        size_t      sz;
    } params;
} CreateChanReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        uint64_t id;
    } result;
} CreateChanResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        void       *content;
        size_t      sz;
    } params;
} PubPostReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        uint64_t id;
    } result;
} PubPostResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        uint64_t    cmt_id;
        void       *content;
        size_t      sz;
    } params;
} PostCmtReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        uint64_t id;
    } result;
} PostCmtResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        uint64_t    cmt_id;
    } params;
} PostLikeReq;

typedef struct {
    uint64_t tsx_id;
} PostLikeResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        uint64_t    cmt_id;
    } params;
} PostUnlikeReq;

typedef struct {
    uint64_t tsx_id;
} PostUnlikeResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        QryCriteria qc;
    } params;
} GetMyChansReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        cvector_vector_type(ChanInfo *) cinfos;
    } result;
} GetMyChansResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        QryCriteria qc;
    } params;
} GetMyChansMetaReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        cvector_vector_type(ChanInfo *) cinfos;
    } result;
} GetMyChansMetaResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        QryCriteria qc;
    } params;
} GetChansReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        cvector_vector_type(ChanInfo *) cinfos;
    } result;
} GetChansResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    id;
    } params;
} GetChanDtlReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        ChanInfo *cinfo;
    } result;
} GetChanDtlResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        QryCriteria qc;
    } params;
} GetSubChansReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        cvector_vector_type(ChanInfo *) cinfos;
    } result;
} GetSubChansResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        QryCriteria qc;
    } params;
} GetPostsReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        cvector_vector_type(PostInfo *) pinfos;
    } result;
} GetPostsResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        QryCriteria qc;
    } params;
} GetLikedPostsReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        cvector_vector_type(PostInfo *) pinfos;
    } result;
} GetLikedPostsResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        QryCriteria qc;
    } params;
} GetCmtsReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        cvector_vector_type(CmtInfo *) cinfos;
    } result;
} GetCmtsResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
    } params;
} GetStatsReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        char    *did;
        uint64_t conn_cs;
    } result;
} GetStatsResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    id;
    } params;
} SubChanReq;

typedef struct {
    uint64_t tsx_id;
} SubChanResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    id;
    } params;
} UnsubChanReq;

typedef struct {
    uint64_t tsx_id;
} UnsubChanResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
    } params;
} EnblNotifReq;

typedef struct {
    uint64_t tsx_id;
} EnblNotifResp;

typedef struct {
    char *method;
    char params[0];
} Notif;

typedef struct {
    char *method;
    struct {
        PostInfo *pinfo;
    } params;
} NewPostNotif;

typedef struct {
    char *method;
    struct {
        CmtInfo *cinfo;
    } params;
} NewCmtNotif;

typedef struct {
    char *method;
    struct {
        LikeInfo *li;
    } params;
} NewLikeNotif;

typedef struct {
    char *method;
    struct {
        uint64_t chan_id;
        UserInfo *uinfo;
    } params;
} NewSubNotif;

typedef struct {
    void  *data;
    size_t sz;
} Marshalled;

int rpc_unmarshal_req(const void *rpc, size_t len, Req **req);
int rpc_unmarshal_notif_or_resp_id(const void *rpc, size_t len, Notif **notif, uint64_t *id);
int rpc_unmarshal_decl_owner_resp(DeclOwnerResp **resp, ErrResp **err);
int rpc_unmarshal_imp_did_resp(ImpDIDResp **resp, ErrResp **err);
int rpc_unmarshal_iss_vc_resp(IssVCResp **resp, ErrResp **err);
int rpc_unmarshal_signin_req_chal_resp(SigninReqChalResp **resp, ErrResp **err);
int rpc_unmarshal_signin_conf_chal_resp(SigninConfChalResp **resp, ErrResp **err);
int rpc_unmarshal_create_chan_resp(CreateChanResp **resp, ErrResp **err);
int rpc_unmarshal_pub_post_resp(PubPostResp **resp, ErrResp **err);
int rpc_unmarshal_post_cmt_resp(PostCmtResp **resp, ErrResp **err);
int rpc_unmarshal_post_like_resp(PostLikeResp **resp, ErrResp **err);
int rpc_unmarshal_post_unlike_resp(PostUnlikeResp **resp, ErrResp **err);
int rpc_unmarshal_get_my_chans_resp(GetMyChansResp **resp, ErrResp **err);
int rpc_unmarshal_get_my_chans_meta_resp(GetMyChansMetaResp **resp, ErrResp **err);
int rpc_unmarshal_get_chans_resp(GetChansResp **resp, ErrResp **err);
int rpc_unmarshal_get_chan_dtl_resp(GetChanDtlResp **resp, ErrResp **err);
int rpc_unmarshal_get_sub_chans_resp(GetSubChansResp **resp, ErrResp **err);
int rpc_unmarshal_get_posts_resp(GetPostsResp **resp, ErrResp **err);
int rpc_unmarshal_get_liked_posts_resp(GetLikedPostsResp **resp, ErrResp **err);
int rpc_unmarshal_get_cmts_resp(GetCmtsResp **resp, ErrResp **err);
int rpc_unmarshal_get_stats_resp(GetStatsResp **resp, ErrResp **err);
int rpc_unmarshal_sub_chan_resp(SubChanResp **resp, ErrResp **err);
int rpc_unmarshal_unsub_chan_resp(UnsubChanResp **resp, ErrResp **err);
int rpc_unmarshal_enbl_notif_resp(EnblNotifResp **resp, ErrResp **err);
Marshalled *rpc_marshal_decl_owner_req(const DeclOwnerReq *req);
Marshalled *rpc_marshal_decl_owner_resp(const DeclOwnerResp *resp);
Marshalled *rpc_marshal_imp_did_req(const ImpDIDReq *req);
Marshalled *rpc_marshal_imp_did_resp(const ImpDIDResp *resp);
Marshalled *rpc_marshal_iss_vc_req(const IssVCReq *req);
Marshalled *rpc_marshal_iss_vc_resp(const IssVCResp *resp);
Marshalled *rpc_marshal_signin_req_chal_req(const SigninReqChalReq *req);
Marshalled *rpc_marshal_signin_req_chal_resp(const SigninReqChalResp *resp);
Marshalled *rpc_marshal_signin_conf_chal_req(const SigninConfChalReq *req);
Marshalled *rpc_marshal_signin_conf_chal_resp(const SigninConfChalResp *resp);
Marshalled *rpc_marshal_new_post_notif(const NewPostNotif *notif);
Marshalled *rpc_marshal_new_cmt_notif(const NewCmtNotif *notif);
Marshalled *rpc_marshal_new_like_notif(const NewLikeNotif *notif);
Marshalled *rpc_marshal_new_sub_notif(const NewSubNotif *notif);
Marshalled *rpc_marshal_err_resp(const ErrResp *resp);
Marshalled *rpc_marshal_create_chan_req(const CreateChanReq *req);
Marshalled *rpc_marshal_create_chan_resp(const CreateChanResp *resp);
Marshalled *rpc_marshal_pub_post_req(const PubPostReq *req);
Marshalled *rpc_marshal_pub_post_resp(const PubPostResp *resp);
Marshalled *rpc_marshal_post_cmt_req(const PostCmtReq *req);
Marshalled *rpc_marshal_post_cmt_resp(const PostCmtResp *resp);
Marshalled *rpc_marshal_post_like_req(const PostLikeReq *req);
Marshalled *rpc_marshal_post_like_resp(const PostLikeResp *resp);
Marshalled *rpc_marshal_post_unlike_req(const PostUnlikeReq *req);
Marshalled *rpc_marshal_post_unlike_resp(const PostUnlikeResp *resp);
Marshalled *rpc_marshal_get_my_chans_req(const GetMyChansReq *req);
Marshalled *rpc_marshal_get_my_chans_resp(const GetMyChansResp *resp);
Marshalled *rpc_marshal_get_my_chans_meta_req(const GetMyChansMetaReq *req);
Marshalled *rpc_marshal_get_my_chans_meta_resp(const GetMyChansMetaResp *resp);
Marshalled *rpc_marshal_get_chans_req(const GetChansReq *req);
Marshalled *rpc_marshal_get_chans_resp(const GetChansResp *resp);
Marshalled *rpc_marshal_get_chan_dtl_req(const GetChanDtlReq *req);
Marshalled *rpc_marshal_get_chan_dtl_resp(const GetChanDtlResp *resp);
Marshalled *rpc_marshal_get_sub_chans_req(const GetSubChansReq *req);
Marshalled *rpc_marshal_get_sub_chans_resp(const GetSubChansResp *resp);
Marshalled *rpc_marshal_get_posts_req(const GetPostsReq *req);
Marshalled *rpc_marshal_get_posts_resp(const GetPostsResp *resp);
Marshalled *rpc_marshal_get_liked_posts_req(const GetLikedPostsReq *req);
Marshalled *rpc_marshal_get_liked_posts_resp(const GetLikedPostsResp *resp);
Marshalled *rpc_marshal_get_cmts_req(const GetCmtsReq *req);
Marshalled *rpc_marshal_get_cmts_resp(const GetCmtsResp *resp);
Marshalled *rpc_marshal_get_stats_req(const GetStatsReq *req);
Marshalled *rpc_marshal_get_stats_resp(const GetStatsResp *resp);
Marshalled *rpc_marshal_sub_chan_req(const SubChanReq *req);
Marshalled *rpc_marshal_sub_chan_resp(const SubChanResp *resp);
Marshalled *rpc_marshal_unsub_chan_req(const UnsubChanReq *req);
Marshalled *rpc_marshal_unsub_chan_resp(const UnsubChanResp *resp);
Marshalled *rpc_marshal_enbl_notif_req(const EnblNotifReq *req);
Marshalled *rpc_marshal_enbl_notif_resp(const EnblNotifResp *resp);

#endif //__RPC_H__
