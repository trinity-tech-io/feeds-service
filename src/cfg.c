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

#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <libconfig.h>
#include <crystal.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <libgen.h>
#include <unistd.h>
#endif

#include "mkdirs.h"
#include "cfg.h"

const char *get_cfg_file(const char *config_file, const char *default_config_files[])
{
    const char **file = config_file ? &config_file : default_config_files;

    for (; *file; ) {
        int fd = open(*file, O_RDONLY);
        if (fd < 0) {
            if (*file == config_file)
                file = default_config_files;
            else
                file++;

            continue;
        }

        close(fd);

        return *file;
    }

    return NULL;
}

static void bootstraps_dtor(void *p)
{
    size_t i;
    size_t *size = (size_t *)p;
    BootstrapNode *bootstraps = (struct BootstrapNode *)(size + 1);

    for (i = 0; i < *size; i++) {
        BootstrapNode *node = bootstraps + i;

        if (node->ipv4)
            free((void *)node->ipv4);

        if (node->ipv6)
            free((void *)node->ipv6);

        if (node->port)
            free((void *)node->port);

        if (node->public_key)
            free((void *)node->public_key);
    }
}

#define PATH_SEP "/"
#define HOME_ENV "HOME"
static void qualified_path(const char *path, const char *ref, char *qualified)
{
    assert(strlen(path) >= 1);

    if (*path == PATH_SEP[0] || path[1] == ':') {
        strcpy(qualified, path);
    } else if (*path == '~') {
        sprintf(qualified, "%s%s", getenv(HOME_ENV), path+1);
    } else {
        getcwd(qualified, PATH_MAX);
        strcat(qualified, PATH_SEP);
        strcat(qualified, path);
    }
}

