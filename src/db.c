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

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <assert.h>

#include <crystal.h>
#include <sqlite3.h>

#include "did.h"
#include "db.h"

typedef struct {
    UserInfo info;
    sqlite3_stmt *stmt;
} DBUserInfo;

typedef void *(*Row2Raw)(sqlite3_stmt *);
struct DBObjIt {
    sqlite3_stmt *stmt;
    Row2Raw cb;
};

static sqlite3 *db;

int db_init(const char *db_file)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    sqlite3_initialize();

    rc = sqlite3_open_v2(db_file, &db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                         NULL);
    if (rc) {
        vlogE("sqlite3_open_v2() failed");
        goto failure;
    }

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS channels ("
          "  channel_id   INTEGER PRIMARY KEY AUTOINCREMENT,"
          "  created_at   REAL    NOT NULL,"
          "  updated_at   REAL    NOT NULL,"
          "  name         TEXT    NOT NULL UNIQUE,"
          "  intro        TEXT    NOT NULL,"
          "  next_post_id INTEGER NOT NULL DEFAULT 1,"
          "  subscribers  INTEGER NOT NULL DEFAULT 0,"
          "  avatar       BLOB    NOT NULL"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto failure;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Creating channels table failed");
        goto failure;
    }

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS posts ("
          "  channel_id      INTEGER NOT NULL REFERENCES channels(channel_id),"
          "  post_id         INTEGER NOT NULL,"
          "  created_at      REAL    NOT NULL,"
          "  updated_at      REAL    NOT NULL,"
          "  next_comment_id INTEGER NOT NULL DEFAULT 1,"
          "  likes           INTEGER NOT NULL DEFAULT 0, "
          "  content         BLOB    NOT NULL,"
          "  PRIMARY KEY(channel_id, post_id)"
          ")";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto failure;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Creating posts table failed");
        goto failure;
    }

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS comments ("
          "  channel_id    INTEGER NOT NULL,"
          "  post_id       INTEGER NOT NULL,"
          "  comment_id    INTEGER NOT NULL,"
          "  refcomment_id INTEGER NOT NULL,"
          "  user_id       INTEGER NOT NULL REFERENCES users(user_id),"
          "  created_at    REAL    NOT NULL,"
          "  updated_at    REAL    NOT NULL,"
          "  likes         INTEGER NOT NULL DEFAULT 0, "
          "  content       BLOB    NOT NULL,"
          "  PRIMARY KEY(channel_id, post_id, comment_id)"
          "  FOREIGN KEY(channel_id, post_id) REFERENCES posts(channel_id, post_id)"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto failure;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Creating comments table failed");
        goto failure;
    }

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS users ("
          "  user_id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "  did     TEXT NOT NULL UNIQUE,"
          "  name    TEXT NOT NULL DEFAULT \"NA\","
          "  email   TEXT NOT NULL DEFAULT \"NA\""
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto failure;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Creating users table failed");
        goto failure;
    }

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS subscriptions ("
          "  user_id    INTEGER NOT NULL REFERENCES users(user_id),"
          "  channel_id INTEGER NOT NULL REFERENCES channels(channel_id),"
          "  PRIMARY KEY(user_id, channel_id)"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto failure;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Creating subscriptions table failed");
        goto failure;
    }

    /* ================================= stmt-sep ================================= */
    sql = "CREATE TABLE IF NOT EXISTS likes ("
          "    channel_id INTEGER NOT NULL,"
          "    post_id    INTEGER NOT NULL,"
          "    comment_id INTEGER NOT NULL,"
          "    user_id    INTEGER NOT NULL REFERENCES users(user_id),"
          "    created_at REAL    NOT NULL,"
          "    FOREIGN KEY(channel_id, post_id) REFERENCES posts(channel_id, post_id)"
          "    PRIMARY KEY(user_id, channel_id, post_id, comment_id)"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto failure;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Creating likes table failed");
        goto failure;
    }

    return 0;

failure:
    db_deinit();
    return -1;
}

