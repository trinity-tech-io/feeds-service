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

#include <mutex>
#include <carrier.h>
#include <crystal.h>
#include <inttypes.h>

#undef static_assert // fix double conflict between crystal and std functional
#include <CommandHandler.hpp>
#include "msgq.h"

#define TAG_MSG "[Feedsd.Msg ]: "

typedef struct {
    linked_hash_entry_t he;
    char peer[CARRIER_MAX_ID_LEN + 1];
    linked_list_t *q;
    bool depr;
} MsgQ;

typedef struct {
    linked_list_entry_t le;
    Marshalled *data;
} Msg;

extern Carrier *carrier;

static linked_hashtable_t *msgqs;
static std::recursive_mutex mutex;

static inline
MsgQ *msgq_get(const char *peer)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    return (MsgQ*)linked_hashtable_get(msgqs, peer, strlen(peer));
}

static inline
MsgQ *msgq_put(MsgQ *q)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    return (MsgQ*)linked_hashtable_put(msgqs, &q->he);
}

static inline
MsgQ *msgq_rm(const char *peer)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    return (MsgQ*)linked_hashtable_remove(msgqs, peer, strlen(peer));
}

static inline
Msg *msgq_pop_head(MsgQ *q)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    return (Msg*)(linked_list_is_empty(q->q) ? NULL : linked_list_pop_head(q->q));
}

static inline
void msgq_push_tail(MsgQ *q, Msg *m)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    linked_list_push_tail(q->q, &m->le);
}

static
void msg_dtor(void *obj)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    Msg *msg = (Msg*)obj;

    deref(msg->data);
}

static
Msg *msg_create(Marshalled *msg)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    Msg *m = (Msg*)rc_zalloc(sizeof(Msg), msg_dtor);
    if (!m)
        return NULL;

    m->le.data = m;
    m->data    = (Marshalled*)ref(msg);

    return m;
}

static
void msgq_dtor(void *obj)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    MsgQ *q = (MsgQ*)obj;

    deref(q->q);
}

static
MsgQ *msgq_create(const char *to)
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    MsgQ *q = (MsgQ*)rc_zalloc(sizeof(MsgQ), msgq_dtor);
    if (!q)
        return NULL;

    q->q = linked_list_create(0, NULL);
    if (!q->q) {
        deref(q);
        return NULL;
    }

    strcpy(q->peer, to);
    q->he.data   = q;
    q->he.key    = q->peer;
    q->he.keylen = strlen(q->peer);

    return q;
}

static
void on_msg_receipt(uint32_t msgid, CarrierReceiptState state, void *context)
{
    MsgQ *q = (MsgQ*)context;
    Msg *m = NULL;
    std::vector<uint8_t> data;

    (void)msgid;
    (void)state;

    vlogD(TAG_MSG "Message [%" PRIi64 "] to [%s] receipt status: %s", msgid, q->peer,
          state == CarrierReceipt_ByFriend ? "received" :
                   state == CarrierReceipt_Offline ? "friend offline" : "error");

    if (q->depr) {
        vlogD(TAG_MSG "Message queue is deprecated.");
        goto finally;
    }

    if (!(m = msgq_pop_head(q))) {
        vlogD(TAG_MSG "Transport channel becomes idle.");
        deref(msgq_rm(q->peer));
        goto finally;
    }

    data = std::move(std::vector<uint8_t>{ reinterpret_cast<uint8_t*>(m->data->data),
                                           reinterpret_cast<uint8_t*>(m->data->data) + m->data->sz });
    std::ignore = trinity::CommandHandler::GetInstance()->send(q->peer, data, on_msg_receipt, ref(q));

finally:
    deref(q);
    deref(m);
}

int msgq_enq(const char *to, Marshalled *msg)
{
    MsgQ *q = NULL;
    Msg *m = NULL;
    int rc = -1;
    std::vector<uint8_t> data;

    q = msgq_get(to);
    if (q) {
        vlogD(TAG_MSG "Transport channel[%s] is busy, put in message queue.", to);

        m = msg_create(msg);
        if (!m) {
            vlogE(TAG_MSG "Creating message failed.");
            goto finally;
        }

        msgq_push_tail(q, m);
        rc = 0;
        goto finally;
    }

    q = msgq_create(to);
    if (!q) {
        vlogE(TAG_MSG "Creating message queue failed.");
        goto finally;
    }

    data = std::move(std::vector<uint8_t>{ reinterpret_cast<uint8_t*>(msg->data),
                                           reinterpret_cast<uint8_t*>(msg->data) + msg->sz });
    std::ignore = trinity::CommandHandler::GetInstance()->send(to, data, on_msg_receipt, ref(q));

    msgq_put(q);
    rc = 0;

finally:
    deref(q);
    deref(m);
    return rc;
}

void msgq_peer_offline(const char *peer)
{
    MsgQ *q = msgq_rm(peer);

    if (q) {
        vlogD(TAG_MSG "Set message queue[%s] deprecated.", q->peer);
        q->depr = true;
    }

    deref(q);
}

int msgq_init()
{
    msgqs = linked_hashtable_create(8, 0, NULL, NULL);
    if (!msgqs) {
        vlogE(TAG_MSG "Creating message queues failed");
        return -1;
    }

    vlogI(TAG_MSG "Message queue module initialized.");

    return 0;
}

void msgq_deinit()
{
    deref(msgqs);
}
