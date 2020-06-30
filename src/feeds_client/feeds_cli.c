/*
 * Copyright (c) 2020 Elastos Foundation
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_PROCESS_H
#include <process.h>
#endif
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <ela_carrier.h>
#include <crystal.h>
#include <ela_did.h>
#include <ela_jwt.h>

#include "cfg.h"
#include "rpc.h"
#include "../err.h"
#include "feeds_client.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>

static enum {
    STANDBY,
    TYPING,
    GOTCMD,
    STOP
} state;

static FeedsClient *fc;
static struct termios term;

static
void console_prompt(void)
{
    fprintf(stdout, "# ");
    fflush(stdout);
}

static
void console(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

static
void carrier_log_printer(const char *fmt, va_list ap)
{
    // do nothing
}

static
int sys_coredump_set(bool enable)
{
    const struct rlimit rlim = {
        enable ? RLIM_INFINITY : 0,
        enable ? RLIM_INFINITY : 0
    };

    return setrlimit(RLIMIT_CORE, &rlim);
}
#endif

static
void usage(void)
{
    console("Elastos feeds client CLI.");
    console("Usage: feeds_cli [OPTION]...");
    console("");
    console("First run options:");
    console("  -c, --config=CONFIG_FILE  Set config file path.");
    console("      --udp-enabled=0|1     Enable UDP, override the option in config.");
    console("      --log-level=LEVEL     Log level(0-7), override the option in config.");
    console("      --log-file=FILE       Log file name, override the option in config.");
    console("      --data-dir=PATH       Data location, override the option in config.");
    console("");
    console("Debugging options:");
    console("      --debug               Wait for debugger attach after start.");
    console("");
}

#define CONFIG_NAME "carrier.conf"
static const char *default_config_files[] = {
    "./"CONFIG_NAME,
    "../etc/feeds/"CONFIG_NAME,
    "/usr/local/etc/feeds/"CONFIG_NAME,
    "/etc/feeds/"CONFIG_NAME,
    NULL
};

static
char *read_cmd(void)
{
    int ch = 0;
    char *p;
    static int  cmd_len = 0;
    static char cmd_line[1024];

    usleep(20000);
getchar:
    ch = fgetc(stdin);
    if (ch == EOF) {
        if (feof(stdin))
            state = STOP;
        return NULL;
    }

    if (isprint(ch)) {
        putchar(ch);

        cmd_line[cmd_len++] = ch;
        if (state == STANDBY)
            state = TYPING;
    } else if (ch == '\r' || ch == '\n') {
        putchar(ch);

        cmd_line[cmd_len] = 0;
        // Trim trailing spaces;
        for (p = cmd_line + cmd_len -1; p > cmd_line && isspace(*p); p--);
        *(++p) = 0;

        // Trim leading spaces;
        for (p = cmd_line; *p && isspace(*p); p++);

        cmd_len = 0;
        if (state == TYPING) {
            state = GOTCMD;
            return p;
        } else
            console_prompt();
    } else if (ch == 127) {
        if (state == TYPING)
            printf("\b \b");

        if (state == TYPING && !--cmd_len)
            state = STANDBY;
    }

    goto getchar;
}

static
void get_address(int argc, char *argv[])
{
    if (argc != 1) {
        console("Invalid command syntax.");
        return;
    }

    char addr[ELA_MAX_ADDRESS_LEN + 1] = {0};
    ela_get_address(feeds_client_get_carrier(fc), addr, sizeof(addr));
    console("Address: %s", addr);
}

static
void get_nodeid(int argc, char *argv[])
{
    if (argc != 1) {
        console("Invalid command syntax.");
        return;
    }

    char id[ELA_MAX_ID_LEN + 1] = {0};
    ela_get_nodeid(feeds_client_get_carrier(fc), id, sizeof(id));
    console("Node ID: %s", id);
}

static
void get_userid(int argc, char *argv[])
{
    if (argc != 1) {
        console("Invalid command syntax.");
        return;
    }

    char id[ELA_MAX_ID_LEN + 1] = {0};
    ela_get_userid(feeds_client_get_carrier(fc), id, sizeof(id));
    console("User ID: %s", id);
}

static
void friend_add(int argc, char *argv[])
{
    char node_id[ELA_MAX_ID_LEN + 1];
    int rc;

    if (argc != 3) {
        console("Invalid command syntax.");
        return;
    }

    if (!ela_address_is_valid(argv[1]))
        console("Invalid address.");

    rc = feeds_client_friend_add(fc, argv[1], argv[2]);
    if (rc < 0)
        console("Failed to add friend.");

    ela_get_id_by_address(argv[1], node_id, sizeof(node_id));
    console("Friend %s online.", node_id);
}

static
void friend_remove(int argc, char *argv[])
{
    int rc;

    if (argc != 2) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_friend_remove(fc, argv[1]);
    if (rc < 0)
        console("Failed to remove friend.");
}

static int first_friends_item = 1;
static const char *connection_name[] = {
    "online",
    "offline"
};

static
bool get_friends_callback(const ElaFriendInfo *friend_info, void *context)
{
    static int count;

    if (first_friends_item) {
        count = 0;
        console("Friends list:");
        console("  %-46s %8s %s", "ID", "Connection", "Label");
        console("  %-46s %8s %s", "----------------", "----------", "-----");
    }

    if (friend_info) {
        console("  %-46s %8s %s", friend_info->user_info.userid,
               connection_name[friend_info->status], friend_info->label);
        first_friends_item = 0;
        count++;
    } else {
        /* The list ended */
        console("  ----------------");
        console("Total %d friends.", count);

        first_friends_item = 1;
    }

    return true;
}