int db_create_chan(const ChanInfo *ci)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO channels(created_at, updated_at, name, intro, avatar) "
          "  VALUES (:ts, :ts, :name, :intro, :avatar)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":ts"),
                            ci->created_at);
    if (rc) {
        vlogE("Binding parameter ts failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":name"),
                           ci->name, -1, NULL);
    if (rc) {
        vlogE("Binding parameter name failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":intro"),
                           ci->intro, -1, NULL);
    if (rc) {
        vlogE("Binding parameter intro failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_blob(stmt,
                           sqlite3_bind_parameter_index(stmt, ":avatar"),
                           ci->avatar, ci->len, NULL);
    if (rc) {
        vlogE("Binding parameter avatar failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Inserting new channel failed");
        return -1;
    }

    return 0;
}

int db_upd_chan(const ChanInfo *ci)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels"
          "  SET updated_at = :upd_at, name = :name, intro = :intro, avatar = :avatar"
          "  WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":upd_at"),
                            ci->upd_at);
    if (rc) {
        vlogE("Binding parameter upd_at failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":name"),
                           ci->name, -1, NULL);
    if (rc) {
        vlogE("Binding parameter name failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":intro"),
                           ci->intro, -1, NULL);
    if (rc) {
        vlogE("Binding parameter intro failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_blob(stmt,
                           sqlite3_bind_parameter_index(stmt, ":avatar"),
                           ci->avatar, ci->len, NULL);
    if (rc) {
        vlogE("Binding parameter avatar failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            ci->chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Updating new channel failed");
        return -1;
    }

    return 0;
}

int db_add_post(const PostInfo *pi)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing BEIGIN failed");
        return -1;
    }

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO posts(channel_id, post_id, created_at, updated_at, content) "
          "  VALUES (:channel_id, :post_id, :ts, :ts, :content)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            pi->chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            pi->post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":ts"),
                            pi->created_at);
    if (rc) {
        vlogE("Binding parameter ts failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_blob(stmt,
                           sqlite3_bind_parameter_index(stmt, ":content"),
                           pi->content, pi->len, NULL);
    if (rc) {
        vlogE("Binding parameter content failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing INSERT failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels "
          "  SET next_post_id = next_post_id + 1"
          "  WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            pi->chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing UPDATE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing END failed");
        goto rollback;
    }

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_cmt_exists(uint64_t channel_id, uint64_t post_id, uint64_t comment_id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT EXISTS( "
          "  SELECT * "
          "    FROM comments "
          "    WHERE channel_id = :channel_id AND "
          "          post_id = :post_id AND "
          "          comment_id = :comment_id"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":comment_id"),
                            comment_id);
    if (rc) {
        vlogE("Binding parameter comment_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return rc ? 1 : 0;
}

int db_add_cmt(CmtInfo *ci, uint64_t *id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing BEGIN failed");
        return -1;
    }

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO comments("
          "  channel_id, post_id, comment_id, "
          "  refcomment_id, user_id, created_at, updated_at, content"
          ") VALUES ("
          "  :channel_id, :post_id, "
          "  (SELECT next_comment_id "
          "     FROM posts "
          "     WHERE channel_id = :channel_id AND "
          "           post_id = :post_id), "
          "  :comment_id, :uid, :ts, :ts, :content"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            ci->chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            ci->post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":comment_id"),
                            ci->reply_to_cmt);
    if (rc) {
        vlogE("Binding parameter comment_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            ci->user.uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":ts"),
                            ci->created_at);
    if (rc) {
        vlogE("Binding parameter ts failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_blob(stmt,
                           sqlite3_bind_parameter_index(stmt, ":content"),
                           ci->content, ci->len, NULL);
    if (rc) {
        vlogE("Binding parameter content failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing INSERT failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE posts "
          "  SET next_comment_id = next_comment_id + 1 "
          "  WHERE channel_id = :channel_id AND post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            ci->chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            ci->post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing UPDATE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "SELECT next_comment_id - 1 "
          "  FROM posts "
          "  WHERE channel_id = :channel_id AND post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            ci->chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            ci->post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    *id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing END failed");
        goto rollback;
    }

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_like_exists(uint64_t uid, uint64_t channel_id, uint64_t post_id, uint64_t comment_id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT EXISTS(SELECT * "
          "                FROM likes "
          "                WHERE user_id = :uid AND "
          "                      channel_id = :channel_id AND "
          "                      post_id = :post_id AND "
          "                      comment_id = :comment_id)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":comment_id"),
                            comment_id);
    if (rc) {
        vlogE("Binding parameter comment_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return rc ? 1 : 0;
}

int db_add_like(uint64_t uid, uint64_t channel_id, uint64_t post_id,
                uint64_t comment_id, uint64_t *likes)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Exectuing BEGIN failed");
        return -1;
    }

    /* ================================= stmt-sep ================================= */
    sql = comment_id ?
          "UPDATE comments "
          "  SET likes = likes + 1 "
          "  WHERE channel_id = :channel_id AND "
          "        post_id = :post_id AND "
          "        comment_id = :comment_id" :
          "UPDATE posts "
          "  SET likes = likes + 1 "
          "  WHERE channel_id = :channel_id AND "
          "        post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    if (comment_id) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":comment_id"),
                                comment_id);
        if (rc) {
            vlogE("Binding parameter comment_id failed");
            sqlite3_finalize(stmt);
            goto rollback;
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing UPDATE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO likes(user_id, channel_id, post_id, comment_id, created_at) "
          "  VALUES (:uid, :channel_id, :post_id, :comment_id, :ts)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding paramter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":comment_id"),
                            comment_id);
    if (rc) {
        vlogE("Binding parameter comment_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":ts"),
                            time(NULL));
    if (rc) {
        vlogE("Binding parameter ts failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing INSERT failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = comment_id ?
          "SELECT likes "
          "  FROM comments "
          "  WHERE channel_id = :channel_id AND "
          "        post_id = :post_id AND "
          "        comment_id = :comment_id" :
          "SELECT likes "
          "  FROM posts "
          "  WHERE channel_id = :channel_id AND "
          "        post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    if (comment_id) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":comment_id"),
                                comment_id);
        if (rc) {
            vlogE("Binding parameter comment_id failed");
            sqlite3_finalize(stmt);
            goto rollback;
        }
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    *likes = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing END failed");
        goto rollback;
    }

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_rm_like(uint64_t uid, uint64_t channel_id, uint64_t post_id, uint64_t comment_id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing BEGIN failed");
        return -1;
    }

    /* ================================= stmt-sep ================================= */
    sql = comment_id ?
          "UPDATE comments "
          "  SET likes = likes - 1 "
          "  WHERE channel_id = :channel_id AND "
          "        post_id = :post_id AND "
          "        comment_id = :comment_id" :
          "UPDATE posts "
          "  SET likes = likes - 1 "
          "  WHERE channel_id = :channel_id AND "
          "        post_id = :post_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    if (comment_id) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":comment_id"),
                                comment_id);
        if (rc) {
            vlogE("Binding parameter comment_id failed");
            sqlite3_finalize(stmt);
            goto rollback;
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing UPDATE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "DELETE FROM likes "
          "  WHERE user_id = :uid AND channel_id = :channel_id AND "
          "        post_id = :post_id AND comment_id = :comment_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            channel_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":comment_id"),
                            comment_id);
    if (rc) {
        vlogE("Binding parameter comment_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing DELETE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing END failed");
        goto rollback;
    }

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_add_sub(uint64_t uid, uint64_t chan_id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing BEGIN failed");
        return -1;
    }

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO subscriptions(user_id, channel_id) "
          "  VALUES (:uid, :channel_id)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing INSERT failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels "
          "  SET subscribers = subscribers + 1"
          "  WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing UPDATE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing END failed");
        goto rollback;
    }

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_unsub(uint64_t uid, uint64_t chan_id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "BEGIN";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing BEGIN failed");
        return -1;
    }

    /* ================================= stmt-sep ================================= */
    sql = "DELETE FROM subscriptions "
          "  WHERE user_id = :uid AND channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing DELETE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "UPDATE channels "
          "  SET subscribers = subscribers - 1"
          "  WHERE channel_id = :channel_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing UPDATE failed");
        goto rollback;
    }

    /* ================================= stmt-sep ================================= */
    sql = "END";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        goto rollback;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing END failed");
        goto rollback;
    }

    return 0;

