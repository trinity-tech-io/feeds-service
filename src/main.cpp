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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

#include <memory>
#include <iostream>

#include <MassDataManager.hpp>
#include <Platform.hpp>

extern "C" {
#define new fix_cpp_keyword_new
#include <ela_carrier.h>
#include <crystal.h>

#include "feeds.h"
#include "auth.h"
#include "msgq.h"
#include "cfg.h"
#include "did.h"
#include "rpc.h"
#include "db.h"
#include "ver.h"
#undef new
}

static const char *resolver = "http://api.elastos.io:20606";
size_t connecting_clients;
std::shared_ptr<ElaCarrier> carrier_instance;
ElaCarrier *carrier;
static bool stop;

static struct {
    const char *method;
    void (*hdlr)(ElaCarrier *c, const char *from, Req *base);
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
    {"edit_post"                   , hdl_edit_post_req          },
    {"delete_post"                 , hdl_del_post_req           },
    {"post_comment"                , hdl_post_cmt_req           },
    {"edit_comment"                , hdl_edit_cmt_req           },
    {"delete_comment"              , hdl_del_cmt_req            },
    {"block_comment"               , hdl_block_cmt_req          },
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


static
void on_receiving_message(ElaCarrier *c, const char *from,
                          const void *msg, size_t len, int64_t timestamp,
                          bool offline, void *context)
{
    Req *req;
    int rc;
    int i;

    (void)offline;
    (void)context;

    rc = rpc_unmarshal_req(msg, len, &req);
    if (rc == -1)
        return;
    else if (rc == -2) {
        hdl_unknown_req(c, from, req);
        deref(req);
        return;
    }

    for (i = 0; i < sizeof(method_hdlrs) / sizeof(method_hdlrs[0]); ++i) {
        if (!strcmp(req->method, method_hdlrs[i].method)) {
            method_hdlrs[i].hdlr(c, from, req);
            break;
        }
    }

    deref(req);
}

static
void idle_callback(ElaCarrier *c, void *context)
{
    (void)c;
    (void)context;

    if (stop) {
        // avoid clean session in idle thread, it will crash.
        // elastos::MassDataManager::GetInstance()->cleanup();
        carrier_instance.reset();
        carrier = NULL;
        return;
    }

    auth_expire_login();
}

static
void on_connection_status(ElaCarrier *carrier,
                          ElaConnectionStatus status, void *context)
{
    vlogI("carrier %s", status == ElaConnectionStatus_Connected ?
                                "connected" : "disconnected");
}

static
void friend_connection_callback(ElaCarrier *c, const char *friend_id,
                                ElaConnectionStatus status, void *context)
{
    (void)c;
    (void)context;

    vlogI("[%s] %s", friend_id, status == ElaConnectionStatus_Connected ?
                                "connected" : "disconnected");

    if (status == ElaConnectionStatus_Connected) {
        ++connecting_clients;
        return;
    }

    --connecting_clients;
    feeds_deactivate_suber(friend_id);
    msgq_peer_offline(friend_id);
}

static
void friend_request_callback(ElaCarrier *c, const char *user_id,
                             const ElaUserInfo *info, const char *hello,
                             void *context)
{
    (void)info;
    (void)hello;
    (void)context;

    vlogI("Received friend request from [%s]", user_id);
    ela_accept_friend(c, user_id);
}

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>

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
    printf("Elastos feeds service.\n");
    printf("Usage: feedsd [OPTION]...\n");
    printf("\n");
    printf("First run options:\n");
    printf("  -d, --daemon              Run as daemon.\n");
    printf("  -c, --config=CONFIG_FILE  Set config file path.\n");
    printf("      --udp-enabled=0|1     Enable UDP, override the option in config.\n");
    printf("      --log-level=LEVEL     Log level(0-7), override the option in config.\n");
    printf("      --log-file=FILE       Log file name, override the option in config.\n");
    printf("      --data-dir=PATH       Data location, override the option in config.\n");
    printf("\n");
    printf("Debugging options:\n");
    printf("      --debug               Wait for debugger attach after start.\n");
    printf("\n");
}

#define CONFIG_NAME "feedsd.conf"
static const char *default_cfg_files[] = {
    "./" CONFIG_NAME,
    "../etc/feedsd/" CONFIG_NAME,
    "/usr/local/etc/feedsd/" CONFIG_NAME,
    "/etc/feedsd/" CONFIG_NAME,
    NULL
};

static
void shutdown_proc(int signum)
{
    (void)signum;

    stop = true;
}

static
void print_backtrace(int sig) {
    std::cerr << "Error: signal " << sig << std::endl;

    std::string backtrace = elastos::Platform::GetBacktrace();
    std::cerr << backtrace << std::endl;

    exit(sig);
}

static
int daemonize()
{
    pid_t pid;
    struct sigaction sa;

    /*
     * Clear file creation mask.
     */
    umask(0);

    /*
     * Become a session leader to lose controlling TTY.
     */
    if ((pid = fork()) < 0)
        return -1;
    else if (pid != 0) /* parent */
        exit(0);
    setsid();

    /*
     * Ensure future opens won’t allocate controlling TTYs.
     */
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return -1;
    if ((pid = fork()) < 0)
        return -1;
    else if (pid != 0) /* parent */
        exit(0);

    /*
     * Change the current working directory to the root so
     * we won’t prevent file systems from being unmounted.
     */
    if (chdir("/") < 0)
        return -1;

    /* Attach file descriptors 0, 1, and 2 to /dev/null. */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);

    return 0;
}

static
void transport_deinit()
{
    elastos::MassDataManager::GetInstance()->cleanup();
    carrier_instance.reset();
    carrier = NULL;
}

