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
#include <limits.h>
#include <stdbool.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <crystal.h>

#define MODE_LAUNCHER   0
#define MODE_CASES      1
#define MODE_ROBOT      2

static int mode = MODE_LAUNCHER;
const char *config_file = NULL;

int test_main(int argc, char *argv[]);
int robot_main(int argc, char *argv[]);
int launcher_main(int argc, char *argv[]);

#define CONFIG_NAME "tests.conf"

static const char *default_config_files[] = {
    "./"CONFIG_NAME,
    "../etc/feeds/"CONFIG_NAME,
    "/usr/local/etc/feeds/"CONFIG_NAME,
    "/etc/feeds/"CONFIG_NAME,
    NULL
};

#ifdef HAVE_SYS_RESOURCE_H
int sys_coredump_set(bool enable)
{
    const struct rlimit rlim = {
        enable ? RLIM_INFINITY : 0,
        enable ? RLIM_INFINITY : 0
    };

    return setrlimit(RLIMIT_CORE, &rlim);
}
#endif

static void usage(void)
{
    printf("Elastos Service Feeds tests.\n");
    printf("\n");
    printf("Usage: feedstests [OPTION]...\n");
    printf("\n");
    printf("First run options:\n");
    printf("      --cases               Run test cases only in manual mode.\n");
    printf("      --robot               Run robot only in manual mode.\n");
    printf("  -c, --config=CONFIG_FILE  Set config file path.\n");
    printf("\n");
    printf("Debugging options:\n");
    printf("      --debug               Wait for debugger attach after start.\n");
    printf("\n");
}

void wait_for_debugger_attach(void)
{
    printf("Wait for debugger attaching, process id is: %d.\n", getpid());
    printf("After debugger attached, press any key to continue......");
    getchar();
}

int main(int argc, char *argv[])
{
    int rc;
    int debug = 0;
    char buffer[PATH_MAX];

    int opt;
    int idx;
    struct option options[] = {
        { "cases",     no_argument,       NULL, 1 },
        { "robot",     no_argument,       NULL, 2 },
        { "debug",     no_argument,       NULL, 3 },
        { "help",      no_argument,       NULL, 'h' },
        { "config",    required_argument, NULL, 'c' },
        { NULL,        0,                 NULL, 0 }
    };

#ifdef HAVE_SYS_RESOURCE_H
    sys_coredump_set(true);
#endif

    while ((opt = getopt_long(argc, argv, "r:c:h", options, &idx)) != -1) {
        switch (opt) {
        case 1:
            mode = MODE_CASES;
            break;

        case 2:
            mode = MODE_ROBOT;
            break;

        case 3:
            debug = 1;
            break;

        case 'c':
            config_file = optarg;
            break;

        case 'h':
        case '?':
            usage();
            return -1;
        }
    }

    if (debug)
        wait_for_debugger_attach();

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    switch (mode) {
    case MODE_CASES:
        rc = test_main(argc, argv);
        break;

    case MODE_ROBOT:
        rc = robot_main(argc, argv);
        break;

    case MODE_LAUNCHER:
        realpath(argv[0], buffer);
        argv[0] = buffer;
        rc = launcher_main(argc, argv);
        break;
    }

    return rc;
}
