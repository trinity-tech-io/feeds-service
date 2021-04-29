#include "LegacyMethod.hpp"

#include <crystal.h>
#include <CommandHandler.hpp>
#include <ErrCode.hpp>
#include <SafePtr.hpp>

extern "C" {
#define new fix_cpp_keyword_new
#include <auth.h>
#include <did.h>
#include <feeds.h>
#undef new
}

namespace trinity {

/* =========================================== */
/* === static variables initialize =========== */
/* =========================================== */
static struct {
    const char *method;
    void (*hdlr)(Carrier *c, const char *from, Req *base);
} method_hdlrs[] = {
    {"declare_owner"               , hdl_decl_owner_req         },
    {"import_did"                  , hdl_imp_did_req            },
    {"issue_credential"            , hdl_iss_vc_req             },
    {"update_credential"           , hdl_update_vc_req          },
    {"signin_request_challenge"    , hdl_signin_req_chal_req    },
    {"signin_confirm_challenge"    , hdl_signin_conf_chal_req   },
    {"create_channel"              , hdl_create_chan_req        },
    {"update_feedinfo"             , hdl_upd_chan_req           },
    {"publish_post"                , hdl_pub_post_req           },
    {"declare_post"                , hdl_declare_post_req       },
    {"notify_post"                 , hdl_notify_post_req        },
    {"edit_post"                   , hdl_edit_post_req          },
    {"delete_post"                 , hdl_del_post_req           },
    {"post_comment"                , hdl_post_cmt_req           },
    {"edit_comment"                , hdl_edit_cmt_req           },
    {"delete_comment"              , hdl_del_cmt_req            },
    {"block_comment"               , hdl_block_cmt_req          },
    {"unblock_comment"             , hdl_unblock_cmt_req        },
    {"post_like"                   , hdl_post_like_req          },
    {"post_unlike"                 , hdl_post_unlike_req        },
    {"get_my_channels"             , hdl_get_my_chans_req       },
    {"get_my_channels_metadata"    , hdl_get_my_chans_meta_req  },
    {"get_channels"                , hdl_get_chans_req          },
    {"get_channel_detail"          , hdl_get_chan_dtl_req       },
    {"get_subscribed_channels"     , hdl_get_sub_chans_req      },
    {"get_posts"                   , hdl_get_posts_req          },
    {"get_posts_likes_and_comments", hdl_get_posts_lac_req      },
    {"get_liked_posts"             , hdl_get_liked_posts_req    },
    {"get_comments"                , hdl_get_cmts_req           },
    {"get_comments_likes"          , hdl_get_cmts_likes_req     },
    {"get_statistics"              , hdl_get_stats_req          },
    {"subscribe_channel"           , hdl_sub_chan_req           },
    {"unsubscribe_channel"         , hdl_unsub_chan_req         },
    {"enable_notification"         , hdl_enbl_notif_req         },
    {"get_service_version"         , hdl_get_srv_ver_req        },
    {"report_illegal_comment"      , hdl_report_illegal_cmt_req },
    {"get_reported_comments"       , hdl_get_reported_cmts_req  },
};

/* =========================================== */
/* === static function implement ============= */
/* =========================================== */

/* =========================================== */
/* === class public function implement  ====== */
/* =========================================== */

/* =========================================== */
/* === class protected function implement  === */
/* =========================================== */
int LegacyMethod::onDispose(const std::string& from,
                            std::shared_ptr<Req> req,
                            std::shared_ptr<Resp>& resp)
{
    auto carrierHandler = CommandHandler::GetInstance()->getCarrierHandler();
    SAFE_GET_PTR(carrier, carrierHandler);

    for (int i = 0; i < sizeof(method_hdlrs) / sizeof(method_hdlrs[0]); ++i) {
        if (!strcmp(req->method, method_hdlrs[i].method)) {
            method_hdlrs[i].hdlr(carrier.get(), from.c_str(), req.get());
            return ErrCode::CompletelyFinishedNotify;
        }
    }

    return ErrCode::UnimplementedError;
}

/* =========================================== */
/* === class private function implement  ===== */
/* =========================================== */

} // namespace trinity
