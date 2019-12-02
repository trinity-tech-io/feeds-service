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

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <stdio.h>
#include <assert.h>

#include <sqlite3.h>

#include "db.h"

static sqlite3 *db;

int db_initialize(const char *db_file)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    sqlite3_initialize();

    rc = sqlite3_open_v2(db_file, &db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                         NULL);
    if (rc)
        goto failure;

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS channels ("
          "    channel_id  INTEGER PRIMARY KEY AUTOINCREMENT,"
          "    user_id     INTEGER NOT NULL REFERENCES users(user_id),"
          "    created_at  REAL    NOT NULL,"
          "    updated_at  REAL    NOT NULL,"
          "    name        TEXT    NOT NULL UNIQUE,"
          "    intro       TEXT    NOT NULL,"
          "    subscribers INTEGER NOT NULL DEFAULT 0"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto failure;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto failure;

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS posts ("
          "    channel_id      INTEGER NOT NULL REFERENCES channels(channel_id), "
          "    post_id         INTEGER NOT NULL,"
          "    created_at      REAL    NOT NULL,"
          "    updated_at      REAL    NOT NULL,"
          "    next_comment_id INTEGER NOT NULL DEFAULT 1,"
          "    likes           INTEGER NOT NULL DEFAULT 0,"
          "    content         BLOB    NOT NULL,"
          "    PRIMARY KEY(channel_id, post_id)"
          ")";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto failure;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto failure;

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS comments ("
          "    channel_id    INTEGER NOT NULL,"
          "    post_id       INTEGER NOT NULL,"
          "    comment_id    INTEGER NOT NULL,"
          "    refcomment_id INTEGER NOT NULL,"
          "    user_id       INTEGER NOT NULL REFERENCES users(user_id),"
          "    created_at    REAL    NOT NULL,"
          "    updated_at    REAL    NOT NULL,"
          "    likes         INTEGER NOT NULL DEFAULT 0,"
          "    content       BLOB    NOT NULL,"
          "    PRIMARY KEY(channel_id, post_id, comment_id)"
          "    FOREIGN KEY(channel_id, post_id) REFERENCES posts(channel_id, post_id)"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto failure;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto failure;

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS users ("
          "    user_id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "    jwt     TEXT NOT NULL UNIQUE"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto failure;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto failure;

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS subscriptions ("
          "    user_id    INTEGER NOT NULL REFERENCES users(user_id),"
          "    channel_id INTEGER NOT NULL REFERENCES channels(channel_id),"
          "    PRIMARY KEY(user_id, channel_id)"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto failure;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto failure;

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS publishers ("
          "    user_id INTEGER PRIMARY KEY REFERENCES users(user_id)"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto failure;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto failure;

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS owners ("
          "    user_id INTEGER PRIMARY KEY REFERENCES users(user_id)"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto failure;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto failure;

    return 0;

failure:
    db_finalize();
    return -1;
}

int db_iterate_channels(int (*it)(uint64_t id, const char *name, const char *desc,
                                  uint64_t next_post_id, const char *owner))
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT "
          "  channel_id, name, intro, "
          "    MAX(CASE WHEN post_id IS NULL THEN 0 ELSE post_id END) + 1 AS next_post_id, jwt"
          "  FROM channels JOIN users USING(user_id) LEFT OUTER JOIN posts USING(channel_id)"
          "  GROUP BY channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (it(sqlite3_column_int64(stmt, 0),
               (const char *)sqlite3_column_text(stmt, 1),
               (const char *)sqlite3_column_text(stmt, 2),
               sqlite3_column_int64(stmt, 3),
               (const char *)sqlite3_column_text(stmt, 4)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    return 0;
}

int db_iterate_publishers(int (*it)(const char *jwt))
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT jwt FROM publishers JOIN users USING(user_id)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (it((const char *)sqlite3_column_text(stmt, 0)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    return 0;
}

int db_iterate_node_owners(int (*it)(const char *jwt))
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT jwt FROM owners JOIN users USING(user_id);";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (it((const char *)sqlite3_column_text(stmt, 0)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    return 0;
}

int db_create_channel(const char *name, const char *jwt, const char *intro)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT OR IGNORE INTO users(jwt) VALUES (:jwt)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO "
          "  channels(user_id, created_at, updated_at, name, intro)"
          "  VALUES ((SELECT user_id FROM users WHERE jwt = :jwt), "
          "    :now, :now, :name, :intro)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":now"),
                            time(NULL));
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":name"),
                           name, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":intro"),
                           intro, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    return 0;
}

int db_add_post(uint64_t channel_id, uint64_t id, const void *content,
                size_t len, uint64_t ts)
{
    sqlite3_stmt *stmt;
    const char *sql;
    time_t now;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO "
          "  posts(channel_id, post_id, created_at, updated_at, content)"
          "  VALUES (:channel_id, :post_id, :now, :now, :content)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":now"),
                            now = time(NULL));
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_blob(stmt,
                           sqlite3_bind_parameter_index(stmt, ":content"),
                           content, len, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels SET updated_at = :now WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":now"),
                            now = time(NULL));
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
        return -1;
}

