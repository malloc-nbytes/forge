/*
 * forge: Forge your own packages
 * Copyright (C) 2025  malloc-nbytes
 * Contact: zdhdev@yahoo.com

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#include "sqlite3.h"

#include "forge/forge.h"
#include "depgraph.h"
#include "flags.h"
#include "utils.h"
#include "colors.h"
#define CIO_IMPL
#include "cio.h"
#define CLAP_IMPL
#include "clap.h"

#define FORGE_C_MODULE_TEMPLATE \
        "#include <forge/forge.h>\n" \
        "\n" \
        "char *deps[] = {NULL}; // Must be NULL terminated\n" \
        "\n" \
        "char *getname(void) { return \"mypackage\"; }\n" \
        "char *getver(void) { return \"1.0.0\"; }\n" \
        "char *getdesc(void) { return \"My Description\"; }\n" \
        "char **getdeps(void) { return deps; }\n" \
        "char *download(void) {\n" \
        "        return NULL; // should return the name of the final directory!\n" \
        "}\n" \
        "void build(void) {}\n" \
        "void install(void) {}\n" \
        "void uninstall(void) {}\n" \
        "void update(void) {\n" \
        "        return 0; // return 1 if it needs a rebuild, 0 otherwise\n" \
        "}\n" \
        "\n" \
        "FORGE_GLOBAL pkg package = {\n" \
        "        .name = getname,\n" \
        "        .ver = getver,\n" \
        "        .desc = getdesc,\n" \
        "        .deps = NULL,\n" \
        "        .download = download,\n" \
        "        .build = build,\n" \
        "        .install = install,\n" \
        "        .uninstall = uninstall,\n" \
        "        .update = forge_pkg_git_update, // or define your own if not using git\n" \
        "};"

#define DB_DIR "/var/lib/forge/"
#define DB_FP DB_DIR "forge.db"

#define C_MODULE_DIR "/usr/src/forge/modules/"
#define C_MODULE_DIR_PARENT "/usr/src/forge/"
#define MODULE_LIB_DIR "/usr/lib/forge/modules/"

#define PKG_SOURCE_DIR "/var/cache/forge/sources/"

#define CHECK_SQLITE(rc, db)                                            \
        do {                                                            \
                if (rc != SQLITE_OK) {                                  \
                        fprintf(stderr, "SQLite error: %s\n",           \
                                sqlite3_errmsg(db));                    \
                        sqlite3_close(db);                              \
                        exit(1);                                        \
                }                                                       \
        } while (0)

DYN_ARRAY_TYPE(void *, handle_array);
DYN_ARRAY_TYPE(pkg *, pkg_ptr_array);

typedef struct {
        sqlite3 *db;
        struct {
                handle_array handles; // should be same len as paths
                str_array paths;      // should be same len as handles
        } dll;
        depgraph dg;
        pkg_ptr_array pkgs;
} forge_context;

typedef struct {
        char *name;
        char *version;
        char *description;
        int installed;
} pkg_info;

DYN_ARRAY_TYPE(pkg_info, pkg_info_array);

struct {
        uint32_t flags;
} g_config = {
        .flags = 0x00000000,
};

char *
get_filename_from_dir(char *path)
{
        char *res = path;
        int skip = 0;
        for (size_t i = 0; path[i]; ++i) {
                if (path[i] == '\\') {
                        skip = 1;
                        continue;
                }
                else if (path[i] == '/' && path[i+1] && !skip) {
                        res = (path+i+1);
                }
                skip = 0;
        }
        return res;
}

str_array
get_dirs_in_dir(const char *fp)
{
        str_array res = dyn_array_empty(str_array);
        DIR *dir = opendir(fp);
        if (!dir) {
                fprintf(stderr, "Failed to open directory %s: %s\n", fp, strerror(errno));
                return res;
        }

        struct dirent *entry;
        struct stat st;
        char full_path[512] = {0};

        while ((entry = readdir(dir))) {
                // Skip "." and ".." entries
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                        continue;
                }

                snprintf(full_path, sizeof(full_path), "%s/%s", fp, entry->d_name);

                if (stat(full_path, &st) == -1) {
                        fprintf(stderr, "Failed to stat %s: %s\n", full_path, strerror(errno));
                        continue;
                }

                // Check if it's a directory
                if (S_ISDIR(st.st_mode)) {
                        char *dirname = strdup(entry->d_name);
                        if (!dirname) {
                                fprintf(stderr, "Failed to allocate memory for %s\n", entry->d_name);
                                continue;
                        }
                        dyn_array_append(res, dirname);
                }
        }

        closedir(dir);
        return res;
}

str_array
get_files_in_dir(const char *fp)
{
        str_array res = dyn_array_empty(str_array);
        DIR *dir = opendir(fp);
        if (!dir) {
                fprintf(stderr, "Failed to open directory %s: %s\n", fp, strerror(errno));
                return res;
        }

        struct dirent *entry;
        struct stat st;
        char full_path[512] = {0};

        while ((entry = readdir(dir))) {
                // Skip "." and ".." entries
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                        continue;
                }

                snprintf(full_path, sizeof(full_path), "%s/%s", fp, entry->d_name);

                if (stat(full_path, &st) == -1) {
                        fprintf(stderr, "Failed to stat %s: %s\n", full_path, strerror(errno));
                        continue;
                }

                // Check if it's a regular file
                if (S_ISREG(st.st_mode)) {
                        char *filename = strdup(entry->d_name);
                        if (!filename) {
                                fprintf(stderr, "Failed to allocate memory for %s\n", entry->d_name);
                                continue;
                        }
                        dyn_array_append(res, filename);
                }
        }

        closedir(dir);
        return res;
}

// Create directory and all parent directories, `mkdir -p`.
int
mkdir_p(const char *path, mode_t mode)
{
        if (!path || !*path) return -1;

        // Try to create the directory
        if (mkdir(path, mode) == 0 || errno == EEXIST) {
                return 0;
        }

        // If the error is not "parent doesn't exist", return failure
        if (errno != ENOENT) {
                return -1;
        }

        // Find the last '/' to get the parent directory
        char *parent = strdup(path);
        if (!parent) return -1;

        char *last_slash = strrchr(parent, '/');
        if (!last_slash || last_slash == parent) {
                free(parent);
                return -1;
        }

        *last_slash = '\0'; // Terminate at the parent path

        // Recursively create parent directories
        int result = mkdir_p(parent, mode);
        free(parent);

        if (result != 0) return -1;

        // Try creating the directory again
        return mkdir(path, mode) == 0 || errno == EEXIST ? 0 : -1;
}

sqlite3 *
init_db(const char *dbname)
{
        sqlite3 *db;
        int rc = sqlite3_open(dbname, &db);
        CHECK_SQLITE(rc, db);

        rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
        CHECK_SQLITE(rc, db);

        const char *create_pkgs =
                "CREATE TABLE IF NOT EXISTS Pkgs ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "name TEXT NOT NULL UNIQUE,"
                "version TEXT NOT NULL,"
                "description TEXT,"
                "installed INTEGER NOT NULL DEFAULT 0,"
                "pkg_src_loc TEXT);";
        rc = sqlite3_exec(db, create_pkgs, NULL, NULL, NULL);
        CHECK_SQLITE(rc, db);

        const char *create_deps =
                "CREATE TABLE IF NOT EXISTS Deps ("
                "pkg_id INTEGER NOT NULL,"
                "dep_id INTEGER NOT NULL,"
                "FOREIGN KEY (pkg_id) REFERENCES Pkgs(id),"
                "FOREIGN KEY (dep_id) REFERENCES Pkgs(id),"
                "PRIMARY KEY (pkg_id, dep_id));";
        rc = sqlite3_exec(db, create_deps, NULL, NULL, NULL);
        CHECK_SQLITE(rc, db);

        return db;
}

int
get_pkg_id(forge_context *ctx, const char *name)
{
        sqlite3_stmt *stmt;
        const char *sql = "SELECT id FROM Pkgs WHERE name = ?;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        int id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                id = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return id;
}

int
pkg_is_installed(forge_context *ctx, pkg *pkg)
{
        return get_pkg_id(ctx, pkg->name()) != -1;
}

void
add_dep_to_db(forge_context *ctx,
              int            pkg_id,
              int            dep_id)
{
        sqlite3_stmt *stmt;
        const char *sql = "INSERT OR IGNORE INTO Deps (pkg_id, dep_id) VALUES (?, ?);";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_int(stmt, 1, pkg_id);
        sqlite3_bind_int(stmt, 2, dep_id);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Dependency insert error: %s\n", sqlite3_errmsg(ctx->db));
        }

        sqlite3_finalize(stmt);
}

void
register_pkg(forge_context *ctx, pkg *pkg)
{
        if (!pkg->name) {
                err("register_pkg(): pkg (unknown) does not have a name");
        }
        if (!pkg->ver) {
                err_wargs("register_pkg(): pkg %s does not have a version", pkg->name());
        }
        if (!pkg->desc) {
                err_wargs("register_pkg(): pkg %s does not have a description", pkg->name());
        }

        const char *name = pkg->name();
        const char *ver = pkg->ver();
        const char *desc = pkg->desc();

        sqlite3_stmt *stmt;
        const char *sql_select = "SELECT id FROM Pkgs WHERE name = ?;";
        int rc = sqlite3_prepare_v2(ctx->db, sql_select, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        int id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (id != -1) {
                // Update existing package
                //printf(GREEN BOLD "*** Updating package registration: %s\n" RESET, name);

                //const char *sql_update = "UPDATE Pkgs SET version = ?, description = ?, installed = 1 WHERE name = ?;";
                const char *sql_update = "UPDATE Pkgs SET version = ?, description = ? WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, ver, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                // Clear existing dependencies to avoid foreign key issues
                /* const char *sql_delete = "DELETE FROM Deps WHERE pkg_id = ?;"; */
                /* rc = sqlite3_prepare_v2(ctx->db, sql_delete, -1, &stmt, NULL); */
                /* CHECK_SQLITE(rc, ctx->db); */

                /* sqlite3_bind_int(stmt, 1, id); */
                /* rc = sqlite3_step(stmt); */
                /* if (rc != SQLITE_DONE) { */
                /*         fprintf(stderr, "Delete dependencies error: %s\n", sqlite3_errmsg(ctx->db)); */
                /* } */
                /* sqlite3_finalize(stmt); */
        } else {
                // New package
                printf(GREEN BOLD "*** Registered package: %s\n" RESET, name);

                const char *sql_insert = "INSERT INTO Pkgs (name, version, description, installed) VALUES (?, ?, ?, 0);";
                int rc = sqlite3_prepare_v2(ctx->db, sql_insert, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, ver, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, desc, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Insert error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                if (pkg->deps) {
                        char **deps = pkg->deps();
                        for (size_t i = 0; deps[i]; ++i) {
                                printf(GREEN BOLD "*** Adding dependency %s for %s\n" RESET, deps[i], name);
                                add_dep_to_db(ctx, get_pkg_id(ctx, name), get_pkg_id(ctx, deps[i]));
                        }
                }
        }
}

