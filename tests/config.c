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

#include <string.h>

#include <crystal.h>
#include <libconfig.h>

#include "../src/carrier_cfg.h"
#include "config.h"

TestConfig test_config;

static
int extra_config_parser(void *p, ElaOptions *options)
{
    TestConfig *tc = (TestConfig *)options;
    config_t *cfg = (config_t *)p;
    const char *stropt;
    int rc;
    const char *host;
    int port;

    rc = config_lookup_string(cfg, "persistent_location", &stropt);
    if (!rc || !*stropt) {
        fprintf(stderr, "Missing persistent_location section.\n");
        return -1;
    }

    config_lookup_int(cfg, "log2file", &tc->log2file);

    tc->persistent_location = strdup(stropt);

    tc->tests.loglevel = tc->ela_options.log_level;
    config_lookup_int(cfg, "tests.loglevel", &tc->tests.loglevel);

    tc->robot.loglevel = tc->ela_options.log_level;
    config_lookup_int(cfg, "robot.loglevel", &tc->robot.loglevel);

    if (!config_lookup_string(cfg, "robot.host", &host))
        host = "127.0.0.1";
    strncpy(tc->robot.host, host, sizeof(tc->robot.host));

    port = 7238;
    config_lookup_int(cfg, "robot.port", &port);
    sprintf(tc->robot.port, "%d", port);

    return 0;
}

TestConfig *load_config(const char *config_file)
{
    return (TestConfig *)carrier_config_load(config_file, extra_config_parser,
                                             (ElaOptions *)&test_config);
}

void free_config()
{
    carrier_config_free(&(test_config.ela_options));
    if (test_config.persistent_location)
        free(test_config.persistent_location);
    memset(&test_config, 0, sizeof(TestConfig));
}