int db_comment_exists(uint64_t channel_id, uint64_t post_id, uint64_t comment_id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT "
          "  EXISTS(SELECT * FROM comments WHERE channel_id = :channel_id AND "
          "    post_id = :post_id AND comment_id = :comment_id)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":comment_id"),
                            comment_id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return rc ? 1 : 0;
}

int db_add_comment(uint64_t channel_id, uint64_t post_id, uint64_t comment_id,
                   const char *jwt, const void *content, size_t len,
                   uint64_t created_at, uint64_t *id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    time_t now;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT OR IGNORE INTO users(jwt) VALUES (:jwt)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO "
          "  comments(channel_id, post_id, comment_id, refcomment_id, user_id, "
          "    created_at, updated_at, content)"
          "  VALUES (:channel_id, :post_id, "
          "    (SELECT next_comment_id FROM posts WHERE channel_id = :channel_id "
          "      AND post_id = :post_id), "
          "    :comment_id,"
          "    (SELECT user_id FROM users WHERE jwt = :jwt), "
          "    :now, :now, :content)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":comment_id"),
                            comment_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":now"),
                            now = time(NULL));
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_blob(stmt,
                           sqlite3_bind_parameter_index(stmt, ":content"),
                           content, len, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE posts "
          "  SET updated_at = :now, next_comment_id = next_comment_id + 1"
          "  WHERE channel_id = :channel_id AND post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":now"),
                            now);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels SET updated_at = :now WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":now"),
                            now);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT next_comment_id - 1"
          "  FROM posts"
          "  WHERE channel_id = :channel_id AND post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    *id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_add_like(uint64_t channel_id, uint64_t post_id, uint64_t comment_id, uint64_t *likes)
{
    sqlite3_stmt *stmt;
    const char *sql;
    time_t now;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = comment_id ?
          "UPDATE comments SET likes = likes + 1 "
          "  WHERE channel_id = :channel_id AND post_id = :post_id AND "
          "    comment_id = :comment_id" :
          "UPDATE posts SET likes = likes + 1 "
          "  WHERE channel_id = :channel_id AND post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    if (comment_id) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":comment_id"),
                                comment_id);
        if (rc) {
            sqlite3_finalize(stmt);
            goto rollback;
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = comment_id ?
          "SELECT likes "
          "  FROM comments "
          "  WHERE channel_id = :channel_id AND post_id = :post_id AND "
          "    comment_id = :comment_id" :
          "SELECT likes "
          "  FROM posts "
          "  WHERE channel_id = :channel_id AND post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    if (comment_id) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":comment_id"),
                                comment_id);
        if (rc) {
            sqlite3_finalize(stmt);
            goto rollback;
        }
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    *likes = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_add_subscriber(uint64_t channel_id, const char *jwt)
{
    sqlite3_stmt *stmt;
    const char *sql;
    time_t now;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT OR IGNORE INTO users(jwt) VALUES (:jwt)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO subscriptions"
          "  (user_id, channel_id) "
          "  VALUES ((SELECT user_id FROM users WHERE jwt = :jwt), :channel_id)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels "
          "  SET subscribers = subscribers + 1"
          "  WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_unsubscribe(uint64_t channel_id, const char *jwt)
{
    sqlite3_stmt *stmt;
    const char *sql;
    time_t now;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = "DELETE FROM subscriptions "
          "  WHERE user_id = (SELECT user_id FROM users WHERE jwt = :jwt) AND "
          "    channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels "
          "  SET subscribers = subscribers - 1"
          "  WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        goto rollback;

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        goto rollback;

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

typedef enum {
    CHANNEL,
    POST,
    COMMENT
} QueryObject;

static
const char *query_column(QueryObject qo, QueryField qf)
{
    switch (qf) {
    case ID:
        switch (qo) {
        case CHANNEL:
            return "channel_id";
        case POST:
            return "post_id";
        case COMMENT:
            return "comment_id";
        default:
            assert(0);
        }
    case LAST_UPDATE:
        return "updated_at";
    case CREATED_AT:
        return "created_at";
    default:
        assert(0);
    }
}

int db_get_owned_channels(const char *jwt, QueryField qf, uint64_t upper,
                          uint64_t lower, uint64_t maxcnt, cJSON **result)
{
    sqlite3_stmt *stmt;
    char sql[1024];
    cJSON *channels;
    const char *qc;
    int rc;

    /* ================================= stmt-sep ================================= */
    qc = query_column(CHANNEL, qf);
    rc = sprintf(sql,
                  "SELECT channel_id, name, intro, subscribers"
                  "  FROM channels"
                  "  WHERE user_id = (SELECT user_id FROM users WHERE jwt = :jwt)");
    if (lower)
        rc += sprintf(sql + rc, " AND %s >= :lower", qc);
    if (upper)
        rc += sprintf(sql + rc, " AND %s <= :upper", qc);
    rc += sprintf(sql + rc, " ORDER BY %s %s",
                  qc, qf == ID ? "ASC" : "DESC");
    if (maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    if (lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                lower);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                upper);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                maxcnt);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    channels = cJSON_CreateArray();
    if (!channels) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        cJSON *channel = cJSON_CreateObject();
        if (!channel)
            break;

        cJSON_AddItemToArray(channels, channel);

        if (!cJSON_AddNumberToObject(channel, "id",
                                     sqlite3_column_int64(stmt, 0)))
            break;

        if (!cJSON_AddStringToObject(channel, "name",
                                     (const char *)sqlite3_column_text(stmt, 1)))
            break;

        if (!cJSON_AddStringToObject(channel, "introduction",
                                     (const char *)sqlite3_column_text(stmt, 2)))
            break;

        if (!cJSON_AddNumberToObject(channel, "subscribers",
                                     sqlite3_column_int64(stmt, 3)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        cJSON_Delete(channels);
        return -1;
    }

    *result = channels;

    return 0;
}

int db_get_owned_channels_metadata(const char *jwt, QueryField qf, uint64_t upper,
                                   uint64_t lower, uint64_t maxcnt, cJSON **result)
{
    sqlite3_stmt *stmt;
    char sql[1024];
    cJSON *channels;
    const char *qc;
    int rc;

    /* ================================= stmt-sep ================================= */
    qc = query_column(CHANNEL, qf);
    rc = sprintf(sql,
                 "SELECT channel_id, subscribers FROM channels"
                 "  WHERE user_id = (SELECT user_id FROM users WHERE jwt = :jwt)");
    if (lower)
        rc += sprintf(sql + rc, " AND %s >= :lower", qc);
    if (upper)
        rc += sprintf(sql + rc, " AND %s <= :upper", qc);
    rc += sprintf(sql + rc, " ORDER BY %s %s",
                  qc, qf == ID ? "ASC" : "DESC");
    if (maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    if (lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                lower);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                upper);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                maxcnt);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    channels = cJSON_CreateArray();
    if (!channels) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        cJSON *channel = cJSON_CreateObject();
        if (!channel)
            break;

        cJSON_AddItemToArray(channels, channel);

        if (!cJSON_AddNumberToObject(channel, "id",
                                     sqlite3_column_int64(stmt, 0)))
            break;

        if (!cJSON_AddNumberToObject(channel, "subscribers",
                                     sqlite3_column_int64(stmt, 1)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        cJSON_Delete(channels);
        return -1;
    }

    *result = channels;

    return 0;
}

int db_get_channels(QueryField qf, uint64_t upper,
                    uint64_t lower, uint64_t maxcnt, cJSON **result)
{
    sqlite3_stmt *stmt;
    char sql[1024];
    cJSON *channels;
    const char *qc;
    int rc;

    /* ================================= stmt-sep ================================= */
    qc = query_column(CHANNEL, qf);
    rc = sprintf(sql,
                 "SELECT channel_id, name, intro, jwt, subscribers, updated_at"
                 "  FROM channels JOIN users USING (user_id)");
    if (lower || upper)
        rc += sprintf(sql + rc, " WHERE");
    if (lower)
        rc += sprintf(sql + rc, " %s >= :lower", qc);
    if (upper)
        rc += sprintf(sql + rc, "%s %s <= :upper", lower ? " AND" : "", qc);
    rc += sprintf(sql + rc, " ORDER BY %s %s",
                  qc, qf == ID ? "ASC" : "DESC");
    if (maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    if (lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                lower);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                upper);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                maxcnt);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    channels = cJSON_CreateArray();
    if (!channels) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        cJSON *channel = cJSON_CreateObject();
        if (!channel)
            break;

        cJSON_AddItemToArray(channels, channel);

        if (!cJSON_AddNumberToObject(channel, "id",
                                     sqlite3_column_int64(stmt, 0)))
            break;

        if (!cJSON_AddStringToObject(channel, "name",
                                     (const char *)sqlite3_column_text(stmt, 1)))
            break;

        if (!cJSON_AddStringToObject(channel, "introduction",
                                     (const char *)sqlite3_column_text(stmt, 2)))
            break;

        if (!cJSON_AddStringToObject(channel, "owner_name",
                                     (const char *)sqlite3_column_text(stmt, 3)))
            break;

        if (!cJSON_AddStringToObject(channel, "owner_did",
                                     (const char *)sqlite3_column_text(stmt, 3)))
            break;

        if (!cJSON_AddNumberToObject(channel, "subscribers",
                                     sqlite3_column_int64(stmt, 4)))
            break;

        if (!cJSON_AddNumberToObject(channel, "last_update",
                                     sqlite3_column_int64(stmt, 5)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        cJSON_Delete(channels);
        return -1;
    }

    *result = channels;

    return 0;
}

int db_get_channel_detail(uint64_t id, cJSON **result)
{
    sqlite3_stmt *stmt;
    const char *sql;
    cJSON *channel;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT channel_id, name, intro, jwt, subscribers, updated_at"
          "  FROM channels JOIN users USING (user_id)"
          "  WHERE channel_id = :id";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":id"),
                            id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    channel = cJSON_CreateObject();
    if (!channel) {
        sqlite3_finalize(stmt);
        return -1;
    }

    if (!cJSON_AddNumberToObject(channel, "id",
                                 sqlite3_column_int64(stmt, 0))) {
        cJSON_Delete(channel);
        sqlite3_finalize(stmt);
        return -1;
    }

    if (!cJSON_AddStringToObject(channel, "name",
                                 (const char *)sqlite3_column_text(stmt, 1))) {
        cJSON_Delete(channel);
        sqlite3_finalize(stmt);
        return -1;
    }

    if (!cJSON_AddStringToObject(channel, "introduction",
                                 (const char *)sqlite3_column_text(stmt, 2))) {
        cJSON_Delete(channel);
        sqlite3_finalize(stmt);
        return -1;
    }

    if (!cJSON_AddStringToObject(channel, "owner_name",
                                 (const char *)sqlite3_column_text(stmt, 3))) {
        cJSON_Delete(channel);
        sqlite3_finalize(stmt);
        return -1;
    }

    if (!cJSON_AddStringToObject(channel, "owner_did",
                                 (const char *)sqlite3_column_text(stmt, 3))) {
        cJSON_Delete(channel);
        sqlite3_finalize(stmt);
        return -1;
    }

    if (!cJSON_AddNumberToObject(channel, "subscribers",
                                 sqlite3_column_int64(stmt, 4))) {
        cJSON_Delete(channel);
        sqlite3_finalize(stmt);
        return -1;
    }

    if (!cJSON_AddNumberToObject(channel, "last_update",
                                 sqlite3_column_int64(stmt, 5))) {
        cJSON_Delete(channel);
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);

    *result = channel;

    return 0;
}

int db_get_subscribed_channels(const char *jwt, QueryField qf, uint64_t upper,
                               uint64_t lower, uint64_t maxcnt, cJSON **result)
{
    sqlite3_stmt *stmt;
    char sql[1024];
    cJSON *channels;
    const char *qc;
    int rc;

    /* ================================= stmt-sep ================================= */
    qc = query_column(CHANNEL, qf);
    rc = sprintf(sql,
                 "SELECT channel_id, name, intro, jwt, subscribers, updated_at"
                 "  FROM (SELECT channel_id FROM subscriptions "
                 "          WHERE user_id = (SELECT user_id FROM users WHERE jwt = :jwt)) "
                 "    JOIN channels USING (channel_id) JOIN users USING (user_id)");
    if (lower || upper)
        rc += sprintf(sql + rc, " WHERE");
    if (lower)
        rc += sprintf(sql + rc, " %s >= :lower", qc);
    if (upper)
        rc += sprintf(sql + rc, "%s %s <= :upper", lower ? " AND" : "", qc);
    rc += sprintf(sql + rc, " ORDER BY %s %s",
                  qc, qf == ID ? "ASC" : "DESC");
    if (maxcnt)
        rc += sprintf(sql +rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    if (lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                lower);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                upper);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                maxcnt);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    channels = cJSON_CreateArray();
    if (!channels) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        cJSON *channel = cJSON_CreateObject();
        if (!channel)
            break;

        cJSON_AddItemToArray(channels, channel);

        if (!cJSON_AddNumberToObject(channel, "id",
                                     sqlite3_column_int64(stmt, 0)))
            break;

        if (!cJSON_AddStringToObject(channel, "name",
                                     (const char *)sqlite3_column_text(stmt, 1)))
            break;

        if (!cJSON_AddStringToObject(channel, "introduction",
                                     (const char *)sqlite3_column_text(stmt, 2)))
            break;

        if (!cJSON_AddStringToObject(channel, "owner_name",
                                     (const char *)sqlite3_column_text(stmt, 3)))
            break;

        if (!cJSON_AddStringToObject(channel, "owner_did",
                                     (const char *)sqlite3_column_text(stmt, 3)))
            break;

        if (!cJSON_AddNumberToObject(channel, "subscribers",
                                     sqlite3_column_int64(stmt, 4)))
            break;

        if (!cJSON_AddNumberToObject(channel, "last_update",
                                     sqlite3_column_int64(stmt, 5)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        cJSON_Delete(channels);
        return -1;
    }

    *result = channels;

    return 0;
}

int db_get_posts(uint64_t channel_id, QueryField qf, uint64_t upper,
                 uint64_t lower, uint64_t maxcnt, cJSON **result)
{
    sqlite3_stmt *stmt;
    char sql[1024];
    cJSON *posts;
    const char *qc;
    int rc;

    /* ================================= stmt-sep ================================= */
    qc = query_column(POST, qf);
    rc = sprintf(sql,
                 "SELECT channel_id, post_id, content, "
                 "  next_comment_id - 1 AS comments, likes, created_at"
                 "  FROM posts"
                 "  WHERE channel_id = :channel_id");
    if (lower)
        rc += sprintf(sql + rc, " AND %s >= :lower", qc);
    if (upper)
        rc += sprintf(sql + rc, " AND %s <= :upper", qc);
    rc += sprintf(sql + rc, " ORDER BY %s %s",
                  qc, qf == ID ? "ASC" : "DESC");
    if (maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    if (lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                lower);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                upper);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                maxcnt);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    posts = cJSON_CreateArray();
    if (!posts) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        cJSON *post = cJSON_CreateObject();
        if (!post)
            break;

        cJSON_AddItemToArray(posts, post);

        if (!cJSON_AddNumberToObject(post, "channel_id",
                                     sqlite3_column_int64(stmt, 0)))
            break;

        if (!cJSON_AddNumberToObject(post, "id",
                                     sqlite3_column_int64(stmt, 1)))
            break;

        if (!cJSON_AddStringToObject(post, "content",
                                     sqlite3_column_blob(stmt, 2)))
            break;

        if (!cJSON_AddNumberToObject(post, "comments",
                                     sqlite3_column_int64(stmt, 3)))
            break;

        if (!cJSON_AddNumberToObject(post, "likes",
                                     sqlite3_column_int64(stmt, 4)))
            break;

        if (!cJSON_AddNumberToObject(post, "created_at",
                                     sqlite3_column_int64(stmt, 5)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        cJSON_Delete(posts);
        return -1;
    }

    *result = posts;

    return 0;
}

int db_get_comments(uint64_t channel_id, uint64_t post_id, QueryField qf, uint64_t upper,
                    uint64_t lower, uint64_t maxcnt, cJSON **result)
{
    sqlite3_stmt *stmt;
    char sql[1024];
    cJSON *comments;
    const char *qc;
    int rc;

    /* ================================= stmt-sep ================================= */
    qc = query_column(COMMENT, qf);
    rc = sprintf(sql,
                 "SELECT channel_id, post_id, comment_id, "
                 "  refcomment_id, jwt, content, likes, created_at"
                 "  FROM comments JOIN users USING (user_id)"
                 "  WHERE channel_id = :channel_id AND post_id = :post_id");
    if (lower)
        rc += sprintf(sql + rc, " AND %s >= :lower", qc);
    if (upper)
        rc += sprintf(sql + rc, " AND %s <= :upper", qc);
    rc += sprintf(sql + rc, " ORDER BY %s %s",
                  qc, qf == ID ? "ASC" : "DESC");
    if (maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    if (lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                lower);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                upper);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    if (maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                maxcnt);
        if (rc) {
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    comments = cJSON_CreateArray();
    if (!comments) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        cJSON *comment = cJSON_CreateObject();
        if (!comment)
            break;

        cJSON_AddItemToArray(comments, comment);

        if (!cJSON_AddNumberToObject(comment, "channel_id",
                                     sqlite3_column_int64(stmt, 0)))
            break;

        if (!cJSON_AddNumberToObject(comment, "post_id",
                                     sqlite3_column_int64(stmt, 1)))
            break;

        if (!cJSON_AddNumberToObject(comment, "id",
                                     sqlite3_column_int64(stmt, 2)))
            break;

        if (!cJSON_AddNumberToObject(comment, "comment_id",
                                     sqlite3_column_int64(stmt, 3)))
            break;

        if (!cJSON_AddStringToObject(comment, "user_name",
                                     (const char *)sqlite3_column_text(stmt, 4)))
            break;

        if (!cJSON_AddStringToObject(comment, "content",
                                     sqlite3_column_blob(stmt, 5)))
            break;

        if (!cJSON_AddNumberToObject(comment, "likes",
                                     sqlite3_column_int64(stmt, 6)))
            break;

        if (!cJSON_AddNumberToObject(comment, "created_at",
                                     sqlite3_column_int64(stmt, 7)))
            break;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        cJSON_Delete(comments);
        return -1;
    }

    *result = comments;

    return 0;
}

int db_add_node_publisher(const char *jwt)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT OR IGNORE INTO users(jwt) VALUES (:jwt)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO publishers(user_id) "
          "  VALUES ((SELECT user_id FROM users WHERE jwt = :jwt))";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    return 0;
}

int db_remove_node_publisher(const char *jwt)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "DELETE FROM publishers "
          "  WHERE user_id = (SELECT user_id FROM users WHERE jwt = :jwt)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    return 0;
}

int db_add_node_owner(const char *jwt)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT OR IGNORE INTO users(jwt) VALUES (:jwt)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO owners(user_id) "
          "  VALUES ((SELECT user_id FROM users WHERE jwt = :jwt))";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return -1;

    return 0;
}

int db_is_subscriber(uint64_t channel_id, const char *jwt)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT "
          "  EXISTS(SELECT * FROM subscriptions "
          "           WHERE user_id = (SELECT user_id FROM users WHERE jwt = :jwt) AND "
          "             channel_id = :channel_id)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc)
        return -1;

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":jwt"),
                           jwt, -1, NULL);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return rc;
}

void db_finalize()
{
    sqlite3_close(db);
    sqlite3_shutdown();
}
