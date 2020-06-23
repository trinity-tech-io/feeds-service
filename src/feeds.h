#ifndef __FEEDS_H__
#define __FEEDS_H__

#include <stddef.h>

#include <ela_carrier.h>

#include "cfg.h"
#include "rpc.h"

int feeds_init(FeedsConfig *cfg);
void feeds_deinit();
void feeds_deactivate_suber(const char *node_id);
void hdl_create_chan_req(ElaCarrier *c, const char *from, Req *base);
void hdl_pub_post_req(ElaCarrier *c, const char *from, Req *base);
void hdl_post_cmt_req(ElaCarrier *c, const char *from, Req *base);
void hdl_post_like_req(ElaCarrier *c, const char *from, Req *base);
void hdl_post_unlike_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_my_chans_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_my_chans_meta_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_chans_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_chan_dtl_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_sub_chans_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_posts_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_liked_posts_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_cmts_req(ElaCarrier *c, const char *from, Req *base);
void hdl_get_stats_req(ElaCarrier *c, const char *from, Req *base);
void hdl_sub_chan_req(ElaCarrier *c, const char *from, Req *base);
void hdl_unsub_chan_req(ElaCarrier *c, const char *from, Req *base);
void hdl_enbl_notif_req(ElaCarrier *c, const char *from, Req *base);

#endif //__FEEDS_H__