void
list_deps(forge_context *ctx, const char *pkg_name)
{
        (void)ctx;

        sqlite3 *db;
        int rc = sqlite3_open_v2(DB_FP, &db, SQLITE_OPEN_READONLY, NULL);
        CHECK_SQLITE(rc, db);

        sqlite3_stmt *stmt;
        const char *sql =
                "SELECT Pkgs.name, Pkgs.version, Pkgs.description, Pkgs.installed "
                "FROM Deps "
                "JOIN Pkgs ON Deps.dep_id = Pkgs.id "
                "WHERE Deps.pkg_id = (SELECT id FROM Pkgs WHERE name = ?);";
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

        sqlite3_bind_text(stmt, 1, pkg_name, -1, SQLITE_STATIC);

        // Collect data and calculate max widths
        pkg_info_array rows = dyn_array_empty(pkg_info_array);
        size_t max_name_len = strlen("Name");
        size_t max_version_len = strlen("Version");
        size_t max_installed_len = strlen("Installed");
        size_t max_desc_len = strlen("Description");

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                pkg_info info = {0};
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                const char *version = (const char *)sqlite3_column_text(stmt, 1);
                const char *description = (const char *)sqlite3_column_text(stmt, 2);
                int installed = sqlite3_column_int(stmt, 3);

                info.name = strdup(name ? name : "");
                info.version = strdup(version ? version : "");
                info.description = strdup(description ? description : "(none)");
                info.installed = installed;

                max_name_len = MAX(max_name_len, strlen(info.name));
                max_version_len = MAX(max_version_len, strlen(info.version));
                max_desc_len = MAX(max_desc_len, strlen(info.description));
                max_installed_len = MAX((int)max_installed_len, snprintf(NULL, 0, "%d", installed));

                dyn_array_append(rows, info);
        }

        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        // Print header
        printf("Dependencies for package '%s':\n", pkg_name);
        printf("%-*s  %-*s  %*s  %-*s\n",
               (int)max_name_len, "Name",
               (int)max_version_len, "Version",
               (int)max_installed_len, "Installed",
               (int)max_desc_len, "Description");
        printf("%-*s  %-*s  %*s  %-*s\n",
               (int)max_name_len, "----",
               (int)max_version_len, "-------",
               (int)max_installed_len, "---------",
               (int)max_desc_len, "-----------");

        // Print rows
        if (rows.len == 0) {
                printf("No dependencies found for package '%s'.\n", pkg_name);
        } else {
                for (size_t i = 0; i < rows.len; ++i) {
                        pkg_info *info = &rows.data[i];
                        printf("%-*s  %-*s  %*d  %-*s\n",
                               (int)max_name_len, info->name,
                               (int)max_version_len, info->version,
                               (int)max_installed_len, info->installed,
                               (int)max_desc_len, info->description);
                }
        }

        // Clean up
        for (size_t i = 0; i < rows.len; ++i) {
                free(rows.data[i].name);
                free(rows.data[i].version);
                free(rows.data[i].description);
        }
        dyn_array_free(rows);
}

