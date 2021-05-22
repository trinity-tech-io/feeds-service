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
        AccessToken tk;
    } params;
} TkReq;

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
        AccessToken tk;
        char *vc;
    } params;
} UpdateVCReq;

typedef struct {
    uint64_t tsx_id;
} UpdateVCResp;

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
        char       *tipm;    //v2.0
        char       *proof;   //v2.0
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
        char       *name;
        char       *intro;
        void       *avatar;
        size_t      sz;
        char       *tipm;    //v2.0
        char       *proof;   //v2.0
    } params;
} UpdChanReq;

typedef struct {
    uint64_t tsx_id;
} UpdChanResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        void       *content;
        size_t      con_sz;
        void       *thumbnails;  //2.0
        size_t      thu_sz;  //2.0
        char       *hash_id;  //2.0
        char       *proof;  //2.0
        char       *origin_post_url;  //2.0
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
        void       *content;
        size_t      con_sz;
        size_t      thu_sz;  //2.0
        char       *hash_id;  //2.0
        char       *proof;  //2.0
        char       *origin_post_url;  //2.0
    } params;
} EditPostReq;

typedef struct {
    uint64_t tsx_id;
} EditPostResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
    } params;
} DelPostReq;

typedef struct {
    uint64_t tsx_id;
} DelPostResp;

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
        uint64_t    id;
        uint64_t    cmt_id;
        void       *content;
        size_t      sz;
    } params;
} EditCmtReq;

typedef struct {
    uint64_t tsx_id;
} EditCmtResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        uint64_t    id;
    } params;
} DelCmtReq;

typedef struct {
    uint64_t tsx_id;
} DelCmtResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        uint64_t    cmt_id;
    } params;
} BlockCmtReq;

typedef struct {
    uint64_t tsx_id;
} BlockCmtResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        uint64_t    cmt_id;
    } params;
} UnblockCmtReq;

typedef struct {
    uint64_t tsx_id;
} UnblockCmtResp;

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
        uint64_t    chan_id;
        QryCriteria qc;
    } params;
} GetPostsLACReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        cvector_vector_type(PostInfo *) pinfos;
    } result;
} GetPostsLACResp;

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
        uint64_t    chan_id;
        uint64_t    post_id;
        QryCriteria qc;
    } params;
} GetCmtsLikesReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        cvector_vector_type(CmtInfo *) cinfos;
    } result;
} GetCmtsLikesResp;

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
        uint64_t total_cs;
    } result;
} GetStatsResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    id;
        char       *proof;  //2.0
    } params;
} SubChanReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        ChanInfo *cinfo;
    } result;
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
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
    } params;
} GetSrvVerReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        char* version;
        int64_t version_code;
    } result;
} GetSrvVerResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
        uint64_t    cmt_id;
        char* reasons;
    } params;
} ReportIllegalCmtReq;

typedef struct {
    uint64_t tsx_id;
} ReportIllegalCmtResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        QryCriteria qc;
    } params;
} GetReportedCmtsReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        bool is_last;
        cvector_vector_type(ReportedCmtInfo *) rcinfos;
    } result;
} GetReportedCmtsResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        void       *content;
        size_t      sz;
        bool  with_notify;
    } params;
} DeclarePostReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        uint64_t id;
    } result;
} DeclarePostResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken tk;
        uint64_t    chan_id;
        uint64_t    post_id;
    } params;
} NotifyPostReq;

typedef struct {
    uint64_t tsx_id;
} NotifyPostResp;

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
        PostInfo *pinfo;
    } params;
} PostUpdNotif;

typedef struct {
    char *method;
    struct {
        CmtInfo *cinfo;
    } params;
} NewCmtNotif;

typedef struct {
    char *method;
    struct {
        CmtInfo *cinfo;
    } params;
} CmtUpdNotif;

typedef struct {
    char *method;
    struct {
        LikeInfo *li;
    } params;
} NewLikeNotif;

typedef struct {
    char *method;
    struct {
        uint64_t  chan_id;
        UserInfo *uinfo;
    } params;
} NewSubNotif;

typedef struct {
    char *method;
    struct {
        ChanInfo *cinfo;
    } params;
} ChanUpdNotif;

typedef struct {
    char *method;
    struct {
        uint64_t total_cs;
    } params;
} StatsChangedNotif;

typedef struct {
    char *method;
    struct {
        ReportedCmtInfo *li;
    } params;
} ReportCmtNotif;

typedef struct {
    uint64_t tsx_id;
    char     result[0];
} Resp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken  tk;
        char        *key;
        char        *algo;
        char        *checksum;
        void        *content;
        size_t       content_sz;
    } params;
} SetBinaryReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        char        *key;
    } result;
} SetBinaryResp;

typedef struct {
    char    *method;
    uint64_t tsx_id;
    struct {
        AccessToken  tk;
        char        *key;
    } params;
} GetBinaryReq;

