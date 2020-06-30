#ifndef __MSGQ_H__
#define __MSGQ_H__

#include "rpc.h"

int msgq_init();
void msgq_deinit();
int msgq_enq(const char *to, Marshalled *msg);
void msgq_peer_offline(const char *peer);

#endif //__MSGQ_H__