static
int transport_init(FeedsConfig *cfg)
{
    int rc;
    ElaCallbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.idle = idle_callback;
    callbacks.connection_status = on_connection_status;
    callbacks.friend_connection = friend_connection_callback;
    callbacks.friend_request = friend_request_callback;
    callbacks.friend_message = on_receiving_message;

    DIDBackend_InitializeDefault(resolver, cfg->didcache_dir);

    auto creater = [&]() -> ElaCarrier* {
        vlogD("Create carrier instance.");
        auto ptr = ela_new(&cfg->carrier_opts, &callbacks, NULL);
        return ptr;
    };
    auto deleter = [=](ElaCarrier* ptr) -> void {
        vlogD("Kill carrier instance.");
        if(ptr != nullptr) {
            ela_kill(ptr);
        }
    };
    carrier_instance = std::shared_ptr<ElaCarrier>(creater(), deleter);
    carrier = carrier_instance.get();
    if (!carrier) {
        vlogE("Creating carrier instance failed");
        goto failure;
    }

    elastos::Log::SetLevel(static_cast<elastos::Log::Level>(cfg->carrier_opts.log_level));
    rc = elastos::MassDataManager::GetInstance()->config(cfg->data_dir, carrier_instance);
    if(rc < 0) {
        vlogE("Carrier session init failed");
        goto failure;
    }

    vlogI("Transport module initialized.");

    return 0;

failure:
    transport_deinit();
    return -1;
}

int main(int argc, char *argv[])
{
    char buf[ELA_MAX_ADDRESS_LEN + 1];
    const char *cfg_file = NULL;
    int wait_for_attach = 0;
    FeedsConfig cfg;
    int daemon = 0;
    int rc;

    int opt;
    int idx;
    struct option options[] = {
        { "daemon",      no_argument,        NULL, 'd' },
        { "config",      required_argument,  NULL, 'c' },
        { "debug",       no_argument,        NULL, 5 },
        { "help",        no_argument,        NULL, 'h' },
        { NULL,          0,                  NULL, 0 }
    };

#ifdef HAVE_SYS_RESOURCE_H
    sys_coredump_set(true);
#endif

    while ((opt = getopt_long(argc, argv, "dc:h?", options, &idx)) != -1) {
        switch (opt) {
        case 'd':
            daemon = 1;
            break;

        case 'c':
            cfg_file = optarg;
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

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    if (wait_for_attach) {
        printf("Wait for debugger attaching, process id is: %d.\n", getpid());
        printf("After debugger attached, press any key to continue......");
        getchar();
    }

    cfg_file = get_cfg_file(cfg_file, default_cfg_files);
    if (!cfg_file) {
        vlogE("Missing config file.");
        usage();
        return -1;
    }

    memset(&cfg, 0, sizeof(cfg));
    if (!load_cfg(cfg_file, &cfg)) {
        vlogE("Loading configure failed!");
        return -1;
    }

    rc = transport_init(&cfg);
    if (rc < 0) {
        free_cfg(&cfg);
        return -1;
    }

    rc = msgq_init();
    if (rc < 0) {
        free_cfg(&cfg);
        transport_deinit();
        return -1;
    }

    rc = db_init(cfg.db_fpath);
    if (rc < 0) {
        free_cfg(&cfg);
        msgq_deinit();
        transport_deinit();
        return -1;
    }

    rc = did_init(&cfg);
    if (rc < 0) {
        free_cfg(&cfg);
        db_deinit();
        msgq_deinit();
        transport_deinit();
        return -1;
    }

    rc = auth_init();
    if (rc < 0) {
        free_cfg(&cfg);
        did_deinit();
        db_deinit();
        msgq_deinit();
        transport_deinit();
        return -1;
    }

    rc = feeds_init(&cfg);
    if (rc < 0) {
        free_cfg(&cfg);
        auth_deinit();
        did_deinit();
        db_deinit();
        msgq_deinit();
        transport_deinit();
        return -1;
    }

    signal(SIGINT, shutdown_proc);
    signal(SIGTERM, shutdown_proc);

    signal(SIGSEGV, print_backtrace);
    signal(SIGABRT, print_backtrace);

    printf("Feedsd version: %s\n", FEEDSD_VER);
    printf("Carrier node identities:\n");
    printf("  Node ID  : %s\n", ela_get_nodeid(carrier, buf, sizeof(buf)));
    printf("  User ID  : %s\n", ela_get_userid(carrier, buf, sizeof(buf)));
    printf("  Address  : %s\n", ela_get_address(carrier, buf, sizeof(buf)));
    if (feeds_owner_info.did && feeds_owner_info.did[0])
        printf("  Owner DID: %s\n", feeds_owner_info.did);
    if (feeds_did_str[0])
        printf("  Feeds DID: %s\n", feeds_did_str);

    if (!did_is_ready())
        printf("Visit http://YOUR-IP-ADDRESS:%s using your browser to start binding process."
               "Verification code:[%s]\n", cfg.http_port, did_get_nonce());
    else
        printf("Visiting http://YOUR-IP-ADDRESS:%s with your browser to retrieve the QRcode "
               "of feeds URL\n", cfg.http_port);
    free_cfg(&cfg);

    if (daemon && daemonize() < 0) {
        fprintf(stderr, "Demonize failure!\n");
        feeds_deinit();
        auth_deinit();
        did_deinit();
        db_deinit();
        msgq_deinit();
        transport_deinit();
        return -1;
    }

    rc = ela_run(carrier, 10);

    feeds_deinit();
    auth_deinit();
    did_deinit();
    db_deinit();
    msgq_deinit();
    transport_deinit();

    return rc;
}
