#ifndef __DID_H__
#define __DID_H__

#include "cfg.h"
#include "obj.h"
#include "rpc.h"

extern DIDURL *feeeds_auth_key_url;
extern UserInfo feeds_owner_info;
extern DIDDocument *feeds_doc;
extern char *feeds_storepass;
extern Credential *feeds_vc;

int did_init(FeedsConfig *cfg);
void did_deinit();
bool did_is_ready();
int oinfo_upd(const UserInfo *ui);
void hdl_decl_owner_req(ElaCarrier *c, const char *from, Req *base);
void hdl_imp_did_req(ElaCarrier *c, const char *from, Req *base);
void hdl_iss_vc_req(ElaCarrier *c, const char *from, Req *base);

#endif //__DID_H__
