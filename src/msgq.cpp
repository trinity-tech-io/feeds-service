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

#include <ela_carrier.h>
#include <crystal.h>

#undef static_assert // fix double conflict between crystal and std functional
#include <CommandHandler.hpp>
#include "msgq.h"

typedef struct {
    hash_entry_t he;
    char peer[ELA_MAX_ID_LEN + 1];
    list_t *q;
    bool depr;
} MsgQ;

typedef struct {
    list_entry_t le;
    Marshalled *data;
} Msg;

extern ElaCarrier *carrier;

static hashtable_t *msgqs;

static inline
MsgQ *msgq_get(const char *peer)
{
    return (MsgQ*)hashtable_get(msgqs, peer, strlen(peer));
}

static inline
MsgQ *msgq_put(MsgQ *q)
{
    return (MsgQ*)hashtable_put(msgqs, &q->he);
}

static inline
MsgQ *msgq_rm(const char *peer)
{
    return (MsgQ*)hashtable_remove(msgqs, peer, strlen(peer));
}

static inline
Msg *msgq_pop_head(MsgQ *q)
{
    return (Msg*)(list_is_empty(q->q) ? NULL : list_pop_head(q->q));
}

static inline
void msgq_push_tail(MsgQ *q, Msg *m)
{
    list_push_tail(q->q, &m->le);
}

static
void msg_dtor(void *obj)
{
    Msg *msg = (Msg*)obj;

    deref(msg->data);
}

static
Msg *msg_create(Marshalled *msg)
{
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
    MsgQ *q = (MsgQ*)obj;

    deref(q->q);
}

static
MsgQ *msgq_create(const char *to)
{
    MsgQ *q = (MsgQ*)rc_zalloc(sizeof(MsgQ), msgq_dtor);
    if (!q)
        return NULL;

    q->q = list_create(0, NULL);
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
void on_msg_receipt(int64_t msgid, ElaReceiptState state, void *context)
{
    MsgQ *q = (MsgQ*)context;
    Msg *m = NULL;
    std::vector<uint8_t> data;

    (void)msgid;
    (void)state;

    vlogD("Message [%" PRIi64 "] to [%s] receipt status: %s", msgid, q->peer,
          state == ElaReceipt_ByFriend ? "received" :
                                         state == ElaReceipt_Offline ? "friend offline" : "error");

    if (q->depr) {
        vlogD("Message queue is deprecated.");
        goto finally;
    }

    if (!(m = msgq_pop_head(q))) {
        vlogD("Transport channel becomes idle.");
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
        vlogD("Transport channel[%s] is busy, put in message queue.", to);

        m = msg_create(msg);
        if (!m) {
            vlogE("Creating message failed.");
            goto finally;
        }

        msgq_push_tail(q, m);
        rc = 0;
        goto finally;
    }

    q = msgq_create(to);
    if (!q) {
        vlogE("Creating message queue failed.");
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
        vlogD("Set message queue[%s] deprecated.", q->peer);
        q->depr = true;
    }

    deref(q);
}

int msgq_init()
{
    msgqs = hashtable_create(8, 0, NULL, NULL);
    if (!msgqs) {
        vlogE("Creating message queues failed");
        return -1;
    }

    vlogI("Message queue module initialized.");

    return 0;
}

void msgq_deinit()
{
    deref(msgqs);
}