typedef struct {
    uint64_t tsx_id;
    struct {
        char        *key;
        char        *algo;
        char        *checksum;
        void        *content;
        size_t       content_sz;
    } result;
} GetBinaryResp;

typedef struct {
    void  *data;
    size_t sz;
} Marshalled;

int rpc_unmarshal_req(const void *rpc, size_t len, Req **req);
Marshalled *rpc_marshal_err(uint64_t tsx_id, int64_t errcode, const char *errdesp);

Marshalled *rpc_marshal_new_post_notif(const NewPostNotif *notif);
Marshalled *rpc_marshal_post_upd_notif(const PostUpdNotif *notif);
Marshalled *rpc_marshal_new_cmt_notif(const NewCmtNotif *notif);
Marshalled *rpc_marshal_cmt_upd_notif(const CmtUpdNotif *notif);
Marshalled *rpc_marshal_new_like_notif(const NewLikeNotif *notif);
Marshalled *rpc_marshal_new_sub_notif(const NewSubNotif *notif);
Marshalled *rpc_marshal_chan_upd_notif(const ChanUpdNotif *notif);
Marshalled *rpc_marshal_stats_changed_notif(const StatsChangedNotif *notif);
Marshalled *rpc_marshal_report_cmt_notif(const ReportCmtNotif *notif);

Marshalled *rpc_marshal_resp(const char* method, const Resp *resp);
Marshalled *rpc_marshal_decl_owner_resp(const DeclOwnerResp *resp);
Marshalled *rpc_marshal_imp_did_resp(const ImpDIDResp *resp);
Marshalled *rpc_marshal_iss_vc_resp(const IssVCResp *resp);
Marshalled *rpc_marshal_update_vc_resp(const UpdateVCResp *resp);
Marshalled *rpc_marshal_signin_req_chal_resp(const SigninReqChalResp *resp);
Marshalled *rpc_marshal_signin_conf_chal_resp(const SigninConfChalResp *resp);
Marshalled *rpc_marshal_err_resp(const ErrResp *resp);
Marshalled *rpc_marshal_create_chan_resp(const CreateChanResp *resp);
Marshalled *rpc_marshal_upd_chan_resp(const UpdChanResp *resp);
Marshalled *rpc_marshal_pub_post_resp(const PubPostResp *resp);
Marshalled *rpc_marshal_declare_post_resp(const DeclarePostResp *resp);
Marshalled *rpc_marshal_notify_post_resp(const NotifyPostResp *resp);
Marshalled *rpc_marshal_edit_post_resp(const EditPostResp *resp);
Marshalled *rpc_marshal_del_post_resp(const DelPostResp *resp);
Marshalled *rpc_marshal_post_cmt_resp(const PostCmtResp *resp);
Marshalled *rpc_marshal_edit_cmt_resp(const EditCmtResp *resp);
Marshalled *rpc_marshal_del_cmt_resp(const DelCmtResp *resp);
Marshalled *rpc_marshal_block_cmt_resp(const BlockCmtResp *resp);
Marshalled *rpc_marshal_unblock_cmt_resp(const UnblockCmtResp *resp);
Marshalled *rpc_marshal_post_like_resp(const PostLikeResp *resp);
Marshalled *rpc_marshal_post_unlike_resp(const PostUnlikeResp *resp);
Marshalled *rpc_marshal_get_my_chans_resp(const GetMyChansResp *resp);
Marshalled *rpc_marshal_get_my_chans_meta_resp(const GetMyChansMetaResp *resp);
Marshalled *rpc_marshal_get_chans_resp(const GetChansResp *resp);
Marshalled *rpc_marshal_get_chan_dtl_resp(const GetChanDtlResp *resp);
Marshalled *rpc_marshal_get_sub_chans_resp(const GetSubChansResp *resp);
Marshalled *rpc_marshal_get_posts_resp(const GetPostsResp *resp);
Marshalled *rpc_marshal_get_posts_lac_resp(const GetPostsLACResp *resp);
Marshalled *rpc_marshal_get_liked_posts_resp(const GetLikedPostsResp *resp);
Marshalled *rpc_marshal_get_cmts_resp(const GetCmtsResp *resp);
Marshalled *rpc_marshal_get_cmts_likes_resp(const GetCmtsLikesResp *resp);
Marshalled *rpc_marshal_get_stats_resp(const GetStatsResp *resp);
Marshalled *rpc_marshal_sub_chan_resp(const SubChanResp *resp);
Marshalled *rpc_marshal_unsub_chan_resp(const UnsubChanResp *resp);
Marshalled *rpc_marshal_enbl_notif_resp(const EnblNotifResp *resp);
Marshalled *rpc_marshal_get_srv_ver_resp(const GetSrvVerResp *resp);
Marshalled *rpc_marshal_report_illegal_cmt_resp(const ReportIllegalCmtResp *resp);
Marshalled *rpc_marshal_get_reported_cmts_resp(const GetReportedCmtsResp *resp);
int get_rpc_version(void);
#endif //__RPC_H__