rollback:
    sql = "ROLLBACK";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return -1;
}

int db_upsert_user(const UserInfo *ui, uint64_t *uid)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "INSERT INTO users(did, name, email) VALUES (:did, :name, :email) "
          "  ON CONFLICT (did) "
          "  DO UPDATE "
          "       SET name = :name, email = :email "
          "       WHERE excluded.name IS NOT name OR excluded.email IS NOT email";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":did"),
                           ui->did, -1, NULL);
    if (rc) {
        vlogE("Binding parameter did failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":name"),
                           ui->name, -1, NULL);
    if (rc) {
        vlogE("Binding parameter name failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":email"),
                           ui->email, -1, NULL);
    if (rc) {
        vlogE("Binding parameter email failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        vlogE("Executing INSERT failed");
        return -1;
    }

    /* ================================= stmt-sep ================================= */
    sql = "SELECT user_id FROM users WHERE did = :did";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":did"),
                           ui->did, -1, NULL);
    if (rc) {
        vlogE("Binding parameter did failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    *uid = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    return 0;
}

typedef enum {
    CHANNEL,
    POST,
    COMMENT
} QueryObject;

static
const char *query_column(QueryObject qo, QryFld qf)
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
                    return NULL;
            }
        case UPD_AT:
            return "updated_at";
        case CREATED_AT:
            return "created_at";
        default:
            assert(0);
            return NULL;
    }
}

