/*
 * Donna SQLite FFI — wraps the SQLite 3 amalgamation.
 *
 * Wire format for column values (one value per donna_sqlite_column call):
 *   N            — NULL
 *   I<digits>    — INTEGER (signed decimal)
 *   F<decimal>   — FLOAT (%.17g)
 *   T<utf8>      — TEXT (raw bytes; tag byte is always 'T', rest is the value)
 *   B<hex>       — BLOB (lowercase hex pairs)
 *
 * No terminator byte: each call returns exactly one tagged value, so the
 * tag byte alone disambiguates type and the Donna side slices the rest.
 *
 * Error handling: functions that can fail return 0/1 or -1.  The global
 * error message can be retrieved with donna_sqlite_last_error().
 */

#include "sqlite3.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static char g_error[1024] = "";

static void set_error(const char *msg) {
    if (msg) {
        strncpy(g_error, msg, sizeof(g_error) - 1);
        g_error[sizeof(g_error) - 1] = '\0';
    } else {
        g_error[0] = '\0';
    }
}

/* Returns a pointer to the last error message string (static storage). */
const char *donna_sqlite_last_error(void) {
    return g_error;
}

/* Open a database at path.  Returns a handle (non-zero) or 0 on error. */
intptr_t donna_sqlite_open(const char *path) {
    sqlite3 *db = NULL;
    int rc;
    if (!path || path[0] == '\0') path = ":memory:";
    rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        set_error(db ? sqlite3_errmsg(db) : "out of memory");
        if (db) sqlite3_close(db);
        return 0;
    }
    g_error[0] = '\0';
    return (intptr_t)db;
}

/* Close a database. */
long donna_sqlite_close(intptr_t handle) {
    sqlite3 *db = (sqlite3 *)handle;
    if (db) sqlite3_close(db);
    return 0;
}

/* Execute one or more SQL statements with no result rows.
 * Returns 0 on success, 1 on error (see donna_sqlite_last_error). */
long donna_sqlite_exec(intptr_t handle, const char *sql) {
    sqlite3 *db = (sqlite3 *)handle;
    char *errmsg = NULL;
    int rc;
    if (!db) { set_error("invalid database handle"); return 1; }
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        set_error(errmsg ? errmsg : sqlite3_errmsg(db));
        if (errmsg) sqlite3_free(errmsg);
        return 1;
    }
    g_error[0] = '\0';
    return 0;
}

/* Prepare a SQL statement.  Returns a handle or 0 on error. */
intptr_t donna_sqlite_prepare(intptr_t handle, const char *sql) {
    sqlite3 *db = (sqlite3 *)handle;
    sqlite3_stmt *stmt = NULL;
    int rc;
    if (!db) { set_error("invalid database handle"); return 0; }
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        set_error(sqlite3_errmsg(db));
        return 0;
    }
    g_error[0] = '\0';
    return (intptr_t)stmt;
}

/* Bind a text value to parameter idx (1-based).  Returns 0 ok, 1 error. */
long donna_sqlite_bind_text(intptr_t stmt_handle, long idx, const char *value) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    int rc;
    if (!stmt) { set_error("invalid statement handle"); return 1; }
    rc = sqlite3_bind_text(stmt, (int)idx, value, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) { set_error(sqlite3_errmsg(sqlite3_db_handle(stmt))); return 1; }
    return 0;
}

/* Bind an integer value to parameter idx (1-based).  Returns 0 ok, 1 error. */
long donna_sqlite_bind_int(intptr_t stmt_handle, long idx, long value) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    int rc;
    if (!stmt) { set_error("invalid statement handle"); return 1; }
    rc = sqlite3_bind_int64(stmt, (int)idx, (sqlite3_int64)value);
    if (rc != SQLITE_OK) { set_error(sqlite3_errmsg(sqlite3_db_handle(stmt))); return 1; }
    return 0;
}

/* Bind a float value to parameter idx (1-based).  Returns 0 ok, 1 error. */
long donna_sqlite_bind_float(intptr_t stmt_handle, long idx, double value) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    int rc;
    if (!stmt) { set_error("invalid statement handle"); return 1; }
    rc = sqlite3_bind_double(stmt, (int)idx, value);
    if (rc != SQLITE_OK) { set_error(sqlite3_errmsg(sqlite3_db_handle(stmt))); return 1; }
    return 0;
}

