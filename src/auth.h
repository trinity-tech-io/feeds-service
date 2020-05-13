#ifndef __AUTH_H__
#define __AUTH_H__

#include <ela_carrier.h>

#include "obj.h"
#include "cfg.h"
#include "rpc.h"

int auth_init();
void auth_deinit();
void hdl_signin_req_chal_req(ElaCarrier *c, const char *from, Req *base);
void hdl_signin_conf_chal_req(ElaCarrier *c, const char *from, Req *base);
UserInfo *create_uinfo_from_access_token(const char *token_marshal);
void auth_expire_login();

#endif // __AUTH_H__