static
void it_dtor(void *obj)
{
    DBObjIt *it = obj;

    if (it->stmt)
        sqlite3_finalize(it->stmt);
}

static
DBObjIt *it_create(sqlite3_stmt *stmt, Row2Raw cb)
{
    DBObjIt *it = rc_zalloc(sizeof(DBObjIt), it_dtor);
    if (!it)
        return NULL;

    it->stmt = stmt;
    it->cb   = cb;

    return it;
}

static
void *row2chan(sqlite3_stmt *stmt)
{
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    const char *intro = (const char *)sqlite3_column_text(stmt, 2);
    size_t avatar_sz = sqlite3_column_int64(stmt, 8);
    ChanInfo *ci = rc_zalloc(sizeof(ChanInfo) + strlen(name) + strlen(intro) + 2 + avatar_sz, NULL);
    void *buf;

    if (!ci) {
        vlogE("OOM");
        return NULL;
    }

    buf = ci + 1;

    ci->chan_id      = sqlite3_column_int64(stmt, 0);
    ci->name         = strcpy(buf, name);
    buf += strlen(name) + 1;
    ci->intro        = strcpy(buf, intro);
    buf += strlen(intro) + 1;
    ci->subs         = sqlite3_column_int64(stmt, 3);
    ci->next_post_id = sqlite3_column_int64(stmt, 4);
    ci->upd_at       = sqlite3_column_int64(stmt, 5);
    ci->created_at   = sqlite3_column_int64(stmt, 6);
    ci->owner        = &feeds_owner_info;
    ci->avatar       = memcpy(buf, sqlite3_column_blob(stmt, 7), avatar_sz);
    ci->len          = avatar_sz;

    return ci;
}