static
void list_friends(int argc, char *argv[])
{
    if (argc != 1) {
        console("Invalid command syntax.");
        return;
    }

    ela_get_friends(feeds_client_get_carrier(fc), get_friends_callback, NULL);
}

static
void kill_carrier(int argc, char *argv[])
{
    state = STOP;
}

static
void decl_owner(int argc, char **argv)
{
    DeclOwnerResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 2) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_decl_owner(fc, argv[1], &resp, &err);
    if (rc < 0) {
        console("Failed to declare owner.");
        goto finally;
    }

    if (err) {
        console("Failed to declare owner. code: %lld", err->ec);
        goto finally;
    }

    console("phase: %s", resp->result.phase);
    if (resp->result.did) {
        console("did: %s", resp->result.did);
        console("tsx: %s", resp->result.tsx_payload);
    }

finally:
    deref(resp);
    deref(err);
}

static
void imp_did(int argc, char **argv)
{
    ImpDIDResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 2) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_imp_did(fc, argv[1], &resp, &err);
    if (rc < 0) {
        console("Failed to import did.");
        goto finally;
    }

    if (err) {
        console("Failed to import DID. code: %lld", err->ec);
        goto finally;
    }

    console("did: %s", resp->result.did);
    console("tsx: %s", resp->result.tsx_payload);

finally:
    deref(resp);
    deref(err);
}

static
void iss_vc(int argc, char **argv)
{
    IssVCResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 2) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_iss_vc(fc, argv[1],
                             "did:elastos:ijUnD4KeRpeBUFmcEDCbhxMTJRzUYCQCZM", &resp, &err);
    if (rc < 0) {
        console("Failed to issue VC.");
        goto finally;
    }

    if (err) {
        console("Failed to iss VC. code: %lld", err->ec);
        goto finally;
    }

finally:
    deref(resp);
    deref(err);
}

