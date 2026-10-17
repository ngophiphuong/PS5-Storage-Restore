/* Copyright (c) 2026 NGÔ PHI PHƯƠNG x PSVIETHOA.COM
 * SPDX-License-Identifier: GPL-3.0-only */
#ifndef RESTORE_NATIVE_H
#define RESTORE_NATIVE_H
#include <sqlite3.h>

/* A title registered through the nullfs alias ends up in appinfo.db with
 * _org_path pointing at /user/app/<id>, a path that only exists while the alias
 * is mounted. The alias does not survive a reboot and nothing recreates it, so
 * the next boot reports the data as corrupt until Recovery runs again.
 *
 * A title installed by the package installer records the real device path and
 * _m2_device_id=1 instead, and keeps working across reboots untouched. No
 * AppInstUtil entry point rewrites those fields for a title that is already
 * registered: AppInstallTitleDir accepts the device base, returns zero and
 * changes nothing, which was confirmed by calling it and then rebooting.
 * Writing the two fields directly is what works - verified on a PS5 Pro on
 * firmware 10.01, where all twenty M.2 titles kept the device path across a
 * power cycle and launched from it with no payload run at boot.
 *
 * This writes those two fields and NOTHING else. Earlier builds also removed
 * the alias mount point and copied icon files into it; both were additions that
 * had never been shown to be needed, and both broke titles that were working.
 * The alias is left mounted and /user/app/<id> is left exactly as found. */

#define NATIVE_MAX 64
#define NATIVE_DB "/system_data/priv/mms/appinfo.db"

typedef struct {
    char id[16];
    char source[192];   /* /mnt/extN/user/app/<id> - the real device path */
} NativeEntry;

static NativeEntry native_list[NATIVE_MAX];
static unsigned native_count, native_applied, native_kept;

static void native_remember(const char *id, const char *source) {
    if(native_count >= NATIVE_MAX) return;
    NativeEntry *e = &native_list[native_count++];
    snprintf(e->id, sizeof(e->id), "%s", id);
    snprintf(e->source, sizeof(e->source), "%s", source);
}

/* Byte copy of the database next to the param backups, so a bad result can be
 * put back by hand. Returns 0 when the copy could not be completed. */
static int native_backup(char *out, size_t cap) {
    if(!directory(STATE "/backups")) return 0;
    int n = snprintf(out, cap, STATE "/backups/appinfo-%d.db", getpid());
    if(n < 0 || (size_t)n >= cap) { errno = ENAMETOOLONG; return 0; }
    FILE *src = fopen(NATIVE_DB, "rb");
    if(!src) return 0;
    int fd = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0) { fclose(src); return 0; }
    unsigned char *buf = malloc(RESTORE_CHUNK);
    int good = buf != NULL;
    uint64_t total = 0;
    while(good) {
        size_t got = fread(buf, 1, RESTORE_CHUNK, src);
        if(!got) { if(ferror(src)) good = 0; break; }
        size_t pos = 0;
        while(pos < got) {
            ssize_t k = write(fd, buf + pos, got - pos);
            if(k < 0 && errno == EINTR) continue;
            if(k <= 0) { good = 0; break; }
            pos += (size_t)k;
        }
        total += got;
    }
    free(buf);
    if(good && fsync(fd)) good = 0;
    if(close(fd) && good) good = 0;
    fclose(src);
    if(good) fprintf(logfile, "DB_BACKUP %s bytes=%llu\n", out, (unsigned long long)total);
    else unlink(out);
    return good;
}

static int native_integrity(sqlite3 *db) {
    sqlite3_stmt *st = NULL;
    if(sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &st, NULL) != SQLITE_OK) return 0;
    int good = 0;
    if(sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(st, 0);
        good = text && !strcmp((const char *)text, "ok");
        fprintf(logfile, "DB_INTEGRITY %s\n", text ? (const char *)text : "(null)");
    }
    sqlite3_finalize(st);
    return good;
}