DBObjIt *db_iter_chans(const QryCriteria *qc)
{
    sqlite3_stmt *stmt;
    const char *qcol;
    char sql[1024];
    DBObjIt *it;
    int rc;

    /* ================================= stmt-sep ================================= */
    rc = sprintf(sql,
                 "SELECT channel_id, name, intro, subscribers, "
                 "       next_post_id, updated_at, created_at, avatar, length(avatar) "
                 "  FROM channels");
    if (qc->by) {
        qcol = query_column(CHANNEL, qc->by);
        if (qc->lower || qc->upper)
            rc += sprintf(sql + rc, " WHERE ");
        if (qc->lower)
            rc += sprintf(sql + rc, "%s >= :lower", qcol);
        if (qc->upper)
            rc += sprintf(sql + rc, "%s %s <= :upper", qc->lower ? " AND" : "", qcol);
        rc += sprintf(sql + rc, " ORDER BY %s %s", qcol, qc->by == ID ? "ASC" : "DESC");
    }
    if (qc->maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return NULL;
    }

    if (qc->lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                qc->lower);
        if (rc) {
            vlogE("Binding parameter lower failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                qc->upper);
        if (rc) {
            vlogE("Binding parameter upper failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                qc->maxcnt);
        if (rc) {
            vlogE("Binding parameter maxcnt failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    it = it_create(stmt, row2chan);
    if (!it) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    return it;
}

static
void *row2subchan(sqlite3_stmt *stmt)
{
    const char *name = (const char *)sqlite3_column_text(stmt, 1);
    const char *intro = (const char *)sqlite3_column_text(stmt, 2);
    size_t avatar_sz = sqlite3_column_int64(stmt, 6);
    ChanInfo *ci = rc_zalloc(sizeof(ChanInfo) + strlen(name) + strlen(intro) + 2 + avatar_sz, NULL);
    void *buf;

    if (!ci) {
        vlogE("OOM");
        return NULL;
    }

    buf = ci + 1;
    ci->chan_id = sqlite3_column_int64(stmt, 0);
    ci->name    = strcpy(buf, name);
    buf += strlen(name) + 1;
    ci->intro   = strcpy(buf, intro);
    buf += strlen(intro) + 1;
    ci->subs    = sqlite3_column_int64(stmt, 3);
    ci->upd_at  = sqlite3_column_int64(stmt, 4);
    ci->owner   = &feeds_owner_info;
    ci->avatar  = memcpy(buf, sqlite3_column_blob(stmt, 5), avatar_sz);
    ci->len     = avatar_sz;

    return ci;
}

DBObjIt *db_iter_sub_chans(uint64_t uid, const QryCriteria *qc)
{
    sqlite3_stmt *stmt;
    const char *qcol;
    char sql[1024];
    DBObjIt *it;
    int rc;

    /* ================================= stmt-sep ================================= */
    rc = sprintf(sql,
                 "SELECT channel_id, name, intro, subscribers, updated_at, avatar, length(avatar) "
                 "  FROM (SELECT channel_id "
                 "          FROM subscriptions "
                 "          WHERE user_id = :uid) JOIN channels USING (channel_id)");
    if (qc->by) {
        qcol = query_column(CHANNEL, qc->by);
        if (qc->lower || qc->upper)
            rc += sprintf(sql + rc, " WHERE ");
        if (qc->lower)
            rc += sprintf(sql + rc, "%s >= :lower", qcol);
        if (qc->upper)
            rc += sprintf(sql + rc, "%s %s <= :upper", qc->lower ? " AND" : "", qcol);
        rc += sprintf(sql + rc, " ORDER BY %s %s", qcol, qc->by == ID ? "ASC" : "DESC");
    }
    if (qc->maxcnt)
        rc += sprintf(sql +rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return NULL;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        return NULL;
    }

    if (qc->lower) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":lower"),
                                qc->lower);
        if (rc) {
            vlogE("Binding parameter lower failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->upper) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":upper"),
                                qc->upper);
        if (rc) {
            vlogE("Binding parameter upper failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->maxcnt) {
        rc = sqlite3_bind_int64(stmt,
                                sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                qc->maxcnt);
        if (rc) {
            vlogE("Binding parameter maxcnt");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    it = it_create(stmt, row2subchan);
    if (!it) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    return it;
}


static
void *row2post(sqlite3_stmt *stmt)
{
    size_t len = sqlite3_column_int64(stmt, 3);
    PostInfo *pi = rc_zalloc(sizeof(PostInfo) + len, NULL);
    void *buf;

    if (!pi) {
        vlogE("OOM");
        return NULL;
    }

    pi->chan_id    = sqlite3_column_int64(stmt, 0);
    pi->post_id    = sqlite3_column_int64(stmt, 1);
    buf = pi + 1;
    pi->content    = memcpy(buf, sqlite3_column_blob(stmt, 2), len);
    pi->len        = len;
    pi->cmts       = sqlite3_column_int64(stmt, 4);
    pi->likes      = sqlite3_column_int64(stmt, 5);
    pi->created_at = sqlite3_column_int64(stmt, 6);

    return pi;
}

DBObjIt *db_iter_posts(uint64_t chan_id, const QryCriteria *qc)
{
    sqlite3_stmt *stmt;
    const char *qcol;
    char sql[1024];
    DBObjIt *it;
    int rc;

    /* ================================= stmt-sep ================================= */
    rc = sprintf(sql,
                 "SELECT channel_id, post_id, content, length(content), "
                 "       next_comment_id - 1 AS comments, likes, created_at "
                 "  FROM posts "
                 "  WHERE channel_id = :channel_id");
    if (qc->by) {
        qcol = query_column(POST, qc->by);
        if (qc->lower)
            rc += sprintf(sql + rc, " AND %s >= :lower", qcol);
        if (qc->upper)
            rc += sprintf(sql + rc, " AND %s <= :upper", qcol);
        rc += sprintf(sql + rc, " ORDER BY %s %s", qcol, qc->by == ID ? "ASC" : "DESC");
    }
    if (qc->maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return NULL;
    }

    rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        return NULL;
    }

    if (qc->lower) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":lower"),
                                qc->lower);
        if (rc) {
            vlogE("Binding parameter lower failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->upper) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":upper"),
                                qc->upper);
        if (rc) {
            vlogE("Binding parameter upper failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->maxcnt) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                qc->maxcnt);
        if (rc) {
            vlogE("Binding parameter maxcnt failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    it = it_create(stmt, row2post);
    if (!it) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    return it;
}

DBObjIt *db_iter_liked_posts(uint64_t uid, const QryCriteria *qc)
{
    sqlite3_stmt *stmt;
    const char *qcol;
    char sql[1024];
    DBObjIt *it;
    int rc;

    /* ================================= stmt-sep ================================= */
    rc = sprintf(sql,
                 "SELECT channel_id, post_id, content, length(content), "
                 "       next_comment_id - 1 AS comments, likes, created_at "
                 "  FROM (SELECT channel_id, post_id "
                 "          FROM likes "
                 "          WHERE user_id = :uid AND comment_id = 0) JOIN "
                 "       posts USING (channel_id, post_id)");
    if (qc->by) {
        qcol = query_column(POST, qc->by);
        if (qc->lower || qc->upper)
            rc += sprintf(sql + rc, " WHERE ");
        if (qc->lower)
            rc += sprintf(sql + rc, "%s >= :lower", qcol);
        if (qc->upper)
            rc += sprintf(sql + rc, "%s %s <= :upper", qc->lower ? " AND" : "", qcol);
        rc += sprintf(sql + rc, " ORDER BY %s %s", qcol, qc->by == ID ? "ASC" : "DESC");
    }
    if (qc->maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return NULL;
    }

    rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        return NULL;
    }

    if (qc->lower) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":lower"),
                                qc->lower);
        if (rc) {
            vlogE("Binding parameter lower failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->upper) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":upper"),
                                qc->upper);
        if (rc) {
            vlogE("Binding parameter upper failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->maxcnt) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                qc->maxcnt);
        if (rc) {
            vlogE("Binding parameter maxcnt failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    it = it_create(stmt, row2post);
    if (!it) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    return it;
}

static
void *row2cmt(sqlite3_stmt *stmt)
{
    size_t content_len = sqlite3_column_int64(stmt, 6);
    const char *name = (const char *)sqlite3_column_text(stmt, 4);
    CmtInfo *ci = rc_zalloc(sizeof(CmtInfo) + content_len + strlen(name) + 1, NULL);
    void *buf;

    if (!ci) {
        vlogE("OOM");
        return NULL;
    }

    buf = ci + 1;

    ci->chan_id      = sqlite3_column_int64(stmt, 0);
    ci->post_id      = sqlite3_column_int64(stmt, 1);
    ci->cmt_id       = sqlite3_column_int64(stmt, 2);
    ci->reply_to_cmt = sqlite3_column_int64(stmt, 3);
    ci->user.name    = strcpy(buf, name);
    buf += strlen(name) + 1;
    ci->content      = memcpy(buf, sqlite3_column_blob(stmt, 5), content_len);
    ci->len          = content_len;
    ci->likes        = sqlite3_column_int64(stmt, 7);
    ci->created_at   = sqlite3_column_int64(stmt, 8);

    return ci;
}

DBObjIt *db_iter_cmts(uint64_t chan_id, uint64_t post_id, const QryCriteria *qc)
{
    sqlite3_stmt *stmt;
    const char *qcol;
    char sql[1024];
    DBObjIt *it;
    int rc;

    /* ================================= stmt-sep ================================= */
    rc = sprintf(sql,
                 "SELECT channel_id, post_id, comment_id, refcomment_id, "
                 "       name, content, length(content), likes, created_at "
                 "  FROM comments JOIN users USING (user_id) "
                 "  WHERE channel_id = :channel_id AND post_id = :post_id");
    if (qc->by) {
        qcol = query_column(COMMENT, qc->by);
        if (qc->lower)
            rc += sprintf(sql + rc, " AND %s >= :lower", qcol);
        if (qc->upper)
            rc += sprintf(sql + rc, " AND %s <= :upper", qcol);
        rc += sprintf(sql + rc, " ORDER BY %s %s", qcol, qc->by == ID ? "ASC" : "DESC");
    }
    if (qc->maxcnt)
        rc += sprintf(sql + rc, " LIMIT :maxcnt");

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return NULL;
    }

    rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        return NULL;
    }

    rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":post_id"),
                            post_id);
    if (rc) {
        vlogE("Binding parameter post_id failed");
        sqlite3_finalize(stmt);
        return NULL;
    }

    if (qc->lower) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":lower"),
                                qc->lower);
        if (rc) {
            vlogE("Binding parameter lower failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->upper) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":upper"),
                                qc->upper);
        if (rc) {
            vlogE("Binding parameter upper failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    if (qc->maxcnt) {
        rc = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":maxcnt"),
                                qc->maxcnt);
        if (rc) {
            vlogE("Binding parameter maxcnt failed");
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    it = it_create(stmt, row2cmt);

    return it;
}

int db_iter_nxt(DBObjIt *it, void **obj)
{
    int rc;

    rc = sqlite3_step(it->stmt);
    if (rc != SQLITE_ROW) {
        if (rc != SQLITE_DONE)
            vlogE("sqlite3_step() failed");
        *obj = NULL;
        return rc == SQLITE_DONE ? 1 : -1;
    }

    *obj = it->cb(it->stmt);
    return *obj ? 0 : -1;
}

int db_is_suber(uint64_t uid, uint64_t chan_id)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT EXISTS("
          "  SELECT * "
          "  FROM subscriptions "
          "  WHERE user_id = :uid AND channel_id = :channel_id"
          ")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":uid"),
                            uid);
    if (rc) {
        vlogE("Binding parameter uid failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":channel_id"),
                            chan_id);
    if (rc) {
        vlogE("Binding parameter channel_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return rc;
}

void db_deinit()
{
    sqlite3_close(db);
    sqlite3_shutdown();
}

static
void dbuinfo_dtor(void *obj)
{
    DBUserInfo *ui = obj;

    if (ui->stmt)
        sqlite3_finalize(ui->stmt);
}

int db_get_owner(UserInfo **ui)
{
    sqlite3_stmt *stmt;
    const char *sql;
    DBUserInfo *tmp;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT did, name, email FROM users WHERE user_id = :owner_user_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_int64(stmt,
                            sqlite3_bind_parameter_index(stmt, ":owner_user_id"),
                            OWNER_USER_ID);
    if (rc) {
        vlogE("Binding parameter owner_uiser_id failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        *ui = NULL;
        return 0;
    }

    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    tmp = rc_zalloc(sizeof(DBUserInfo), dbuinfo_dtor);
    if (!tmp) {
        vlogE("OOM");
        sqlite3_finalize(stmt);
        return -1;
    }

    tmp->info.uid   = OWNER_USER_ID;
    tmp->info.did   = (char *)sqlite3_column_text(stmt, 0);
    tmp->info.name  = (char *)sqlite3_column_text(stmt, 1);
    tmp->info.email = (char *)sqlite3_column_text(stmt, 2);
    tmp->stmt       = stmt;

    *ui = &tmp->info;

    return 0;
}

int db_need_upsert_user(const char *did)
{
    sqlite3_stmt *stmt;
    const char *sql;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT EXISTS(SELECT * FROM users WHERE did = :did AND name != \"NA\")";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":did"),
                           did, -1, NULL);
    if (rc) {
        vlogE("Binding parameter did failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    return rc ? 0 : 1;
}

int db_get_user(const char *did, UserInfo **ui)
{
    sqlite3_stmt *stmt;
    const char *sql;
    DBUserInfo *tmp;
    int rc;

    /* ================================= stmt-sep ================================= */
    sql = "SELECT user_id, did, name, email FROM users WHERE did = :did;";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc) {
        vlogE("sqlite3_prepare_v2() failed");
        return -1;
    }

    rc = sqlite3_bind_text(stmt,
                           sqlite3_bind_parameter_index(stmt, ":did"),
                           did, -1, NULL);
    if (rc) {
        vlogE("Binding parameter did failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        *ui = NULL;
        return 0;
    }

    if (rc != SQLITE_ROW) {
        vlogE("Executing SELECT failed");
        sqlite3_finalize(stmt);
        return -1;
    }

    tmp = rc_zalloc(sizeof(DBUserInfo), dbuinfo_dtor);
    if (!tmp) {
        vlogE("OOM");
        sqlite3_finalize(stmt);
        return -1;
    }

    tmp->info.uid   = sqlite3_column_int64(stmt, 0);
    tmp->info.did   = (char *)sqlite3_column_text(stmt, 1);
    tmp->info.name  = (char *)sqlite3_column_text(stmt, 2);
    tmp->info.email = (char *)sqlite3_column_text(stmt, 3);
    tmp->stmt       = stmt;

    *ui = &tmp->info;

    return 0;
}