static
void signin(int argc, char **argv)
{
    SigninReqChalResp *resp1 = NULL;
    SigninConfChalResp *resp2 = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 2) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_signin1(fc, argv[1], &resp1, &err);
    if (rc < 0) {
        console("Failed to signin1.");
        goto finally;
    }

    if (err) {
        console("Failed to signin1. code: %lld", err->ec);
        goto finally;
    }

    JWS *jws = JWTParser_Parse(resp1->result.jws);

    console("iss: %s", JWS_GetIssuer(jws));
    console("realm: %s", JWS_GetClaim(jws, "realm"));
    console("nonce: %s", JWS_GetClaim(jws, "nonce"));

    if (resp1->result.vc) {
        Credential *vc = Credential_FromJson(resp1->result.vc, NULL);
        console("vc(%s): %s", Credential_IsValid(vc) ? "valid" : "invalid", resp1->result.vc);
        Credential_Destroy(vc);
    }

    rc = feeds_client_signin2(fc, argv[1],
                              JWS_GetClaim(jws, "realm"),
                              JWS_GetClaim(jws, "nonce"), &resp2, &err);
    JWS_Destroy(jws);
    if (rc < 0) {
        console("Failed to signin2.");
        goto finally;
    }

    if (err) {
        console("Failed to signin2. code: %lld", err->ec);
        goto finally;
    }

    jws = JWTParser_Parse(resp2->result.tk);

    console("iss: %s", JWS_GetIssuer(jws));
    console("sub: %s", JWS_GetSubject(jws));
    console("name: %s", JWS_GetClaim(jws, "name"));
    console("email: %s", JWS_GetClaim(jws, "email"));
    console("exp: %d", JWS_GetExpiration(jws));
    JWS_Destroy(jws);

finally:
    deref(resp1);
    deref(resp2);
    deref(err);
}

static
void create_channel(int argc, char *argv[])
{
    CreateChanResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 5) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_create_channel(fc, argv[1], argv[2], argv[3],
                                     atoi(argv[4]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to create channel. code: %lld",
                err ? err->ec : -111);
        goto finally;
    }

    console("channel [%d] created.", resp->result.id);

finally:
    deref(resp);
    deref(err);
}