/* Reads one key back as text so the stored value can be compared exactly. */
static int native_read(sqlite3 *db, const char *id, const char *key, char *out, size_t cap) {
    sqlite3_stmt *st = NULL;
    if(sqlite3_prepare_v2(db, "SELECT val FROM tbl_appinfo WHERE titleId=? AND key=?", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, key, -1, SQLITE_STATIC);
    int good = 0;
    if(sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(st, 0);
        if(text) { snprintf(out, cap, "%s", (const char *)text); good = 1; }
    }
    sqlite3_finalize(st);
    return good;
}

/* Row probe used before deciding whether a title needs AppInst at all. Returns
 * 1 with org filled when appinfo already has the title, 0 when it does not, and
 * -1 when the database could not be read. */
static int native_row_state(const char *id, char *org, size_t cap) {
    sqlite3 *db = NULL;
    if(sqlite3_open_v2(NATIVE_DB, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if(db) sqlite3_close(db);
        return -1;
    }
    sqlite3_busy_timeout(db, 5000);
    int state = native_read(db, id, "_org_path", org, cap) ? 1 : 0;
    sqlite3_close(db);
    return state;
}

static int native_update_text(sqlite3 *db, const char *id, const char *key, const char *value) {
    sqlite3_stmt *st = NULL;
    if(sqlite3_prepare_v2(db, "UPDATE tbl_appinfo SET val=? WHERE titleId=? AND key=?", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(st, 1, value, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, key, -1, SQLITE_STATIC);
    int done = sqlite3_step(st) == SQLITE_DONE;
    int rows = sqlite3_changes(db);
    sqlite3_finalize(st);
    return done && rows == 1;
}

/* _m2_device_id is stored as an integer on this firmware; binding an int keeps
 * the storage class identical to a package-installer row. */
static int native_update_int(sqlite3 *db, const char *id, const char *key, int value) {
    sqlite3_stmt *st = NULL;
    if(sqlite3_prepare_v2(db, "UPDATE tbl_appinfo SET val=? WHERE titleId=? AND key=?", -1, &st, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_int(st, 1, value);
    sqlite3_bind_text(st, 2, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, key, -1, SQLITE_STATIC);
    int done = sqlite3_step(st) == SQLITE_DONE;
    int rows = sqlite3_changes(db);
    sqlite3_finalize(st);
    return done && rows == 1;
}

static void native_apply(void) {
    if(!native_count) return;
    char backup[320] = {0};
    if(!native_backup(backup, sizeof(backup))) {
        fprintf(logfile, "NATIVE_ABORT cannot back up appinfo.db errno=%d\n", errno);
        return;
    }
    sqlite3 *db = NULL;
    if(sqlite3_open_v2(NATIVE_DB, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        fprintf(logfile, "NATIVE_ABORT cannot open appinfo.db: %s\n", db ? sqlite3_errmsg(db) : "?");
        if(db) sqlite3_close(db);
        return;
    }
    sqlite3_busy_timeout(db, 5000);
    if(!native_integrity(db)) {
        fprintf(logfile, "NATIVE_ABORT integrity check failed before any write\n");
        sqlite3_close(db);
        return;
    }
    for(unsigned i = 0; i < native_count; i++) {
        NativeEntry *e = &native_list[i];
        char current[256] = {0};
        if(!native_read(db, e->id, "_org_path", current, sizeof(current))) {
            fprintf(logfile, "NATIVE_SKIP %s no _org_path row\n", e->id);
            native_kept++; continue;
        }
        if(!strcmp(current, e->source)) {
            fprintf(logfile, "NATIVE_ALREADY %s %s\n", e->id, e->source);
        } else if(!native_update_text(db, e->id, "_org_path", e->source)) {
            fprintf(logfile, "NATIVE_FAILED %s _org_path: %s\n", e->id, sqlite3_errmsg(db));
            native_kept++; continue;
        }
        if(!native_update_int(db, e->id, "_m2_device_id", 1))
            fprintf(logfile, "NATIVE_WARN %s _m2_device_id not updated: %s\n", e->id, sqlite3_errmsg(db));
        char check[256] = {0};
        if(!native_read(db, e->id, "_org_path", check, sizeof(check)) || strcmp(check, e->source)) {
            fprintf(logfile, "NATIVE_FAILED %s readback mismatch got=%s\n", e->id, check);
            native_kept++; continue;
        }
        fprintf(logfile, "NATIVE %s _org_path=%s _m2_device_id=1\n", e->id, e->source);
        native_applied++;
        fflush(logfile);
    }
    int ok = native_integrity(db);
    if(sqlite3_close(db) != SQLITE_OK)
        fprintf(logfile, "NATIVE_WARN database did not close cleanly\n");
    if(!ok) {
        fprintf(logfile, "NATIVE_RESTORE integrity failed after writing; put %s back by hand\n", backup);
        return;
    }
    /* The alias stays mounted and /user/app/<id> is left untouched: this run
     * only changes where appinfo says the data lives. The alias becomes
     * redundant at the next boot, when the device path takes over by itself. */
    fprintf(logfile, "NATIVE_SUMMARY applied=%u kept=%u backup=%s\n", native_applied, native_kept, backup);
}
#endif