/* Bind NULL to parameter idx (1-based).  Returns 0 ok, 1 error. */
long donna_sqlite_bind_null(intptr_t stmt_handle, long idx) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    int rc;
    if (!stmt) { set_error("invalid statement handle"); return 1; }
    rc = sqlite3_bind_null(stmt, (int)idx);
    if (rc != SQLITE_OK) { set_error(sqlite3_errmsg(sqlite3_db_handle(stmt))); return 1; }
    return 0;
}

/* Advance to the next row.
 * Returns: 1 = row ready, 0 = done, -1 = error. */
long donna_sqlite_step(intptr_t stmt_handle) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    int rc;
    if (!stmt) { set_error("invalid statement handle"); return -1; }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)  { g_error[0] = '\0'; return 1; }
    if (rc == SQLITE_DONE) { g_error[0] = '\0'; return 0; }
    set_error(sqlite3_errmsg(sqlite3_db_handle(stmt)));
    return -1;
}

/* Return the number of columns in the result set. */
long donna_sqlite_column_count(intptr_t stmt_handle) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    if (!stmt) return 0;
    return (long)sqlite3_column_count(stmt);
}

/* Return a tagged wire string for column idx (0-based).
 * Caller owns the returned heap string. */
char *donna_sqlite_column(intptr_t stmt_handle, long idx) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    int col = (int)idx;
    char num[64];

    if (!stmt) return strdup("N");

    switch (sqlite3_column_type(stmt, col)) {
    case SQLITE_NULL:
        return strdup("N");

    case SQLITE_INTEGER: {
        sqlite3_int64 v = sqlite3_column_int64(stmt, col);
        snprintf(num, sizeof(num), "I%lld", (long long)v);
        return strdup(num);
    }

    case SQLITE_FLOAT: {
        double v = sqlite3_column_double(stmt, col);
        snprintf(num, sizeof(num), "F%.17g", v);
        return strdup(num);
    }

    case SQLITE_TEXT: {
        const char *text = (const char *)sqlite3_column_text(stmt, col);
        int blen = sqlite3_column_bytes(stmt, col);
        char *result;
        if (!text) return strdup("N");
        result = malloc((size_t)blen + 2);
        if (!result) return strdup("N");
        result[0] = 'T';
        memcpy(result + 1, text, (size_t)blen);
        result[blen + 1] = '\0';
        return result;
    }

    case SQLITE_BLOB: {
        const unsigned char *blob = sqlite3_column_blob(stmt, col);
        int blen = sqlite3_column_bytes(stmt, col);
        char *result;
        int i;
        if (!blob || blen == 0) return strdup("B");
        result = malloc((size_t)blen * 2 + 2);
        if (!result) return strdup("N");
        result[0] = 'B';
        for (i = 0; i < blen; i++) {
            snprintf(result + 1 + i * 2, 3, "%02x", blob[i]);
        }
        result[blen * 2 + 1] = '\0';
        return result;
    }

    default:
        return strdup("N");
    }
}

/* Return the name of column idx (0-based).  Caller owns the returned string. */
char *donna_sqlite_column_name(intptr_t stmt_handle, long idx) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    const char *name;
    if (!stmt) return strdup("");
    name = sqlite3_column_name(stmt, (int)idx);
    return strdup(name ? name : "");
}

/* Release a prepared statement. */
long donna_sqlite_finalize(intptr_t stmt_handle) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    if (stmt) sqlite3_finalize(stmt);
    return 0;
}

/* Reset a prepared statement for re-execution. */
long donna_sqlite_reset(intptr_t stmt_handle) {
    sqlite3_stmt *stmt = (sqlite3_stmt *)stmt_handle;
    if (stmt) sqlite3_reset(stmt);
    return 0;
}

/* Return the rowid of the last successful INSERT. */
long donna_sqlite_last_insert_rowid(intptr_t handle) {
    sqlite3 *db = (sqlite3 *)handle;
    if (!db) return 0;
    return (long)sqlite3_last_insert_rowid(db);
}

/* Return the number of rows changed by the last DML statement. */
long donna_sqlite_changes(intptr_t handle) {
    sqlite3 *db = (sqlite3 *)handle;
    if (!db) return 0;
    return (long)sqlite3_changes(db);
}
