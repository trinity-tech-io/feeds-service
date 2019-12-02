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
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#include <crystal.h>

#include "config.h"
#include "../src/mkdirs.h"

extern const char *config_file;

static char publisher_node_id[ELA_MAX_ID_LEN + 1];
static char robot_persistent_location[PATH_MAX];
static char feeds_config[PATH_MAX];
static SOCKET msg_sock = INVALID_SOCKET;
static pid_t feeds_pid;

int wait_test_case_connecting()
{
    int rc;
    SOCKET svr_sock;

    svr_sock = socket_create(SOCK_STREAM, test_config.robot.host,
                             test_config.robot.port);
    if (svr_sock == INVALID_SOCKET)
        return -1;

    rc = listen(svr_sock, 1);
    if (rc < 0) {
        socket_close(svr_sock);
        return -1;
    }

    msg_sock = accept(svr_sock, NULL, NULL);
    socket_close(svr_sock);

    if (msg_sock == INVALID_SOCKET)
        return -1;

    struct timeval timeout = {900,0};
    setsockopt(msg_sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(msg_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

    return 0;
}

static
int get_publisher_node_id_from_test_case()
{
    int rc;

    rc = recv(msg_sock, publisher_node_id, sizeof(publisher_node_id) - 1, 0);
    if (rc < 0 || !ela_id_is_valid(publisher_node_id))
        return -1;

    return 0;
}

static
int generate_feeds_config()
{
    int in_fd;
    int out_fd;
    int rc;
    char buf[4096];

    in_fd = open(config_file, O_RDONLY);
    if (in_fd < 0)
        return -1;

    out_fd = open(feeds_config, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
    if (out_fd < 0) {
        close(in_fd);
        return -1;
    }

    while ((rc = read(in_fd, buf, sizeof(buf))) > 0) {
        rc = write(out_fd, buf, rc);
        if (rc < 0) {
            close(in_fd);
            close(out_fd);
            remove(feeds_config);
            return -1;
        }
    }

    close(in_fd);
    if (rc < 0) {
        close(out_fd);
        remove(feeds_config);
        return -1;
    }

    rc = sprintf(buf, "\nlog-level = %d\n", test_config.robot.loglevel);
    rc = write(out_fd, buf, rc);
    if (rc < 0) {
        close(out_fd);
        remove(feeds_config);
        return -1;
    }

    if (test_config.log2file) {
        rc = sprintf(buf, "log-file = \"%s/feeds.log\"\n", robot_persistent_location);
        rc = write(out_fd, buf, rc);
        if (rc < 0) {
            close(out_fd);
            remove(feeds_config);
            return -1;
        }
    }

    rc = sprintf(buf, "data-dir = \"%s\"\n", robot_persistent_location);
    rc = write(out_fd, buf, rc);
    if (rc < 0) {
        close(out_fd);
        remove(feeds_config);
        return -1;
    }

    rc = sprintf(buf, "publishers = [\"%s\"]\n", publisher_node_id);
    rc = write(out_fd, buf, rc);
    close(out_fd);
    if (rc < 0) {
        remove(feeds_config);
        return -1;
    }

    return 0;
}

static
int notify_test_case_of_feeds_address()
{
    char feeds_address[ELA_MAX_ADDRESS_LEN + 1];
    char fpath[PATH_MAX];
    int fd;
    int rc;

    sleep(1);

    memset(feeds_address, 0, sizeof(feeds_address));

    sprintf(fpath, "%s/address.txt", robot_persistent_location);
    fd = open(fpath, O_RDONLY);
    if (fd < 0)
        return -1;

    rc = read(fd, feeds_address, sizeof(feeds_address) - 1);
    close(fd);
    if (rc < 0 || !ela_address_is_valid(feeds_address))
        return -1;

    rc = send(msg_sock, feeds_address, strlen(feeds_address), 0);
    if (rc < 0)
        return -1;

    return 0;
}

static
int wait_test_case_disconnected()
{
    char buf[1024];
    int rc;

    while ((rc = recv(msg_sock, buf, sizeof(buf), 0)) > 0) ;

    return rc;
}

static
void cleanup()
{
    char dbpath[PATH_MAX];

    if (msg_sock != INVALID_SOCKET)
        socket_close(msg_sock);

    if (feeds_pid > 0) {
        kill(feeds_pid, SIGINT);
        waitpid(feeds_pid, NULL, 0);
    }

    free_config();

    sprintf(dbpath, "%s/feeds.sqlite3", robot_persistent_location);
    remove(dbpath);
}

static
void signal_handler(int signum)
{
    cleanup();
    exit(-1);
}

int robot_main(int argc, char *argv[])
{
    char feeds_path[PATH_MAX];
    int rc;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!load_config(config_file)) {
        fprintf(stderr, "Loading configure failed!\n");
        return -1;
    }

    sprintf(robot_persistent_location, "%s/robot", test_config.persistent_location);
    rc = mkdirs(robot_persistent_location, S_IRWXU);
    if (rc < 0) {
        fprintf(stderr, "mkdirs error!\n");
        goto cleanup;
    }

    printf("waiting test case connection ...");
    fflush(stdout);
    rc = wait_test_case_connecting();
    if (rc < 0) {
        fprintf(stderr, "test case connection error!\n");
        goto cleanup;
    }
    printf("ok\n");

    printf("getting publisher's node id ...");
    fflush(stdout);
    rc = get_publisher_node_id_from_test_case();
    if (rc < 0) {
        fprintf(stderr, "get publisher node id error!\n");
        goto cleanup;
    }
    printf("ok\n");

    sprintf(feeds_config, "%s/feeds.conf", robot_persistent_location);
    rc = generate_feeds_config();
    if (rc < 0) {
        fprintf(stderr, "generate feeds config error!\n");
        goto cleanup;
    }

    printf("starting service feeds ...");
    fflush(stdout);
    feeds_pid = fork();
    if (feeds_pid < 0) {
        fprintf(stderr, "error starting feeds!\n");
        goto cleanup;
    } else if (!feeds_pid) {
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        sprintf(feeds_path, "%s/svc_feeds", dirname(argv[0]));
        execl(feeds_path, feeds_path, "-c", feeds_config, NULL);
        return -1;
    }
    printf("ok\n");

    printf("notifying test case of feeds carrier address ...");
    fflush(stdout);
    rc = notify_test_case_of_feeds_address();
    if (rc) {
        fprintf(stderr, "notify feeds address error!\n");
        goto cleanup;
    }
    printf("ok\n");

    rc = wait_test_case_disconnected();

cleanup:
    cleanup();

    return rc;
}