void
list_registerd_pkgs(forge_context *ctx)
{
        (void)ctx;

        sqlite3 *db;
        int rc = sqlite3_open_v2(DB_FP, &db, SQLITE_OPEN_READONLY, NULL);
        CHECK_SQLITE(rc, db);

        sqlite3_stmt *stmt;
        const char *sql = "SELECT name, version, description, installed FROM Pkgs;";
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

        // Collect data and calculate max widths
        pkg_info_array rows = dyn_array_empty(pkg_info_array);
        size_t max_name_len = strlen("Name");
        size_t max_version_len = strlen("Version");
        size_t max_installed_len = strlen("Installed");
        size_t max_desc_len = strlen("Description");

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                pkg_info info = {0};
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                const char *version = (const char *)sqlite3_column_text(stmt, 1);
                const char *description = (const char *)sqlite3_column_text(stmt, 2);
                int installed = sqlite3_column_int(stmt, 3);

                info.name = strdup(name ? name : "");
                info.version = strdup(version ? version : "");
                info.description = strdup(description ? description : "(none)");
                info.installed = installed;

                max_name_len = MAX(max_name_len, strlen(info.name));
                max_version_len = MAX(max_version_len, strlen(info.version));
                max_desc_len = MAX(max_desc_len, strlen(info.description));
                max_installed_len = MAX((int)max_installed_len, snprintf(NULL, 0, "%d", installed));

                dyn_array_append(rows, info);
        }

        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        // Print header
        printf("Available packages:\n");
        printf("%-*s  %-*s  %*s  %-*s\n",
               (int)max_name_len, "Name",
               (int)max_version_len, "Version",
               (int)max_installed_len, "Installed",
               (int)max_desc_len, "Description");
        printf("%-*s  %-*s  %*s  %-*s\n",
               (int)max_name_len, "----",
               (int)max_version_len, "-------",
               (int)max_installed_len, "---------",
               (int)max_desc_len, "-----------");

        // Print rows
        if (rows.len == 0) {
                printf("No packages found in the database.\n");
        } else {
                for (size_t i = 0; i < rows.len; ++i) {
                        pkg_info *info = &rows.data[i];
                        printf("%-*s  %-*s  %*d  %-*s\n",
                               (int)max_name_len, info->name,
                               (int)max_version_len, info->version,
                               (int)max_installed_len, info->installed,
                               (int)max_desc_len, info->description);
                }
        }

        // Clean up
        for (size_t i = 0; i < rows.len; ++i) {
                free(rows.data[i].name);
                free(rows.data[i].version);
                free(rows.data[i].description);
        }
        dyn_array_free(rows);
}