#define DEFAULT_LOG_LEVEL CarrierLogLevel_Info
#define DEFAULT_DATA_DIR  "/var/lib/feedsd"
FeedsConfig *load_cfg(const char *cfg_file, FeedsConfig *fc)
{
    config_setting_t *nodes_setting;
    config_setting_t *node_setting;
    char path[PATH_MAX];
    const char *stropt;
    char number[64];
    config_t cfg;
    size_t *mem;
    int entries;
    int intopt;
    int rc;
    int i;

    if (!cfg_file || !*cfg_file || !fc)
        return NULL;

    memset(fc, 0, sizeof(FeedsConfig));

    config_init(&cfg);

    rc = config_read_file(&cfg, cfg_file);
    if (!rc) {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return NULL;
    }

    nodes_setting = config_lookup(&cfg, "carrier.bootstraps");
    if (!nodes_setting) {
        fprintf(stderr, "Missing bootstraps section.\n");
        config_destroy(&cfg);
        return NULL;
    }

    entries = config_setting_length(nodes_setting);
    if (entries <= 0) {
        fprintf(stderr, "Empty bootstraps option.\n");
        config_destroy(&cfg);
        return NULL;
    }

    mem = (size_t *)rc_zalloc(sizeof(size_t) +
                              sizeof(BootstrapNode) * entries, bootstraps_dtor);
    if (!mem) {
        fprintf(stderr, "Load configuration failed, out of memory.\n");
        config_destroy(&cfg);
        return NULL;
    }

    *mem = entries;
    fc->carrier_opts.bootstraps_size = entries;
    fc->carrier_opts.bootstraps = (BootstrapNode *)(++mem);

    for (i = 0; i < entries; i++) {
        BootstrapNode *node = fc->carrier_opts.bootstraps + i;

        node_setting = config_setting_get_elem(nodes_setting, i);

        rc = config_setting_lookup_string(node_setting, "ipv4", &stropt);
        if (rc && *stropt)
            node->ipv4 = (const char *)strdup(stropt);
        else
            node->ipv4 = NULL;

        rc = config_setting_lookup_string(node_setting, "ipv6", &stropt);
        if (rc && *stropt)
            node->ipv6 = (const char *)strdup(stropt);
        else
            node->ipv6 = NULL;

        rc = config_setting_lookup_int(node_setting, "port", &intopt);
        if (rc && intopt) {
            sprintf(number, "%d", intopt);
            node->port = (const char *)strdup(number);
        } else
            node->port = NULL;

        rc = config_setting_lookup_string(node_setting, "public-key", &stropt);
        if (rc && *stropt)
            node->public_key = (const char *)strdup(stropt);
        else
            node->public_key = NULL;
    }

    fc->carrier_opts.udp_enabled = true;
    rc = config_lookup_bool(&cfg, "carrier.udp-enabled", &intopt);
    if (rc)
        fc->carrier_opts.udp_enabled = !!intopt;

    fc->carrier_opts.log_level = DEFAULT_LOG_LEVEL;
    rc = config_lookup_int(&cfg, "log-level", &intopt);
    if (rc)
        fc->carrier_opts.log_level = intopt;

    rc = config_lookup_string(&cfg, "log-file", &stropt);
    if (rc && *stropt) {
        qualified_path(stropt, cfg_file, path);
        fc->carrier_opts.log_file = strdup(path);
    }

    rc = config_lookup_string(&cfg, "data-dir", &stropt);
    if (!rc || !*stropt)
        stropt = DEFAULT_DATA_DIR;
    qualified_path(stropt, cfg_file, path);
    fc->data_dir = strdup(path);

    sprintf(path, "%s/carrier", fc->data_dir);
    fc->carrier_opts.persistent_location = strdup(path);
    if (!fc->carrier_opts.persistent_location ||
        makedirs(fc->carrier_opts.persistent_location, S_IRWXU) < 0) {
        fprintf(stderr, "Making carrier dir[%s] failed.\n", fc->carrier_opts.persistent_location);
        config_destroy(&cfg);
        free_cfg(fc);
        return NULL;
    }

    sprintf(path, "%s/didcache", fc->data_dir);
    fc->didcache_dir = strdup(path);
    if (!fc->didcache_dir || makedirs(fc->didcache_dir, S_IRWXU) < 0) {
        fprintf(stderr, "Making did cache dir[%s] failed.\n", fc->didcache_dir);
        config_destroy(&cfg);
        free_cfg(fc);
        return NULL;
    }

    sprintf(path, "%s/didstore", fc->data_dir);
    fc->didstore_dir = strdup(path);
    if (!fc->didstore_dir || makedirs(fc->didstore_dir, S_IRWXU) < 0) {
        fprintf(stderr, "Making did store dir[%s] failed.\n", fc->didstore_dir);
        config_destroy(&cfg);
        free_cfg(fc);
        return NULL;
    }

    sprintf(path, "%s/db/feeds.sqlite3", fc->data_dir);
    fc->db_fpath = strdup(path);
    strcpy(path, fc->db_fpath);
    if (!fc->db_fpath || makedirs(dirname(path), S_IRWXU) < 0) {
        fprintf(stderr, "Making db dir[%s] failed.\n", path);
        config_destroy(&cfg);
        free_cfg(fc);
        return NULL;
    }

    rc = config_lookup_string(&cfg, "did.store-password", &stropt);
    if (!rc || !*stropt || !(fc->didstore_passwd = strdup(stropt))) {
        fprintf(stderr, "Missing did.store-password entry.\n");
        config_destroy(&cfg);
        free_cfg(fc);
        return NULL;
    }

    rc = config_lookup_string(&cfg, "did.binding-server.ip", &stropt);
    if (!rc || !*stropt || !(fc->http_ip = strdup(stropt))) {
        fprintf(stderr, "Missing did.binding-server.ip entry.\n");
        config_destroy(&cfg);
        free_cfg(fc);
        return NULL;
    }

    rc = config_lookup_int(&cfg, "did.binding-server.port", &intopt);
    if (!rc || (intopt <= 0 || intopt > 65535)) {
        fprintf(stderr, "Missing did.binding-server.port entry.\n");
        config_destroy(&cfg);
        free_cfg(fc);
        return NULL;
    }
    sprintf(number, "%d", intopt);
    fc->http_port = strdup(number);

    config_destroy(&cfg);
    return fc;
}

void free_cfg(FeedsConfig *fc)
{
    if (fc->carrier_opts.persistent_location)
        free((void *)fc->carrier_opts.persistent_location);

    if (fc->carrier_opts.log_file)
        free((void *)fc->carrier_opts.log_file);

    if (fc->carrier_opts.bootstraps) {
        size_t *p = (size_t *)fc->carrier_opts.bootstraps;
        deref(--p);
    }

    if (fc->carrier_opts.express_nodes) {
        size_t *p = (size_t *)fc->carrier_opts.express_nodes;
        deref(--p);
    }

    if (fc->data_dir)
        free(fc->data_dir);

    if (fc->didcache_dir)
        free(fc->didcache_dir);

    if (fc->didstore_dir)
        free(fc->didstore_dir);

    if (fc->db_fpath)
        free(fc->db_fpath);

    if (fc->didstore_passwd)
        free(fc->didstore_passwd);

    if (fc->http_ip)
        free(fc->http_ip);

    if (fc->http_port)
        free(fc->http_port);
}