static
void publish_post(int argc, char **argv)
{
    PubPostResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 4) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_publish_post(fc, argv[1], atoi(argv[2]), atoi(argv[3]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to publish post. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    console("post [%llu] created.", resp->result.id);

finally:
    deref(resp);
    deref(err);
}

static
void post_comment(int argc, char **argv)
{
    PostCmtResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 6) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_post_comment(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                   atoi(argv[4]), atoi(argv[5]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to post comment. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    console("comment [%llu] created.", resp->result.id);

finally:
    deref(resp);
    deref(err);
}

static
void post_like(int argc, char **argv)
{
    PostLikeResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 5) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_post_like(fc, argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]),
                                &resp, &err);
    if (rc < 0 || err) {
        console("Failed to post like. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

finally:
    deref(resp);
    deref(err);
}

static
void post_unlike(int argc, char **argv)
{
    PostUnlikeResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 5) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_post_unlike(fc, argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]),
                                &resp, &err);
    if (rc < 0 || err) {
        console("Failed to post unlike. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

finally:
    deref(resp);
    deref(err);
}

static
void get_my_channels(int argc, char **argv)
{
    GetMyChansResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 6) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_my_channels(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                      atoi(argv[4]), atoi(argv[5]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get my channels. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    ChanInfo **i;
    cvector_foreach(resp->result.cinfos, i)
        console("id: %llu, name: %s, introduction: %s, subscribers: %llu, avatar_sz: %" PRIu64,
                (*i)->chan_id, (*i)->name, (*i)->intro, (*i)->subs, (*i)->len);

finally:
    deref(resp);
    deref(err);
}

static
void get_my_channels_metadata(int argc, char **argv)
{
    GetMyChansMetaResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 6) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_my_channels_metadata(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                               atoi(argv[4]), atoi(argv[5]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get my channels metadata. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    ChanInfo **i;
    cvector_foreach(resp->result.cinfos, i)
        console("id: %llu, subscribers: %llu", (*i)->chan_id, (*i)->subs);

finally:
    deref(resp);
    deref(err);
}

static
void get_channels(int argc, char **argv)
{
    GetChansResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 6) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_channels(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                   atoi(argv[4]), atoi(argv[5]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get channels. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    ChanInfo **i;
    cvector_foreach(resp->result.cinfos, i)
        console("id: %llu, name: %s, introduction: %s, owner_name: %s, owner_did: %s,"
                " subscribers: %llu, last_update: %llu, avatar_sz: %" PRIu64,
                (*i)->chan_id, (*i)->name, (*i)->intro, (*i)->owner->name,
                (*i)->owner->did, (*i)->subs, (*i)->upd_at, (*i)->len);

finally:
    deref(resp);
    deref(err);
}

static
void get_channel_detail(int argc, char **argv)
{
    GetChanDtlResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 3) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_channel_detail(fc, argv[1], atoi(argv[2]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get my channels. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    console("id: %llu, name: %s, introduction: %s, owner_name: %s, owner_did: %s,"
            " subscribers: %llu, last_update: %llu, avatar: %s",
            resp->result.cinfo->chan_id, resp->result.cinfo->name,
            resp->result.cinfo->intro, resp->result.cinfo->owner->name,
            resp->result.cinfo->owner->did, resp->result.cinfo->subs,
            resp->result.cinfo->upd_at, resp->result.cinfo->avatar);

finally:
    deref(resp);
    deref(err);
}

static
void get_subscribed_channels(int argc, char **argv)
{
    GetSubChansResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 6) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_subscribed_channels(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                              atoi(argv[4]), atoi(argv[5]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get subscribed channels. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    ChanInfo **i;
    cvector_foreach(resp->result.cinfos, i)
        console("id: %llu, name: %s, introduction: %s, owner_name: %s,"
                " owner_did: %s, subscribers: %llu, last_update: %llu, avatar_sz: %" PRIu64,
                (*i)->chan_id, (*i)->name, (*i)->intro, (*i)->owner->name, (*i)->owner->did,
                (*i)->subs, (*i)->upd_at, (*i)->len);

finally:
    deref(resp);
    deref(err);
}

static
void get_posts(int argc, char **argv)
{
    GetPostsResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 7) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_posts(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get posts. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    PostInfo **i;
    cvector_foreach(resp->result.pinfos, i)
        console("channel_id: %llu, id: %llu, content_sz: %" PRIu64 ", comments: %llu, likes: %llu, created_at: %llu",
                (*i)->chan_id, (*i)->post_id, (*i)->len, (*i)->cmts, (*i)->likes, (*i)->created_at);

finally:
    deref(resp);
    deref(err);
}

static
void get_liked_posts(int argc, char **argv)
{
    GetLikedPostsResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 6) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_liked_posts(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                      atoi(argv[4]), atoi(argv[5]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get liked posts. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    PostInfo **i;
    cvector_foreach(resp->result.pinfos, i)
        console("channel_id: %llu, id: %llu, content_sz: %" PRIu64 ", comments: %llu, likes: %llu, created_at: %llu",
                (*i)->chan_id, (*i)->post_id, (*i)->len, (*i)->cmts, (*i)->likes, (*i)->created_at);

finally:
    deref(resp);
    deref(err);
}

static
void get_comments(int argc, char **argv)
{
    GetCmtsResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 8) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_comments(fc, argv[1], atoi(argv[2]), atoi(argv[3]),
                                   atoi(argv[4]), atoi(argv[5]), atoi(argv[6]),
                                   atoi(argv[7]), &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get comments. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    CmtInfo **i;
    cvector_foreach(resp->result.cinfos, i)
        console("channel_id: %llu, post_id: %llu, id: %llu, comment_id:%llu, user_name: %s, content_sz: %" PRIu64 ", likes: %llu, created_at: %llu",
                (*i)->chan_id, (*i)->post_id, (*i)->cmt_id, (*i)->reply_to_cmt,
                (*i)->user.name, (*i)->len, (*i)->likes, (*i)->created_at);

finally:
    deref(resp);
    deref(err);
}

static
void get_statistics(int argc, char **argv)
{
    GetStatsResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 2) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_get_statistics(fc, argv[1], &resp, &err);
    if (rc < 0 || err) {
        console("Failed to get statistics. Code: %lld", err ? err->ec : -111);
        goto finally;
    }

    console("did: %s, conntecting clients: %llu", resp->result.did, resp->result.conn_cs);

finally:
    deref(resp);
    deref(err);
}

static
void subscribe_channel(int argc, char **argv)
{
    SubChanResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 3) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_subscribe_channel(fc, argv[1], atoi(argv[2]), &resp, &err);
    if (rc < 0 || err)
        console("Failed to subscribe channel. Code: %lld",
                err ? err->ec : -111);

    deref(resp);
    deref(err);
}

static
void unsubscribe_channel(int argc, char **argv)
{
    UnsubChanResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 3) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_unsubscribe_channel(fc, argv[1], atoi(argv[2]), &resp, &err);
    if (rc < 0 || err)
        console("Failed to unsubscribe channel. Code: %lld",
                err ? err->ec : -111);

    deref(resp);
    deref(err);
}

static
void enable_notification(int argc, char **argv)
{
    EnblNotifResp *resp = NULL;
    ErrResp *err = NULL;
    int rc;

    if (argc != 2) {
        console("Invalid command syntax.");
        return;
    }

    rc = feeds_client_wait_until_friend_connected(fc, argv[1]);
    if (rc < 0) {
        console("%s is not friend now.", argv[1]);
        return;
    }

    rc = feeds_client_enable_notification(fc, argv[1], &resp, &err);
    if (rc < 0 || err)
        console("Failed to enable notification. Code: %lld",
                err ? err->ec : -111);

    deref(resp);
    deref(err);
}

static void help(int argc, char *argv[]);
struct command {
    const char *cmd;
    void (*function)(int argc, char *argv[]);
    const char *help;
} commands[] = {
    { "help",              help,              "help - Display available command list. *OR* help [Command] - Display usage description for specific command." },

    { "address",                  get_address,              "address - Display own address." },
    { "nodeid",                   get_nodeid,               "nodeid - Display own node ID." },
    { "userid",                   get_userid,               "userid - Display own user ID." },

    { "fadd",                     friend_add,               "fadd [Address] [Message] - Add new friend." },
    { "fremove",                  friend_remove,            "fremove [User ID] - Remove friend." },
    { "friends",                  list_friends,             "friends - List all friends." },
    { "kill",                     kill_carrier,             "kill - Stop carrier." },

    { "decl_owner",               decl_owner,               "decl_owner [nodeid] - Bind owner." },
    { "imp_did",                  imp_did,                  "imp_did [nodeid] - Bind owner." },
    { "iss_vc",                   iss_vc,                   "iss_vc [nodeid]- Bind owner." },
    { "signin",                   signin,                   "signin [nodeid] - Bind owner." },
    { "create_channel",           create_channel,           "create_channel [nodeid] [channel] [intro] [avatar] - Create channel." },
    { "publish_post",             publish_post,             "publish_post [nodeid] [channel_id] [content] - Publish post." },
    { "post_comment",             post_comment,             "post_comment [nodeid] [channel_id] [post_id] [comment_id] [content] - Post comment." },
    { "post_like",                post_like,                "post_like [nodeid] [channel_id] [post_id] [comment_id] - Post like." },
    { "post_unlike",                post_unlike,                "post_unlike [nodeid] [channel_id] [post_id] [comment_id] - Post like." },
    { "get_my_channels",          get_my_channels,          "get_my_channels [nodeid] [by] [upper] [lower] [maxcnt] - Get owned channels." },
    { "get_my_channels_metadata", get_my_channels_metadata, "get_my_channels_metadata [nodeid] [by] [upper] [lower] [maxcnt] - Get owned channels metadata." },
    { "get_channels",             get_channels,             "get_channels [nodeid] [by] [upper] [lower] [maxcnt] - Get channels." },
    { "get_channel_detail",       get_channel_detail,       "get_channel_detail [nodeid] [channel_id] - Get channel detail." },
    { "get_subscribed_channels",  get_subscribed_channels,  "get_subscribed_channels [nodeid] [by] [upper] [lower] [maxcnt] - Get subscribed channels." },
    { "get_posts",                get_posts,                "get_posts [nodeid] [channel_id] [by] [upper] [lower] [maxcnt] - Get posts." },
    { "get_liked_posts",          get_liked_posts,          "get_liked_posts [nodeid] [by] [upper] [lower] [maxcnt] - Get liked posts." },
    { "get_comments",             get_comments,             "get_comments [nodeid] [channel_id] [post_id] [by] [upper] [lower] [maxcnt] - Get comments." },
    { "get_statistics",           get_statistics,           "get_statistics [nodeid] - Get statistics." },
    { "subscribe_channel",        subscribe_channel,        "subscribe_channel [nodeid] [channel_id] - Subscribe channel." },
    { "unsubscribe_channel",      unsubscribe_channel,      "unsubscribe_channel [nodeid] [channel_id] - Unsubscribe channel." },
    { "enable_notification",      enable_notification,      "enable_notification [nodeid] - Enable notification." },

    { NULL }
};

static
void help(int argc, char *argv[])
{
    char line[256] = {0};
    struct command *p;

    if (argc == 1) {
        console("available commands list:");

        for (p = commands; p->cmd; p++) {
            strcat(line, p->cmd);
            strcat(line, " ");
        }
        console("  %s", line);
        memset(line, 0, sizeof(line));
    } else {
        for (p = commands; p->cmd; p++) {
            if (strcmp(argv[1], p->cmd) == 0) {
                console("usage: %s", p->help);
                return;
            }
        }
        console("unknown command: %s", argv[1]);
    }
}

static
void do_cmd(char *line)
{
    char *args[64];
    int count = 0;
    char *p;
    int word = 0;

    for (p = line; *p != 0; p++) {
        if (isspace(*p)) {
            *p = 0;
            word = 0;
        } else {
            if (word == 0) {
                args[count] = p;
                count++;
            }
            word = 1;
        }
    }

    if (count > 0) {
        struct command *p;

        for (p = commands; p->cmd; p++) {
            if (strcmp(args[0], p->cmd) == 0) {
                p->function(count, args);
                return;
            }
        }
        console("unknown command: %s", args[0]);
    }
}

static
void reset_tcattr()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

int main(int argc, char *argv[])
{
    const char *config_file = NULL;
    int wait_for_attach = 0;
    struct termios term_tmp;
    FeedsCLIConfig opts;
    char *cmd;
    int rc;

    int opt;
    int idx;
    struct option options[] = {
        { "config",         required_argument,  NULL, 'c' },
        { "udp-enabled",    required_argument,  NULL, 1 },
        { "log-level",      required_argument,  NULL, 2 },
        { "log-file",       required_argument,  NULL, 3 },
        { "data-dir",       required_argument,  NULL, 4 },
        { "debug",          no_argument,        NULL, 5 },
        { "help",           no_argument,        NULL, 'h' },
        { NULL,             0,                  NULL, 0 }
    };

#ifdef HAVE_SYS_RESOURCE_H
    sys_coredump_set(true);
#endif

    memset(&opts, 0, sizeof(opts));

    while ((opt = getopt_long(argc, argv, "c:h?", options, &idx)) != -1) {
        switch (opt) {
        case 'c':
            config_file = optarg;
            break;

        case 1:
        case 2:
        case 3:
        case 4:
            break;

        case 5:
            wait_for_attach = 1;
            break;

        case 'h':
        case '?':
        default:
            usage();
            exit(-1);
        }
    }

    if (wait_for_attach) {
        console("Wait for debugger attaching, process id is: %d.", getpid());
        console("After debugger attached, press any key to continue......");
        getchar();
    }

    rc = fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    if (rc < 0) {
        console("set stdin NON-BLOCKING failed.\n");
        return -1;
    }

    rc = setvbuf(stdin, NULL, _IONBF, 0);
    if (rc < 0) {
        console("set stdin unbuffered failed.\n");
        return -1;
    }

    tcgetattr(STDIN_FILENO, &term_tmp);
    term = term_tmp;
    term_tmp.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_tmp);
    atexit(reset_tcattr);

    config_file = get_cfg_file(config_file, default_config_files);
    if (!config_file) {
        console("missing config file.");
        usage();
        return -1;
    }

    if (!load_cfg(config_file, &opts)) {
        console("loading configure failed!");
        return -1;
    }

    opts.carrier_opts.log_printer = carrier_log_printer;

    console("connecting to carrier network");
    fc = feeds_client_create(&opts);
    free_cfg(&opts);
    if (!fc) {
        console("Error initializing feeds client");
        return -1;
    }
    feeds_client_wait_until_online(fc);

    do {
        state = STANDBY;
        console_prompt();
read_cmd:
        cmd = read_cmd();
        if (state == STANDBY) {
            //check_new_posts();
            //check_new_comments();
            //check_new_likes();
            goto read_cmd;
        } else if (state == TYPING)
            goto read_cmd;
        else if (state == GOTCMD)
            do_cmd(cmd);
    } while (state != STOP);

    feeds_client_delete(fc);
    return 0;
}