void
construct_depgraph(forge_context *ctx)
{
        for (size_t i = 0; i < ctx->pkgs.len; ++i) {
                // TODO: assert name
                depgraph_insert_pkg(&ctx->dg, ctx->pkgs.data[i]->name());
        }

        for (size_t i = 0; i < ctx->pkgs.len; ++i) {
                if (!ctx->pkgs.data[i]->deps) { continue; }

                char *name = ctx->pkgs.data[i]->name();
                char **deps = ctx->pkgs.data[i]->deps();
                for (size_t j = 0; deps[j]; ++j) {
                        depgraph_add_dep(&ctx->dg, name, deps[j]);
                }
        }
}

void
uninstall_pkg(forge_context *ctx,
              str_array     *names)
{
        for (size_t i = 0; i < names->len; ++i) {
                const char *name = names->data[i];
                printf(GREEN BOLD "*** Uninstalling package %s [%zu of %zu]\n" RESET, name, i+1, names->len);
                fflush(stdout);
                sleep(1);

                pkg *pkg = NULL;
                char *pkg_src_loc = NULL;
                int pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        err_wargs("unregistered package `%s`", name);
                }
                for (size_t i = 0; i < ctx->pkgs.len; ++i) {
                        if (!strcmp(ctx->pkgs.data[i]->name(), name)) {
                                pkg = ctx->pkgs.data[i];
                                break;
                        }
                }
                assert(pkg);

                // Retrieve pkg_src_loc from database
                sqlite3_stmt *stmt;
                const char *sql_select = "SELECT pkg_src_loc FROM Pkgs WHERE name = ?;";
                int rc = sqlite3_prepare_v2(ctx->db, sql_select, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *src_loc = (const char *)sqlite3_column_text(stmt, 0);
                        if (src_loc) {
                                pkg_src_loc = strdup(src_loc);
                        }
                }
                sqlite3_finalize(stmt);

                char base[256] = {0};

                if (pkg_src_loc) {
                        const char *pkgname = get_filename_from_dir(pkg_src_loc);
                        snprintf(base, sizeof(base), "%s%s", PKG_SOURCE_DIR, pkgname);

                        // Check if directory exists
                        struct stat st;
                        if (stat(base, &st) == -1 || !S_ISDIR(st.st_mode)) {
                                fprintf(stderr, "Could not find source code, please reinstall\n");
                                return;
                        }
                } else {
                        fprintf(stderr, "Could not find source code, please reinstall\n");
                        return;
                }

                // Change to package directory
                if (!cd(base)) {
                        fprintf(stderr, "aborting...\n");
                        free(pkg_src_loc);
                        return;
                }

                // Perform uninstall
                info_minor("Performing: pkg->uninstall()", 1);
                pkg->uninstall();

                // Update installed status in database
                sql_select = "SELECT id FROM Pkgs WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_select, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                int id = -1;
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                        id = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);

                const char *sql_update = "UPDATE Pkgs SET installed = 0 WHERE id = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_int(stmt, 1, id);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                free(pkg_src_loc);
        }
}

int
install_pkg(forge_context *ctx,
            str_array     *names,
            int            is_dep)
{
        for (size_t i = 0; i < names->len; ++i) {
                const char *name = names->data[i];
                if (is_dep) {
                        printf(GREEN BOLD "*** Installing dependency %s [%zu of %zu]\n" RESET,
                               name, i+1, names->len);
                } else {
                        printf(GREEN BOLD "*** Installing package %s [%zu of %zu]\n" RESET,
                               name, i+1, names->len);
                }
                fflush(stdout);
                sleep(1);

                pkg *pkg = NULL;
                char *pkg_src_loc = NULL;
                int pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        err_wargs("unregistered package `%s`", name);
                }
                for (size_t i = 0; i < ctx->pkgs.len; ++i) {
                        if (!strcmp(ctx->pkgs.data[i]->name(), name)) {
                                pkg = ctx->pkgs.data[i];
                                break;
                        }
                }
                assert(pkg);

                // Install deps
                if (pkg->deps) {
                        good_minor("installing dependencies", 1);
                        info_minor("performing: pkg->deps()", 1);
                        char **deps = pkg->deps();
                        str_array depnames = dyn_array_empty(str_array);
                        for (size_t i = 0; deps[i]; ++i) {
                                dyn_array_append(depnames, deps[i]);
                        }
                        if (!install_pkg(ctx, &depnames, 1)) {
                                err_wargs("could not install package %s, stopping...\n", names->data[i]);
                                return 0;
                        }
                }

                sqlite3_stmt *stmt;
                const char *sql_select = "SELECT pkg_src_loc FROM Pkgs WHERE name = ?;";
                int rc = sqlite3_prepare_v2(ctx->db, sql_select, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *src_loc = (const char *)sqlite3_column_text(stmt, 0);
                        if (src_loc) {
                                pkg_src_loc = strdup(src_loc);
                        }
                }
                sqlite3_finalize(stmt);

                if (!cd(PKG_SOURCE_DIR)) {
                        fprintf(stderr, "aborting...\n");
                        return 0;
                }

                const char *pkgname = NULL;

                if (pkg_src_loc) {
                        pkgname = get_filename_from_dir(pkg_src_loc);
                }
                else {
                        info_minor("Performing: pkg->download()", 1);
                        pkgname = pkg->download();
                }

                if (!cd_silent(pkgname)) {
                        pkg->download();
                        if (!cd(pkgname)) {
                                fprintf(stderr, "aborting...\n");
                                return 0;
                        }
                }

                char base[256] = {0};
                sprintf(base, PKG_SOURCE_DIR "%s", pkgname);

                info_minor("Performing: pkg->build()", 1);
                pkg->build();
                if (!cd(base)) {
                        fprintf(stderr, "aborting...\n");
                        return 0;
                }
                info_minor("Performing: pkg->install()", 1);
                pkg->install();

                // Update pkg_src_loc in database
                const char *sql_update = "UPDATE Pkgs SET pkg_src_loc = ? WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, base, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update pkg_src_loc error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                // Update the installed flag
                sql_select = "SELECT id FROM Pkgs WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_select, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                int id = -1;
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                        id = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);

                sql_update = "UPDATE Pkgs SET installed = 1 WHERE id = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                // Bind package id
                sqlite3_bind_int(stmt, 1, id);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);
        }

        return 1;
}

void
obtain_handles_and_pkgs_from_dll(forge_context *ctx)
{
        DIR *dir = opendir(MODULE_LIB_DIR);
        if (!dir) {
                perror("Failed to open package directory");
                return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir))) {
                if (strstr(entry->d_name, ".so")) {
                        char *path = (char *)malloc(512);
                        snprintf(path, 512, "%s%s", MODULE_LIB_DIR, entry->d_name);

                        void *handle = dlopen(path, RTLD_LAZY);
                        if (!handle) {
                                fprintf(stderr, "Error loading dll path: `%s`, %s\n", path, dlerror());
                                return;
                        }

                        dlerror();
                        pkg *pkg = dlsym(handle, "package");
                        char *error = dlerror();
                        if (error != NULL) {
                                fprintf(stderr, "Error finding 'package' symbol in %s: %s\n", path, error);
                                dlclose(handle);
                                return;
                        }

                        dyn_array_append(ctx->dll.handles, handle);
                        dyn_array_append(ctx->dll.paths, path);
                        dyn_array_append(ctx->pkgs, pkg);
                }
        }

        closedir(dir);
}

void
cleanup_forge_context(forge_context *ctx)
{
        sqlite3_close(ctx->db);
        for (size_t i = 0; i < ctx->dll.handles.len; ++i) {
                dlclose(ctx->dll.handles.data[i]);
                free(ctx->dll.paths.data[i]);
        }
        dyn_array_free(ctx->dll.handles);
        dyn_array_free(ctx->dll.paths);
        dyn_array_free(ctx->pkgs);
        depgraph_destroy(&ctx->dg);
}

void
assert_sudo(void)
{
        if (geteuid() != 0) {
                err("This action requires sudo privileges");
        }
}

void
rebuild_pkgs(forge_context *ctx)
{
        (void)ctx;

        good_major("Rebuilding package modules", 1);

        DIR *dir = opendir(C_MODULE_DIR);
        if (!dir) {
                perror("Failed to open directory");
                return;
        }

        str_array files = dyn_array_empty(str_array);
        struct dirent *entry;
        while ((entry = readdir(dir))) {
                // Check if the file name ends with ".c"
                if (entry->d_type == DT_REG && strstr(entry->d_name, ".c") != NULL) {
                        size_t len = strlen(entry->d_name);
                        if (len >= 2 && strcmp(entry->d_name + len - 2, ".c") == 0) {
                                // Allocate memory for the filename without ".c"
                                char *filename = strdup(entry->d_name);
                                filename[len - 2] = '\0'; // Remove ".c" by null-terminating early
                                dyn_array_append(files, filename);
                        }
                }
        }

        if (!cd_silent(C_MODULE_DIR)) {
                fprintf(stderr, "aborting...\n");
                goto cleanup;
        }

        str_array passed = dyn_array_empty(str_array),
                failed = dyn_array_empty(str_array);
        for (size_t i = 0; i < files.len; ++i) {
                char buf[256] = {0};
                sprintf(buf, "gcc -Wextra -Wall -Werror -shared -fPIC %s.c -lforge -L/usr/local/lib -o" MODULE_LIB_DIR "%s.so -I../include",
                        files.data[i], files.data[i]);
                printf("%s\n", buf);
                fflush(stdout);
                int status = system(buf);
                if (status == -1) {
                        perror("system");
                        dyn_array_append(failed, files.data[i]);
                } else {
                        if (WIFEXITED(status)) {
                                printf("\033[1A");
                                printf("\033[2K");
                                int exit_status = WEXITSTATUS(status);
                                if (exit_status != 0) {
                                        fflush(stdout);
                                        fprintf(stderr, INVERT BOLD RED "In module %s:\n" RESET, files.data[i]);
                                        fprintf(stderr, INVERT BOLD RED "  located in: " MODULE_LIB_DIR "%s.c\n" RESET, files.data[i]);
                                        fprintf(stderr, INVERT BOLD RED "  use:\n" RESET);
                                        fprintf(stderr, INVERT BOLD RED "    forge edit %s\n" RESET, files.data[i]);
                                        fprintf(stderr, INVERT BOLD RED "  to fix your errors!\n" RESET);
                                        fprintf(stderr, BOLD YELLOW "  skipping %s module compilation...\n" RESET, files.data[i]);
                                        dyn_array_append(failed, files.data[i]);
                                } else {
                                        dyn_array_append(passed, files.data[i]);
                                }
                        } else {
                                fprintf(stdout, INVERT BOLD RED "program did not exit normally\n" RESET);
                                dyn_array_append(failed, files.data[i]);
                        }
                }
        }

        if (failed.len > 0) {
                printf("Results: [ " BOLD GREEN "%zu Passed" RESET ", " BOLD RED "%zu Failed" RESET " ]\n", passed.len, failed.len);
        } else {
                printf("Results: [ " BOLD GREEN "%zu Passed" RESET " ]\n", passed.len);
        }

 cleanup:
        dyn_array_free(passed);
        dyn_array_free(failed);
        for (size_t i = 0; i < files.len; ++i) { free(files.data[i]); }
        dyn_array_free(files);
        closedir(dir);
}

void
init_env(void)
{
        // Database location
        if (mkdir(DB_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", DB_DIR, strerror(errno));
        }
        sqlite3 *db = init_db(DB_FP);
        if (!db) {
                fprintf(stderr, "could not initialize database: %s\n", DB_FP);
        }
        sqlite3_close(db);

        // Module locations
        if (mkdir_p(MODULE_LIB_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", DB_DIR, strerror(errno));
        }
        if (mkdir_p(C_MODULE_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", DB_DIR, strerror(errno));
        }
        if (!cd(C_MODULE_DIR_PARENT)) {
                fprintf(stderr, "could cd to path: %s, %s\n", C_MODULE_DIR_PARENT, strerror(errno));
        }
        if (!cmd("git clone https://www.github.com/malloc-nbytes/forge-modules.git/ ./modules")) {
                fprintf(stderr, "could not git clone forge-modules: %s\n", strerror(errno));
        }

        // Pkg source location
        if (mkdir_p(PKG_SOURCE_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", DB_DIR, strerror(errno));
        }
}

void
new_pkg(forge_context *ctx, str_array *names)
{
        (void)ctx;

        for (size_t i = 0; i < names->len; ++i) {
                char fp[256] = {0};
                sprintf(fp, C_MODULE_DIR "%s.c", names->data[i]);
                if (cio_file_exists(fp)) {
                        err_wargs("file %s already exists", fp);
                }
                if (!cio_write_file(fp, FORGE_C_MODULE_TEMPLATE)) {
                        err_wargs("failed to write to file %s, %s", fp, strerror(errno));
                }

                // Open in Vim
                char cmd[512] = {0};
                snprintf(cmd, sizeof(cmd), "vim %s", fp);
                if (system(cmd) == -1) {
                        fprintf(stderr, "Failed to open %s in Vim: %s\n", fp, strerror(errno));
                }
        }
}

void
edit_c_module(forge_context *ctx, str_array *names)
{
        (void)ctx;

        for (size_t i = 0; i < names->len; ++i) {
                char fp[256] = {0};
                sprintf(fp, C_MODULE_DIR "%s.c", names->data[i]);
                if (!cio_file_exists(fp)) {
                        err_wargs("C module %s does not exist", fp);
                }

                char cmd[512] = {0};
                snprintf(cmd, sizeof(cmd), "vim %s", fp);
                if (system(cmd) == -1) {
                        fprintf(stderr, "Failed to open %s in Vim: %s\n", fp, strerror(errno));
                }
        }
}

void
update_pkgs(forge_context *ctx, str_array *names)
{
        // If no names provided, update all installed packages
        str_array pkg_names;
        if (names->len == 0) {
                pkg_names = dyn_array_empty(str_array);
                sqlite3_stmt *stmt;
                const char *sql = "SELECT name FROM Pkgs WHERE installed = 1;";
                int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *name = (const char *)sqlite3_column_text(stmt, 0);
                        dyn_array_append(pkg_names, strdup(name));
                }
                sqlite3_finalize(stmt);
        } else {
                pkg_names = *names;
        }

        for (size_t i = 0; i < pkg_names.len; ++i) {
                const char *name = pkg_names.data[i];

                // Check if package is installed
                sqlite3_stmt *stmt;
                const char *sql = "SELECT installed, pkg_src_loc FROM Pkgs WHERE name = ?;";
                int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                int installed = 0;
                char *pkg_src_loc = NULL;

                if (sqlite3_step(stmt) == SQLITE_ROW) {
                        installed = sqlite3_column_int(stmt, 0);
                        const char *src_loc = (const char *)sqlite3_column_text(stmt, 1);
                        if (src_loc) {
                                pkg_src_loc = strdup(src_loc);
                        }
                }
                sqlite3_finalize(stmt);

                if (!installed) {
                        printf(YELLOW "*** Skipping update for %s: not installed\n" RESET, name);
                        free(pkg_src_loc);
                        continue;
                }

                printf(GREEN BOLD "*** Updating package %s [%zu of %zu]\n" RESET, name, i+1, pkg_names.len);
                fflush(stdout);
                sleep(1);

                // Find the package
                pkg *pkg = NULL;
                for (size_t j = 0; j < ctx->pkgs.len; ++j) {
                        if (!strcmp(ctx->pkgs.data[j]->name(), name)) {
                                pkg = ctx->pkgs.data[j];
                                break;
                        }
                }
                if (!pkg) {
                        err_wargs("package %s not found in loaded modules", name);
                        free(pkg_src_loc);
                        continue;
                }

                // Navigate to source directory
                if (!pkg_src_loc) {
                        fprintf(stderr, "No source location found for %s, please reinstall\n", name);
                        continue;
                }

                const char *pkgname = get_filename_from_dir(pkg_src_loc);
                char base[256] = {0};
                snprintf(base, sizeof(base), "%s%s", PKG_SOURCE_DIR, pkgname);

                if (!cd(base)) {
                        fprintf(stderr, "Could not access source directory for %s, please reinstall\n", name);
                        free(pkg_src_loc);
                        continue;
                }

                int updated = 0;

                // Perform update
                info_minor("Performing: pkg->update()", 1);
                if (pkg->update) {
                        updated = pkg->update();
                } else {
                        printf(YELLOW "*** No update function defined for %s, skipping update step\n" RESET, name);
                }

                // Update dependencies and reinstall package
                str_array install_names = dyn_array_empty(str_array);
                dyn_array_append(install_names, strdup(name));

                // Add dependencies to install list
                if (pkg->deps) {
                        good_minor("queueing dependencies for reinstall", 1);
                        char **deps = pkg->deps();
                        for (size_t j = 0; deps[j]; ++j) {
                                // Check if dependency is installed before adding
                                sql = "SELECT installed FROM Pkgs WHERE name = ?;";
                                rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                                CHECK_SQLITE(rc, ctx->db);

                                sqlite3_bind_text(stmt, 1, deps[j], -1, SQLITE_STATIC);
                                if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0)) {
                                        dyn_array_append(install_names, strdup(deps[j]));
                                }
                                sqlite3_finalize(stmt);
                        }
                }

                // Reinstall package and its dependencies
                if (updated) {
                        if (!install_pkg(ctx, &install_names, 0)) {
                                fprintf(stderr, "Failed to reinstall %s and its dependencies\n", name);
                        }
                } else {
                        good_major("Up-to-date", 1);
                }

                // Clean up install_names
                for (size_t j = 0; j < install_names.len; ++j) {
                        free(install_names.data[j]);
                }
                dyn_array_free(install_names);

                // Update version and description in database
                sql = "UPDATE Pkgs SET version = ?, description = ? WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, pkg->ver(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, pkg->desc(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update package info error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                free(pkg_src_loc);
        }

        // Clean up if we created the names array
        if (names->len == 0) {
                for (size_t i = 0; i < pkg_names.len; ++i) {
                        free(pkg_names.data[i]);
                }
                dyn_array_free(pkg_names);
        }
}

void
dump_module(const forge_context *ctx,
            const str_array     *names)
{
        (void)ctx;

        for (size_t i = 0; i < names->len; ++i) {
                printf(GREEN BOLD "*** Dumping package %s [%zu of %zu]\n" RESET,
                               names->data[i], i+1, names->len);
                char fp[256] = {0};
                sprintf(fp, C_MODULE_DIR "%s.c", names->data[i]);
                if (!cio_file_exists(fp)) {
                        fprintf(stderr, BOLD RED "C module %s does not exist\n" RESET, names->data[i]);
                        continue;
                }
                size_t ret_len = 0;
                char **lines = cio_file_to_lines_wnewlines(fp, &ret_len);
                for (size_t j = 0; j < ret_len; ++j) {
                        printf("  %zu: %s", j, lines[j]);
                        free(lines[j]);
                }
                free(lines);
        }
}

void
sync(void)
{
        good_major("Pulling changes", 1);
        if (!cd_silent(C_MODULE_DIR)) {
                fprintf(stderr, "could cd to path: %s, %s\n", C_MODULE_DIR, strerror(errno));
                return;
        }
        if (!cmd("git fetch origin && git pull origin main")) {
                fprintf(stderr, "could not sync: %s\n", strerror(errno));
        }
}

void
clearln(void)
{
        fflush(stdout);
        printf("\033[1A");
        printf("\033[2K");
}

int
main(int argc, char **argv)
{
        ++argv, --argc;
        clap_init(argc, argv);
        int exists = cio_file_exists(DB_FP);

        if (exists && argc == 0) {
                usage();
        } else if (!exists) {
                assert_sudo();
                init_env();
                return 0;
        }

        sqlite3 *db = init_db(DB_FP);

        forge_context ctx = (forge_context) {
                .db = db,
                .dll = {
                        .handles = dyn_array_empty(handle_array),
                        .paths = dyn_array_empty(str_array),
                },
                .dg = depgraph_create(),
                .pkgs = dyn_array_empty(pkg_ptr_array),
        };

        // Load existing .so files and packages
        obtain_handles_and_pkgs_from_dll(&ctx);
        construct_depgraph(&ctx);
        size_t_array indices = depgraph_gen_order(&ctx.dg);

        Clap_Arg arg = {0};
        while (clap_next(&arg)) {
                if (arg.hyphc == 1 && arg.start[0] == FLAG_1HY_HELP) {
                        usage();
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_HELP)) {
                        usage();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_LIST)) {
                        list_registerd_pkgs(&ctx);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_DEPS)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires an argument", FLAG_2HY_DEPS);
                        }
                        list_deps(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_INSTALL)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_INSTALL);
                        assert_sudo();
                        install_pkg(&ctx, &names, 0);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_UNINSTALL)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_INSTALL);
                        assert_sudo();
                        uninstall_pkg(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_REBUILD)) {
                        g_config.flags |= FT_REBUILD;
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_NEW)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_NEW);
                        assert_sudo();
                        new_pkg(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_EDIT)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_INSTALL);
                        assert_sudo();
                        edit_c_module(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_UPDATE)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        assert_sudo();
                        update_pkgs(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_DUMP)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_DUMP);
                        dump_module(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_SYNC)) {
                        g_config.flags |= FT_SYNC;
                } else if (arg.hyphc == 1) { // one hyph options
                        for (size_t i = 0; arg.start[i]; ++i) {
                                char c = arg.start[i];
                                switch (c) {
                                case FLAG_1HY_REBUILD: g_config.flags |= FT_REBUILD; break;
                                case FLAG_1HY_SYNC: g_config.flags |= FT_SYNC; break;
                                default: err_wargs("unknown option `%c`", c);
                                }
                        }
                }
                else {
                        err_wargs("unknown flag `%s`", arg.start);
                }
        }

        if (g_config.flags & FT_SYNC) {
                assert_sudo();
                sync();
        }
        if (g_config.flags & FT_REBUILD) {
                assert_sudo();
                // Clean up existing context to avoid stale handles
                for (size_t i = 0; i < ctx.dll.handles.len; ++i) {
                        dlclose(ctx.dll.handles.data[i]);
                        free(ctx.dll.paths.data[i]);
                }
                dyn_array_free(ctx.dll.handles);
                dyn_array_free(ctx.dll.paths);
                dyn_array_free(ctx.pkgs);
                depgraph_destroy(&ctx.dg);

                // Reinitialize context
                ctx.dll.handles = dyn_array_empty(handle_array);
                ctx.dll.paths = dyn_array_empty(str_array);
                ctx.pkgs = dyn_array_empty(pkg_ptr_array);
                ctx.dg = depgraph_create();

                // Rebuild packages and load new .so files
                rebuild_pkgs(&ctx);
                obtain_handles_and_pkgs_from_dll(&ctx);
                construct_depgraph(&ctx);
                indices = depgraph_gen_order(&ctx.dg);

                // Register packages
                for (size_t i = 0; i < indices.len; ++i) {
                        register_pkg(&ctx, ctx.pkgs.data[indices.data[i]]);
                }
        }

        dyn_array_free(indices);
        cleanup_forge_context(&ctx);
        return 0;
}
