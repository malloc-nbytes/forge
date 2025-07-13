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
#include <time.h>
#include <regex.h>

#include "sqlite3.h"

#include "forge/forge.h"
#include "config.h"
#include "depgraph.h"
#include "flags.h"
#include "utils.h"
#include "colors.h"
#include "viewer.h"
#define CIO_IMPL
#include "cio.h"
#define CLAP_IMPL
#include "clap.h"

#define FORGE_C_MODULE_TEMPLATE \
        "#include <forge/forge.h>\n" \
        "\n" \
        "char *deps[] = {NULL}; // Must be NULL terminated\n" \
        "\n" \
        "char *getname(void) { return \"author@pkg_name\"; }\n" \
        "char *getver(void) { return \"1.0.0\"; }\n" \
        "char *getdesc(void) { return \"My Description\"; }\n" \
        "char **getdeps(void) { return deps; }\n" \
        "char *download(void) {\n" \
        "        return NULL; // should return the name of the final directory!\n" \
        "}\n" \
        "void build(void) {}\n" \
        "void install(void) {}\n" \
        "void uninstall(void) {}\n" \
        "int update(void) {\n" \
        "        return 0; // return 1 if it needs a rebuild, 0 otherwise\n" \
        "}\n" \
        "void get_changes(void) {\n" \
        "        // pull in the new changes if update() returns 1\n" \
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
        "         \n" \
        "         // Make this NULL to just re-download the source code\n" \
        "         // or define your own if not using git\n" \
        "        .get_changes = forge_pkg_git_pull,\n" \
        "};"

static const char *common_install_dirs[] = {
        "/usr/bin",
        "/usr/include",
        "/usr/lib",
        "/usr/lib64",
        "/etc",
        "/opt",
        "/usr/share",
        "/usr/local/bin",
        "/usr/local/include",
        "/usr/local/lib64",
        "/usr/local/lib",
        "/usr/local/sbin",
        "/usr/local/share",
};

#define CD(path, block) if (!cd(path)) block;
#define CMD(c, block)   if (!cmd(c))   block;

#define DB_DIR                                    "/var/lib/forge/"
#define DB_FP DB_DIR                              "forge.db"
#define C_MODULE_DIR                              PREFIX "/src/forge/modules/"
#define C_MODULE_USER_DIR                         PREFIX "/src/forge/user_modules/"
#define C_MODULE_DIR_PARENT                       PREFIX "/src/forge/"
#define MODULE_LIB_DIR                            PREFIX "/lib/forge/modules/"
#define PKG_SOURCE_DIR                            "/var/cache/forge/sources/"
#define FORGE_API_HEADER_DIR                      PREFIX "/include/forge"
#define FORGE_CONF_HEADER_FP FORGE_API_HEADER_DIR "/conf.h"

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

str_array
get_absolute_files_in_dir(const char *fp)
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
                //if (S_ISREG(st.st_mode)) {
                char *absolute_path = strdup(full_path);
                if (!absolute_path) {
                        fprintf(stderr, "Failed to allocate memory for %s\n", full_path);
                        continue;
                }
                dyn_array_append(res, absolute_path);
                //}
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

forge_smap
snapshot_files(void)
{
        static int SNAPSHOT_FILES_TRUE = 1;
        forge_smap map = forge_smap_create();
        for (size_t i = 0;
             i < sizeof(common_install_dirs)/sizeof(*common_install_dirs);
             ++i) {
                str_array ar = get_absolute_files_in_dir(common_install_dirs[i]);
                for (size_t j = 0; j < ar.len; ++j) {
                        forge_smap_insert(&map, ar.data[j], &SNAPSHOT_FILES_TRUE);
                        free(ar.data[j]);
                }
                dyn_array_free(ar);
        }
        return map;
}

void
clear_package_files_from_db(forge_context *ctx, const char *name, int pkg_id)
{
        sqlite3_stmt *stmt;
        const char *sql = "DELETE FROM Files WHERE pkg_id = ?;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_int(stmt, 1, pkg_id);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Failed to delete file entries for package %s: %s\n",
                        name, sqlite3_errmsg(ctx->db));
        }
        sqlite3_finalize(stmt);
}

// Create directory and all parent directories, `mkdir -p`.
int
mkdir_p_wmode(const char *path, mode_t mode)
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
        int result = mkdir_p_wmode(parent, mode);
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

        const char *create_files =
                "CREATE TABLE IF NOT EXISTS Files ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "pkg_id INTEGER NOT NULL,"
                "file_path TEXT NOT NULL,"
                "FOREIGN KEY (pkg_id) REFERENCES Pkgs(id),"
                "UNIQUE (pkg_id, file_path));";
        rc = sqlite3_exec(db, create_files, NULL, NULL, NULL);
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

char *
get_c_module_filepath_from_basic_name(const char *name)
{
        char **dirs = ls(C_MODULE_DIR_PARENT);
        for (size_t i = 0; dirs[i]; ++i) {
                char *module_dir = forge_str_builder(C_MODULE_DIR_PARENT, dirs[i], NULL);
                if (forge_io_is_dir(module_dir)) {
                        CD(module_dir, {});
                        char *path = forge_str_builder(module_dir, "/", name, ".c", NULL);
                        if (cio_file_exists(path)) {
                                return path;
                        }
                        free(path);
                }
                free(module_dir);
                free(dirs[i]);
        }
        free(dirs);
        return NULL;
}

void
restore_pkg(forge_context *ctx, const char *name)
{
        (void)ctx;

        // Construct the pattern to match backup files: <name>.c-<timestamp>
        char pattern[256] = {0};
        snprintf(pattern, sizeof(pattern), "%s.c-", name);

        // Get all directories under C_MODULE_DIR_PARENT
        char **dirs = ls(C_MODULE_DIR_PARENT);
        if (!dirs) {
                fprintf(stderr, "Failed to list directories in %s: %s\n", C_MODULE_DIR_PARENT, strerror(errno));
                return;
        }

        char *latest_file = NULL;
        time_t latest_mtime = 0;
        char *target_dir = NULL;

        // Iterate through each directory
        for (size_t i = 0; dirs[i]; ++i) {
                if (!strcmp(dirs[i], ".") || !strcmp(dirs[i], "..")) {
                        free(dirs[i]);
                        continue;
                }

                char *module_dir = forge_str_builder(C_MODULE_DIR_PARENT, dirs[i], NULL);
                if (!forge_io_is_dir(module_dir)) {
                        free(module_dir);
                        free(dirs[i]);
                        continue;
                }

                DIR *dir = opendir(module_dir);
                if (!dir) {
                        fprintf(stderr, "Failed to open directory %s: %s\n", module_dir, strerror(errno));
                        free(module_dir);
                        free(dirs[i]);
                        continue;
                }

                struct dirent *entry;
                struct stat st;

                // Iterate through files in the directory
                while ((entry = readdir(dir))) {
                        // Check if the file matches the pattern
                        if (strncmp(entry->d_name, pattern, strlen(pattern)) == 0) {
                                char full_path[512] = {0};
                                snprintf(full_path, sizeof(full_path), "%s/%s", module_dir, entry->d_name);

                                // Get file modification time
                                if (stat(full_path, &st) == -1) {
                                        fprintf(stderr, "Failed to stat %s: %s\n", full_path, strerror(errno));
                                        continue;
                                }

                                // Update latest file if this one is newer
                                if (st.st_mtime > latest_mtime) {
                                        latest_mtime = st.st_mtime;
                                        if (latest_file) {
                                                free(latest_file);
                                        }
                                        if (target_dir) {
                                                free(target_dir);
                                        }
                                        latest_file = strdup(full_path);
                                        target_dir = strdup(module_dir);
                                }
                        }
                }
                closedir(dir);
                free(module_dir);
                free(dirs[i]);
        }
        free(dirs);

        // Check if a backup file was found
        if (!latest_file || !target_dir) {
                fprintf(stderr, "No backup file found for package %s\n", name);
                return;
        }

        // Construct the original file path
        char original_path[256] = {0};
        snprintf(original_path, sizeof(original_path), "%s/%s.c", target_dir, name);

        // Check if the original file already exists
        if (cio_file_exists(original_path)) {
                fprintf(stderr, "Original file %s already exists, cannot restore\n", original_path);
                free(latest_file);
                free(target_dir);
                return;
        }

        // Rename the latest backup file to the original name
        printf(YELLOW "Restoring C module: %s to %s\n" RESET, latest_file, original_path);
        if (rename(latest_file, original_path) != 0) {
                fprintf(stderr, "Failed to restore file %s to %s: %s\n",
                        latest_file, original_path, strerror(errno));
                free(latest_file);
                free(target_dir);
                return;
        }

        printf(GREEN BOLD "*** Successfully restored package %s\n" RESET, name);
        free(latest_file);
        free(target_dir);
}

void
drop_pkg(forge_context *ctx, const char *name)
{
        // Check if package exists
        int pkg_id = get_pkg_id(ctx, name);
        if (pkg_id == -1) {
                err_wargs("package `%s` not found in database", name);
        }

        sqlite3_stmt *stmt;

        // Delete dependencies associated with the package
        const char *sql_delete_deps = "DELETE FROM Deps WHERE pkg_id = ? OR dep_id = ?;";
        int rc = sqlite3_prepare_v2(ctx->db, sql_delete_deps, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_int(stmt, 1, pkg_id);
        sqlite3_bind_int(stmt, 2, pkg_id);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Failed to delete dependencies for %s: %s\n", name, sqlite3_errmsg(ctx->db));
        }
        sqlite3_finalize(stmt);

        // Delete the package
        const char *sql_delete_pkg = "DELETE FROM Pkgs WHERE id = ?;";
        rc = sqlite3_prepare_v2(ctx->db, sql_delete_pkg, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_int(stmt, 1, pkg_id);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Failed to delete package %s: %s\n", name, sqlite3_errmsg(ctx->db));
        } else {
                printf(GREEN BOLD "*** Successfully dropped package %s from database\n" RESET, name);
        }
        sqlite3_finalize(stmt);

        // Remove .c file
        char *abspath = get_c_module_filepath_from_basic_name(name);
        forge_str pkg_filename = forge_str_from(abspath);
        free(abspath);
        forge_str pkg_new_filename = forge_str_from(forge_str_to_cstr(&pkg_filename));
        char hash[32] = {0};
        snprintf(hash, 32, "%ld", time(NULL));
        forge_str_append(&pkg_new_filename, '-');
        forge_str_concat(&pkg_new_filename, hash);

        printf(YELLOW "creating backup of C module: %s\n" RESET, forge_str_to_cstr(&pkg_new_filename));
        if (rename(forge_str_to_cstr(&pkg_filename), forge_str_to_cstr(&pkg_new_filename)) != 0) {
                fprintf(stderr, "failed to rename file: %s to %s: %s\n",
                        forge_str_to_cstr(&pkg_filename), forge_str_to_cstr(&pkg_new_filename), strerror(errno));
        }

        forge_str_destroy(&pkg_filename);
        forge_str_destroy(&pkg_new_filename);

        // Remove .so file
        forge_str so_path = forge_str_from(MODULE_LIB_DIR);
        forge_str_concat(&so_path, name);
        forge_str_concat(&so_path, ".so");

        printf(YELLOW "removing library file: %s\n" RESET, forge_str_to_cstr(&so_path));
        if (remove(forge_str_to_cstr(&so_path)) != 0) {
                fprintf(stderr, "failed to remove file: %s: %s\n",
                        forge_str_to_cstr(&so_path), strerror(errno));
                //return;
        }

        forge_str_destroy(&so_path);
}

int
pkg_is_registered(forge_context *ctx, pkg *pkg)
{
        return get_pkg_id(ctx, pkg->name()) != -1;
}

int
pkg_is_installed(forge_context *ctx, const char *name)
{
        sqlite3_stmt *stmt;
        const char *sql = "SELECT installed FROM Pkgs WHERE name = ?;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        int installed = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                installed = sqlite3_column_int(stmt, 0);
        } else {
                // Package not found in database
                sqlite3_finalize(stmt);
                return -1;
        }

        sqlite3_finalize(stmt);
        return installed;
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

                clear_package_files_from_db(ctx, name, pkg_id);

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
                        const char *pkgname = forge_io_basename(pkg_src_loc);
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
                printf(GREEN "(%s)->uninstall()\n" RESET, name);
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

void
list_files_recursive(const char *dir_path, str_array *files)
{
        DIR *dir = opendir(dir_path);
        if (!dir) {
                fprintf(stderr, "Failed to open directory %s: %s\n", dir_path, strerror(errno));
                return;
        }

        struct dirent *entry;
        struct stat st;
        char full_path[512] = {0};

        while ((entry = readdir(dir))) {
                // Skip "." and ".." entries
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                        continue;
                }

                snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

                if (stat(full_path, &st) == -1) {
                        fprintf(stderr, "Failed to stat %s: %s\n", full_path, strerror(errno));
                        continue;
                }

                // If it's a regular file, add to the array
                if (S_ISREG(st.st_mode)) {
                        char *file_copy = strdup(full_path);
                        if (!file_copy) {
                                fprintf(stderr, "Failed to allocate memory for %s\n", full_path);
                                continue;
                        }
                        dyn_array_append(*files, file_copy);
                }
                // If it's a directory, recurse
                else if (S_ISDIR(st.st_mode)) {
                        list_files_recursive(full_path, files);
                }
        }

        closedir(dir);
}

void
list_files(forge_context *ctx,
           const char    *name,
           int            pad)
{
        if (!strcmp(name, "forge")) {
                str_array files = dyn_array_empty(str_array);
                list_files_recursive(DB_DIR, &files);
                list_files_recursive(C_MODULE_DIR_PARENT, &files);
                list_files_recursive(FORGE_API_HEADER_DIR, &files);
                list_files_recursive(FORGE_API_HEADER_DIR, &files);
                for (size_t i = 0; i < files.len; ++i) {
                        printf("%s\n", files.data[i]);
                }
                dyn_array_free(files);
                return;
        }

        sqlite3_stmt *stmt;
        const char *sql = "SELECT Files.file_path "
                "FROM Files "
                "JOIN Pkgs ON Files.pkg_id = Pkgs.id "
                "WHERE Pkgs.name = ?;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        // Collect file paths
        str_array files = dyn_array_empty(str_array);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const char *file_path = (const char *)sqlite3_column_text(stmt, 0);
                if (!file_path) {
                        continue;
                }

                struct stat st;
                if (stat(file_path, &st) == -1) {
                        fprintf(stderr, "Failed to stat %s: %s\n", file_path, strerror(errno));
                        continue;
                }

                // If it's a regular file, add directly
                if (S_ISREG(st.st_mode)) {
                        char *file_copy = strdup(file_path);
                        if (!file_copy) {
                                fprintf(stderr, "Failed to allocate memory for %s\n", file_path);
                                continue;
                        }
                        dyn_array_append(files, file_copy);
                }
                // If it's a directory, recursively list all files
                else if (S_ISDIR(st.st_mode)) {
                        list_files_recursive(file_path, &files);
                }
        }

        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(ctx->db));
        }

        sqlite3_finalize(stmt);

        // Print files
        if (files.len == 0) {
                printf("No files found for package '%s'.\n", name);
        } else {
                for (size_t i = 0; i < files.len; ++i) {
                        if (pad) {
                                printf("  ");
                        }
                        printf("%s\n", files.data[i]);
                }
        }

        // Clean up
        for (size_t i = 0; i < files.len; ++i) {
                free(files.data[i]);
        }
        dyn_array_free(files);
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

                if (pkg_is_installed(ctx, name) && is_dep) {
                        printf(YELLOW "dependency %s is already installed\n" RESET, name);
                        return 1;
                }

                // Install deps
                if (pkg->deps) {
                        good_major("installing dependencies", 1);
                        printf(GREEN "(%s)->deps()\n" RESET, name);
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
                        pkgname = forge_io_basename(pkg_src_loc);
                }
                else {
                        printf(GREEN "(%s)->download()\n" RESET, name);
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

                printf(GREEN "(%s)->build()\n" RESET, name);
                pkg->build();
                if (!cd(base)) {
                        fprintf(stderr, "aborting...\n");
                        return 0;
                }

                forge_smap snapshot_before = snapshot_files();

                printf(GREEN "(%s)->install()\n" RESET, name);
                pkg->install();

                forge_smap snapshot_after = snapshot_files();

                str_array diff_files = dyn_array_empty(str_array);
                char **keys = forge_smap_iter(&snapshot_after);
                for (size_t j = 0; keys[j]; ++j) {
                        if (!forge_smap_contains(&snapshot_before, keys[j])) {
                                dyn_array_append(diff_files, strdup(keys[j]));
                        }
                }

                // Ensure pkg_id is available
                pkg_id = -1;
                sqlite3_stmt *stmt_id;
                const char *sql_select_id = "SELECT id FROM Pkgs WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_select_id, -1, &stmt_id, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt_id, 1, name, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt_id) == SQLITE_ROW) {
                        pkg_id = sqlite3_column_int(stmt_id, 0);
                }
                sqlite3_finalize(stmt_id);

                if (pkg_id == -1) {
                        fprintf(stderr, "Failed to find package ID for %s\n", name);
                        // Clean up and return
                        for (size_t j = 0; j < diff_files.len; ++j) {
                                free(diff_files.data[j]);
                        }
                        dyn_array_free(diff_files);
                        forge_smap_destroy(&snapshot_before);
                        forge_smap_destroy(&snapshot_after);
                        return 0;
                }

                // Insert diff_files into the Files table
                sqlite3_stmt *stmt_files;
                const char *sql_insert_file = "INSERT OR IGNORE INTO Files (pkg_id, file_path) VALUES (?, ?);";
                rc = sqlite3_prepare_v2(ctx->db, sql_insert_file, -1, &stmt_files, NULL);
                CHECK_SQLITE(rc, ctx->db);

                for (size_t j = 0; j < diff_files.len; ++j) {
                        sqlite3_bind_int(stmt_files, 1, pkg_id);
                        sqlite3_bind_text(stmt_files, 2, diff_files.data[j], -1, SQLITE_STATIC);

                        rc = sqlite3_step(stmt_files);
                        if (rc != SQLITE_DONE) {
                                fprintf(stderr, "Failed to insert file %s for package %s: %s\n",
                                        diff_files.data[j], name, sqlite3_errmsg(ctx->db));
                        }
                        sqlite3_reset(stmt_files);
                }
                sqlite3_finalize(stmt_files);

                for (size_t j = 0; j < diff_files.len; ++j) {
                        free(diff_files.data[j]);
                }
                dyn_array_free(diff_files);

                forge_smap_destroy(&snapshot_before);
                forge_smap_destroy(&snapshot_after);

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

                printf(PINK ITALIC BOLD "*** Installed:\n" RESET PINK ITALIC);
                list_files(ctx, name, 1);
                printf(RESET);
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

        char **dirs = ls(C_MODULE_DIR_PARENT);
        for (size_t d = 0; dirs[d]; ++d) {
                if (!strcmp(dirs[d], ".") || !strcmp(dirs[d], "..")) {
                        free(dirs[d]);
                        continue;
                }
                char *abspath = forge_str_builder(C_MODULE_DIR_PARENT, dirs[d], NULL);
                DIR *dir = opendir(abspath);
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

                if (!cd_silent(abspath)) {
                        fprintf(stderr, "aborting...\n");
                        goto cleanup;
                }

                str_array passed = dyn_array_empty(str_array),
                        failed = dyn_array_empty(str_array);
                for (size_t i = 0; i < files.len; ++i) {
                        char buf[256] = {0};
                        sprintf(buf, "gcc -Wextra -Wall -Werror -shared -fPIC %s.c -lforge -L/usr/local/lib -o" MODULE_LIB_DIR "%s.so -I../include",
                                files.data[i], files.data[i]);
                        printf("[CC] %s.c\n", files.data[i]);
                        //printf("%s\n", buf);
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
                                                fprintf(stderr, INVERT BOLD RED "    forge -r edit %s\n" RESET, files.data[i]);
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

                const char *basename = forge_io_basename(abspath);
                if (failed.len > 0) {
                        printf(YELLOW "%s:" RESET " [ " BOLD GREEN "%zu Compiled" RESET ", " BOLD RED "%zu Failed" RESET " ]\n",
                               basename, passed.len, failed.len);
                } else {
                        printf(YELLOW "%s:" RESET " [ " BOLD GREEN "%zu Compiled" RESET " ]\n",
                               basename, passed.len);
                }

        cleanup:
                dyn_array_free(passed);
                dyn_array_free(failed);
                for (size_t i = 0; i < files.len; ++i) { free(files.data[i]); }
                dyn_array_free(files);
                closedir(dir);
                free(dirs[d]);
                free(abspath);
        }
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
        if (mkdir_p_wmode(MODULE_LIB_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", MODULE_LIB_DIR, strerror(errno));
        }
        if (mkdir_p_wmode(C_MODULE_USER_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", C_MODULE_USER_DIR, strerror(errno));
        }

        // Pkg source location
        if (mkdir_p_wmode(PKG_SOURCE_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", PKG_SOURCE_DIR, strerror(errno));
        }
}

void
edit_file_in_editor(const char *path)
{
        char *cmd = forge_str_builder(FORGE_EDITOR, " ", path, NULL);
        if (system(cmd) == -1) {
                fprintf(stderr, "Failed to open %s in %s: %s\n", path, FORGE_EDITOR, strerror(errno));
        }
        free(cmd);
}

void
new_pkg(forge_context *ctx, str_array *names)
{
        (void)ctx;

        for (size_t i = 0; i < names->len; ++i) {
                const char *n = names->data[i];
                int hitat = 0;
                for (size_t j = 0; n[j]; ++j) {
                        if (n[j] == '@') {
                                if (hitat) {
                                        err_wargs("only a single '@' is allowed in a package name: %s", n);
                                } else if (!n[j+1]) {
                                        err_wargs("'@' is not allowed in the last position of a package name: %s", n);
                                } else if (j == 0) {
                                        err_wargs("'@' is not allowed in the first position of a package name: %s", n);
                                }
                                hitat = 1;
                        }
                }
                if (!hitat) {
                        err_wargs("Missing '@'. Expected name in the format of `author@name`, got: %s", n);
                }
        }

        for (size_t i = 0; i < names->len; ++i) {
                char fp[256] = {0};
                sprintf(fp, C_MODULE_USER_DIR "%s.c", names->data[i]);
                if (cio_file_exists(fp)) {
                        err_wargs("file %s already exists", fp);
                }
                if (!cio_write_file(fp, FORGE_C_MODULE_TEMPLATE)) {
                        err_wargs("failed to write to file %s, %s", fp, strerror(errno));
                }
                edit_file_in_editor(fp);
        }
}

void
edit_c_module(str_array *names)
{
        for (size_t i = 0; i < names->len; ++i) {
                char *path = get_c_module_filepath_from_basic_name(names->data[i]);
                if (path) {
                        char *cmd = forge_str_builder(FORGE_EDITOR, " ", path, NULL);
                        if (system(cmd) == -1) {
                                fprintf(stderr, "Failed to open %s in %s: %s\n", path, FORGE_EDITOR, strerror(errno));
                        }
                        free(cmd);
                } else {
                        err_wargs("package %s does not exist", names->data[i]);
                }
                free(path);
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

        str_array skipped_pkgs = dyn_array_empty(str_array);

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

                const char *pkgname = forge_io_basename(pkg_src_loc);
                char base[256] = {0};
                snprintf(base, sizeof(base), "%s%s", PKG_SOURCE_DIR, pkgname);

                if (!cd(base)) {
                        fprintf(stderr, "Could not access source directory for %s, please reinstall\n", name);
                        free(pkg_src_loc);
                        continue;
                }

                // Get package ID
                int pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        fprintf(stderr, "Failed to find package ID for %s\n", name);
                        free(pkg_src_loc);
                        continue;
                }

                // Get current files from database
                str_array db_files = dyn_array_empty(str_array);
                sql = "SELECT file_path FROM Files WHERE pkg_id = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_int(stmt, 1, pkg_id);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *file_path = (const char *)sqlite3_column_text(stmt, 0);
                        dyn_array_append(db_files, strdup(file_path));
                }
                sqlite3_finalize(stmt);

                // Take a snapshot before update
                forge_smap snapshot_before = snapshot_files();

                int updated = 0;

                // Perform update
                printf(GREEN "(%s)->update()\n" RESET, name);
                if (pkg->update) {
                        updated = pkg->update();
                } else if ((g_config.flags & FT_FORCE) == 0) {
                        // We are not forcing the update, notify that we are skipping.
                        printf(YELLOW "*** No update function defined for %s, skipping update step\n" RESET, name);
                        dyn_array_append(skipped_pkgs, strdup(name));
                }

                // Take a snapshot after update
                forge_smap snapshot_after = snapshot_files();

                // Remove files from database that no longer exist
                sqlite3_stmt *stmt_delete;
                const char *sql_delete = "DELETE FROM Files WHERE pkg_id = ? AND file_path = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_delete, -1, &stmt_delete, NULL);
                CHECK_SQLITE(rc, ctx->db);

                for (size_t j = 0; j < db_files.len; ++j) {
                        if (!forge_smap_contains(&snapshot_after, db_files.data[j])) {
                                sqlite3_bind_int(stmt_delete, 1, pkg_id);
                                sqlite3_bind_text(stmt_delete, 2, db_files.data[j], -1, SQLITE_STATIC);
                                rc = sqlite3_step(stmt_delete);
                                if (rc != SQLITE_DONE) {
                                        fprintf(stderr, "Failed to delete file %s for package %s: %s\n",
                                                db_files.data[j], name, sqlite3_errmsg(ctx->db));
                                }
                                sqlite3_reset(stmt_delete);
                        }
                }
                sqlite3_finalize(stmt_delete);

                // Find new or modified files
                str_array diff_files = dyn_array_empty(str_array);
                char **keys = forge_smap_iter(&snapshot_after);
                for (size_t j = 0; keys[j]; ++j) {
                        // Add file if it's not in snapshot_before and not already in db_files
                        int in_db = 0;
                        for (size_t k = 0; k < db_files.len; ++k) {
                                if (strcmp(keys[j], db_files.data[k]) == 0) {
                                        in_db = 1;
                                        break;
                                }
                        }
                        if (!forge_smap_contains(&snapshot_before, keys[j]) && !in_db) {
                                dyn_array_append(diff_files, strdup(keys[j]));
                        }
                }

                // Insert new files into the Files table
                if (diff_files.len > 0) {
                        good_major("Adding new files to database", 1);
                        sqlite3_stmt *stmt_files;
                        const char *sql_insert_file = "INSERT OR IGNORE INTO Files (pkg_id, file_path) VALUES (?, ?);";
                        rc = sqlite3_prepare_v2(ctx->db, sql_insert_file, -1, &stmt_files, NULL);
                        CHECK_SQLITE(rc, ctx->db);

                        for (size_t j = 0; j < diff_files.len; ++j) {
                                sqlite3_bind_int(stmt_files, 1, pkg_id);
                                sqlite3_bind_text(stmt_files, 2, diff_files.data[j], -1, SQLITE_STATIC);

                                rc = sqlite3_step(stmt_files);
                                if (rc != SQLITE_DONE) {
                                        fprintf(stderr, "Failed to insert file %s for package %s: %s\n",
                                                diff_files.data[j], name, sqlite3_errmsg(ctx->db));
                                }
                                sqlite3_reset(stmt_files);
                        }
                        sqlite3_finalize(stmt_files);
                }

                // Clean up diff_files and db_files
                for (size_t j = 0; j < diff_files.len; ++j) {
                        free(diff_files.data[j]);
                }
                dyn_array_free(diff_files);
                for (size_t j = 0; j < db_files.len; ++j) {
                        free(db_files.data[j]);
                }
                dyn_array_free(db_files);

                // Clean up snapshots
                forge_smap_destroy(&snapshot_before);
                forge_smap_destroy(&snapshot_after);

                // Update dependencies and reinstall package if updated
                str_array install_names = dyn_array_empty(str_array);
                dyn_array_append(install_names, strdup(name));

                // Add dependencies to install list
                if (pkg->deps) {
                        good_major("queueing dependencies for reinstall", 1);
                        char **deps = pkg->deps();
                        for (size_t j = 0; deps[j]; ++j) {
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

                // Reinstall package and its dependencies if updated
                if (updated || (g_config.flags & FT_FORCE)) {
                        if (pkg->get_changes) {
                                printf(GREEN "(%s)->get_changes()\n" RESET, name);
                                pkg->get_changes();
                        } else {
                                printf(YELLOW "Removing source directory: %s\n" RESET, base);
                                if (!forge_io_rm_dir(base)) {
                                        fprintf(stderr, "Failed to remove source directory %s: %s\n", base, strerror(errno));
                                        // Continue with reinstallation even if removal fails, as it may still be possible
                                }
                        }

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

        if (skipped_pkgs.len > 0) {
                printf(BOLD YELLOW "*** %zu package(s) need to be checked for an updated manually:\n" RESET, skipped_pkgs.len);
                for (size_t i = 0; i < skipped_pkgs.len; ++i) {
                        printf(YELLOW "  %s\n" RESET, skipped_pkgs.data[i]);
                        free(skipped_pkgs.data[i]);
                }

                dyn_array_free(skipped_pkgs);
                printf(UNDERLINE YELLOW "Use the --force option to force the update\n" RESET);
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
sync(void)
{
        CD(C_MODULE_DIR_PARENT, {
                fprintf(stderr, "could cd to path: %s, %s\n", C_MODULE_DIR, strerror(errno));
                return;
        });

        char **files = ls(".");

        for (size_t i = 0; files[i]; ++i) {
                if (is_git_dir(files[i])) {
                        printf(GREEN BOLD "Syncing [%s]\n" RESET, files[i]);
                        CD(files[i], fprintf(stderr, "could not change directory: %s\n", strerror(errno)));
                        CMD("git fetch origin && git pull origin main", {
                                fprintf(stderr, "could not sync directory %s: %s\n",
                                        files[i], strerror(errno));
                        });
                        CD(C_MODULE_DIR_PARENT, {
                                fprintf(stderr, "could cd to path: %s, %s\n", C_MODULE_DIR, strerror(errno));
                                return;
                        });
                }
                free(files[i]);
        }
        free(files);
}

void
clearln(void)
{
        fflush(stdout);
        printf("\033[1A");
        printf("\033[2K");
}

int
iskw(char *s)
{
        const char *kwds[] = {
                "void",
                "int",
                "const",
                "char",
                "float",
                "double",
                "size_t",
                "unsigned",
                "long",
                "typedef",
                "struct",
                "enum",
                "#define",
                "#ifndef",
                "#endif",
                "#if",
                "#else",
                "#endif",
                "#include",
                "__attribute__",
                "return",
                "break",
                "continue",
                "goto",
                "if",
                "else",
                "while",
                "for",
                "sizeof",
                "typeof",
        };

        for (size_t i = 0; i < sizeof(kwds)/sizeof(*kwds); ++i) {
                if (!strcmp(s, kwds[i])) {
                        return 1;
                }
        }
        return 0;
}

void
dyn_array_append_str(char_array *arr, const char *str)
{
        if (!arr || !str) {
                return; // Handle null inputs
        }

        // Calculate length of the input string (excluding null terminator)
        size_t str_len = strlen(str);

        // Ensure the array has enough capacity
        while (arr->len + str_len >= arr->cap) {
                // Double the capacity or set to a minimum if zero
                size_t new_cap = arr->cap == 0 ? 16 : arr->cap * 2;
                char *new_data = realloc(arr->data, new_cap * sizeof(char));
                if (!new_data) {
                        fprintf(stderr, "Failed to reallocate memory for char_array\n");
                        return; // Memory allocation failure
                }
                arr->data = new_data;
                arr->cap = new_cap;
        }

        // Copy the string into the array
        memcpy(arr->data + arr->len, str, str_len);
        arr->len += str_len;
}

char *
api_colorize_to_string(const char *s)
{
        char_array buf = dyn_array_empty(char_array);
        char_array result = dyn_array_empty(char_array);
        static int in_multiline_comment = 0; // Persists across calls
        int in_single_line_comment = 0;
        int in_string = 0; // Track if inside a string literal
        int in_char = 0;   // Track if inside a character literal
        int escape = 0;    // Track escape sequences in strings/chars

        for (size_t i = 0; s[i]; ++i) {
                if (in_multiline_comment) {
                        // Inside a multiline comment, look for the end (*/)
                        if (s[i] == '*' && s[i + 1] == '/') {
                                dyn_array_append(buf, s[i]);
                                dyn_array_append(buf, s[i + 1]);
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, PINK BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                                in_multiline_comment = 0;
                                i++; // Skip the '/'
                                continue;
                        }
                        dyn_array_append(buf, s[i]);
                } else if (in_single_line_comment) {
                        // Inside a single-line comment, append until newline
                        if (s[i] == '\n') {
                                in_single_line_comment = 0;
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, PINK BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                                dyn_array_append(result, s[i]);
                        } else {
                                dyn_array_append(buf, s[i]);
                        }
                } else if (in_string) {
                        // Inside a string literal
                        dyn_array_append(buf, s[i]);
                        if (escape) {
                                escape = 0; // Reset escape flag after handling
                        } else if (s[i] == '\\') {
                                escape = 1; // Next character is escaped
                        } else if (s[i] == '"') {
                                // End of string literal
                                in_string = 0;
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, GREEN BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                        }
                } else if (in_char) {
                        // Inside a character literal
                        dyn_array_append(buf, s[i]);
                        if (escape) {
                                escape = 0; // Reset escape flag
                        } else if (s[i] == '\\') {
                                escape = 1; // Next character is escaped
                        } else if (s[i] == '\'') {
                                // End of character literal
                                in_char = 0;
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, GREEN BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                        }
                } else {
                        // Not in a comment, string, or char; check for starts or delimiters
                        if (s[i] == '/' && s[i + 1] == '*') {
                                dyn_array_append(buf, s[i]);
                                dyn_array_append(buf, s[i + 1]);
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, PINK BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                                in_multiline_comment = 1;
                                i++; // Skip the '*'
                                continue;
                        } else if (s[i] == '/' && s[i + 1] == '/') {
                                dyn_array_append(buf, s[i]);
                                dyn_array_append(buf, s[i + 1]);
                                in_single_line_comment = 1;
                                i++; // Skip the '/'
                                continue;
                        } else if (s[i] == '"') {
                                // Start of string literal
                                in_string = 1;
                                dyn_array_append(buf, s[i]);
                                continue;
                        } else if (s[i] == '\'') {
                                // Start of character literal
                                in_char = 1;
                                dyn_array_append(buf, s[i]);
                                continue;
                        } else if (s[i] == ';' || s[i] == '\n' || s[i] == '\t' ||
                                   s[i] == ' ' || s[i] == '(' || s[i] == ')' || s[i] == ',') {
                                // Handle delimiters
                                dyn_array_append(buf, 0);
                                if (buf.len > 0 && iskw(buf.data)) {
                                        dyn_array_append_str(&result, YELLOW BOLD);
                                        dyn_array_append_str(&result, buf.data);
                                        dyn_array_append_str(&result, RESET);
                                } else if (buf.len > 0) {
                                        dyn_array_append_str(&result, buf.data);
                                }
                                dyn_array_clear(buf);
                                dyn_array_append(result, s[i]);
                        } else {
                                dyn_array_append(buf, s[i]);
                        }
                }
        }

        dyn_array_append(buf, 0);
        if (buf.len > 0) {
                if (in_multiline_comment || in_single_line_comment) {
                        dyn_array_append_str(&result, PINK BOLD);
                } else if (in_string || in_char) {
                        dyn_array_append_str(&result, GREEN BOLD);
                } else if (iskw(buf.data)) {
                        dyn_array_append_str(&result, YELLOW BOLD);
                }
                dyn_array_append_str(&result, buf.data);
                dyn_array_append_str(&result, RESET);
        }

        dyn_array_append(result, 0);
        char *final_string = strdup(result.data);

        dyn_array_free(buf);
        dyn_array_free(result);

        return final_string;
}

void
api_dump(const char *name, int api)
{
        forge_str path = forge_str_create();
        if (api) {
                // Handle API header dump
                forge_str_concat(&path, FORGE_API_HEADER_DIR);
                forge_str_concat(&path, "/");
                forge_str_concat(&path, name);
                forge_str_concat(&path, ".h");
                if (!forge_io_filepath_exists(forge_str_to_cstr(&path))) {
                        err_wargs("API `%s` does not exist", name);
                }
        } else {
                char *abspath = get_c_module_filepath_from_basic_name(name);
                if (!abspath) {
                        err_wargs("package `%s` does not exist", name);
                        return;
                }
                else { forge_str_concat(&path, abspath); }
                free(abspath);
        }

        if (api) {
                printf(GREEN BOLD "*** API dump of: [ %s ]\n" RESET, forge_str_to_cstr(&path));
        } else {
                printf(GREEN BOLD "*** C Module dump of: [ %s ]\n" RESET, forge_str_to_cstr(&path));
        }
        char **lines = forge_io_read_file_to_lines(forge_str_to_cstr(&path));

        // Count total lines to determine maximum line number width
        size_t line_count = 0;
        for (size_t i = 0; lines[i]; ++i) {
                line_count++;
        }

        char **colored_lines = (char**)malloc(sizeof(char*)*line_count);
        for (size_t i = 0; i < line_count; ++i) {
                colored_lines[i] = api_colorize_to_string(lines[i]);
                free(lines[i]);
        }
        free(lines);

        forge_viewer *m = forge_viewer_alloc(colored_lines, line_count);
        forge_viewer_display(m);
        forge_viewer_free(m);
        forge_str_destroy(&path);
}

void
editconf(void)
{
        if (!forge_io_filepath_exists(FORGE_CONF_HEADER_FP)) {
                err_wargs("fatal: somehow the path %s does not exist",
                          FORGE_CONF_HEADER_FP);
        }
        edit_file_in_editor(FORGE_CONF_HEADER_FP);
}

void
updateforge(void)
{
        time_t now = time(NULL);
        char hash[32] = {0};
        snprintf(hash, 32, "%ld", now);
        forge_str dir = forge_str_from("/tmp/forgeupdate-"),
                clone = forge_str_from("git clone https://www.github.com/malloc-nbytes/forge.git ");
        forge_str_concat(&dir, hash);
        forge_str_concat(&clone, forge_str_to_cstr(&dir));

        // git clone
        if (!cmd(forge_str_to_cstr(&clone))) goto fail;
        // cd /tmp/forgeupdate
        if (!cd(forge_str_to_cstr(&dir))) goto fail;

        // save forge/conf.h
        char *conf_content = forge_io_read_file_to_cstr(FORGE_CONF_HEADER_FP);
        forge_str overwrite_fp = forge_str_from(forge_str_to_cstr(&dir));
        forge_str_concat(&overwrite_fp, "/src/forge/conf.h");

        if (!forge_io_write_file(forge_str_to_cstr(&overwrite_fp), conf_content)) {
                fprintf(stderr, "failed to overwrite the new conf.h at: %s\n",
                        forge_str_to_cstr(&overwrite_fp));
                goto fail;
        }

        if (!cmd("./bootstrap.sh"))  goto fail;
        if (!cmd("make -j$(nproc)")) goto fail;
        if (!cmd("make install"))    goto fail;

        forge_str_destroy(&dir);
        forge_str_destroy(&clone);
        forge_str_destroy(&overwrite_fp);
        free(conf_content);

        return;

 fail:
        fprintf(stderr, "aborting...\n");
        return;
}

void
apilist(void)
{
        char **files = ls(FORGE_API_HEADER_DIR);
        if (!files) {
                fprintf(stderr, "could not find FORGE_API_HEADER_DIR\n");
                return;
        }
        for (size_t i = 0; files[i]; ++i) {
                if (strcmp(files[i], ".")
                    && strcmp(files[i], "..")
                    && strcmp(files[i], "forge.h")) {
                        forge_str include = forge_str_from("#include <forge/");
                        forge_str_concat(&include, files[i]);
                        forge_str_append(&include, '>');

                        int per = 0;
                        for (size_t j = 0; files[i][j]; ++j) {
                                if (files[i][j] == '.') {
                                        per = j;
                                }
                        }
                        files[i][per] = 0;

                        printf("Name: %s\n", files[i]);

                        printf("  %s\n", forge_str_to_cstr(&include));
                        forge_str_destroy(&include);
                }
                free(files[i]);
        }
        printf("Name: forge\n");
        printf("  #include <forge/forge.h> // includes all headers\n");
}

void
pkg_search(const str_array *names)
{
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

                int found = 0;
                for (size_t i = 0; i < names->len; ++i) {
                        if (regex(names->data[i], name)) {
                                found = 1;
                                break;
                        }
                }

                if (found) {
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
                printf("No packages matched the search.\n");
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
add_repo(const char *name)
{
        CD(C_MODULE_DIR_PARENT, goto bad);
        char *clone = forge_str_builder("git clone ", name, NULL);
        CMD(clone, goto bad);
        goto ok;
 bad:
        printf("aborting...\n");
 ok:
        free(clone);
}

void
drop_repo(forge_context *ctx,
          const char    *repo_name)
{
        // Construct the full path to the repository
        char *repo_path = forge_str_builder(C_MODULE_DIR_PARENT, repo_name, NULL);
        if (!forge_io_is_dir(repo_path)) {
                fprintf(stderr, "Repository %s does not exist at %s\n", repo_name, repo_path);
                free(repo_path);
                return;
        }

        // Get list of .c files in the repository
        str_array pkg_files = get_files_in_dir(repo_path);
        str_array pkg_names = dyn_array_empty(str_array);

        // Extract package names (without .c extension)
        for (size_t i = 0; i < pkg_files.len; ++i) {
                char *filename = pkg_files.data[i];
                size_t len = strlen(filename);
                if (len > 2 && strcmp(filename + len - 2, ".c") == 0) {
                        char *name = strdup(filename);
                        name[len - 2] = '\0'; // Remove .c extension
                        dyn_array_append(pkg_names, name);
                }
        }

        for (size_t i = 0; i < pkg_files.len; ++i) {
                free(pkg_files.data[i]);
        }
        dyn_array_free(pkg_files);

        // Prompt user for uninstalling packages
        int uninstall = 0;
        if (pkg_names.len > 0) {
                printf(YELLOW "Found %zu package(s) in repository %s:\n" RESET, pkg_names.len, repo_name);
                for (size_t i = 0; i < pkg_names.len; ++i) {
                        printf("  %s\n", pkg_names.data[i]);
                }
                printf(YELLOW "\nWould you like to uninstall installed packages from this repository? [y/N]: " RESET);
                fflush(stdout);

                char response[10];
                if (fgets(response, sizeof(response), stdin) != NULL) {
                        response[strcspn(response, "\n")] = '\0';
                        if (response[0] == 'y' || response[0] == 'Y') {
                                uninstall = 1;
                        }
                }
        }

        // Uninstall packages if requested
        if (uninstall) {
                str_array names_to_uninstall = dyn_array_empty(str_array);
                for (size_t i = 0; i < pkg_names.len; ++i) {
                        if (pkg_is_installed(ctx, pkg_names.data[i]) == 1) {
                                dyn_array_append(names_to_uninstall, strdup(pkg_names.data[i]));
                        }
                }
                if (names_to_uninstall.len > 0) {
                        printf(GREEN BOLD "*** Uninstalling packages from repository %s\n" RESET, repo_name);
                        uninstall_pkg(ctx, &names_to_uninstall);
                        for (size_t i = 0; i < names_to_uninstall.len; ++i) {
                                free(names_to_uninstall.data[i]);
                        }
                        dyn_array_free(names_to_uninstall);
                } else {
                        printf(YELLOW "No installed packages found in repository %s\n" RESET, repo_name);
                }
        }

        printf(GREEN BOLD "*** Dropping packages from repository %s\n" RESET, repo_name);
        for (size_t i = 0; i < pkg_names.len; ++i) {
                if (get_pkg_id(ctx, pkg_names.data[i]) != -1) {
                        drop_pkg(ctx, pkg_names.data[i]);
                }
        }

        // Remove the repository directory
        printf(YELLOW "Removing repository directory: %s\n" RESET, repo_path);
        if (!forge_io_rm_dir(repo_path)) {
                fprintf(stderr, "Failed to remove repository directory %s: %s\n", repo_path, strerror(errno));
        } else {
                printf(GREEN BOLD "*** Successfully removed repository %s\n" RESET, repo_name);
        }

        for (size_t i = 0; i < pkg_names.len; ++i) {
                free(pkg_names.data[i]);
        }
        dyn_array_free(pkg_names);
        free(repo_path);
}

void
list_repos(void)
{
        char **dirs = ls(C_MODULE_DIR_PARENT);
        if (!dirs) {
                fprintf(stderr, "Failed to list directories in %s: %s\n", C_MODULE_DIR_PARENT, strerror(errno));
                return;
        }

        // Collect valid directories and calculate max width for formatting
        str_array repos = dyn_array_empty(str_array);
        size_t max_name_len = strlen("Repository");

        for (size_t i = 0; dirs[i]; ++i) {
                if (!strcmp(dirs[i], ".") || !strcmp(dirs[i], "..")) {
                        free(dirs[i]);
                        continue;
                }

                char *repo_path = forge_str_builder(C_MODULE_DIR_PARENT, dirs[i], NULL);
                if (forge_io_is_dir(repo_path)) {
                        dyn_array_append(repos, strdup(dirs[i]));
                        max_name_len = MAX(max_name_len, strlen(dirs[i]));
                }
                free(repo_path);
                free(dirs[i]);
        }
        free(dirs);

        printf("%-*s\n", (int)max_name_len, "Repository");
        printf("%-*s\n", (int)max_name_len, "----------");

        if (repos.len == 0) {
                printf("No repositories found in %s.\n", C_MODULE_DIR_PARENT);
        } else {
                for (size_t i = 0; i < repos.len; ++i) {
                        //printf("%-*s", (int)max_name_len, repos.data[i]);
                        if (!strcmp(repos.data[i], "user_modules")) {
                                printf(BLUE "%-*s" RESET, (int)max_name_len, repos.data[i]);
                                printf(BLUE " (built-in)" RESET);
                        } else if (!strcmp(repos.data[i], "forge-modules")) {
                                printf(GREEN "%-*s" RESET, (int)max_name_len, repos.data[i]);
                                printf(GREEN " (forge official repository)" RESET);
                        } else {
                                printf(YELLOW "%-*s" RESET, (int)max_name_len, repos.data[i]);
                                printf(YELLOW " (third-party)" RESET);
                        }
                        putchar('\n');
                }
        }

        for (size_t i = 0; i < repos.len; ++i) {
                free(repos.data[i]);
        }
        dyn_array_free(repos);
}

void
create_repo_compile_template(void)
{
        char *script = "#!/bin/bash\n"
                "\n"
                "set -e\n"
                "\n"
                "for file in *.c; do\n"
                "    if [[ -f \"$file\" ]]; then\n"
                "        echo \"gcc -shared -fPIC -o \\\"${file%.c}.so\\\" \\\"$file\\\"\"\n"
                "        gcc -shared -fPIC -o \"${file%.c}.so\" \"$file\"\n"
                "\n"
                "        if ! [[ $? -eq 0 ]]; then\n"
                "                echo \"Failed to compile $file\"\n"
                "                exit 1\n"
                "        fi\n"
                "    else\n"
                "        echo \"No .c files found in the current directory\"\n"
                "        exit 1\n"
                "    fi\n"
                "done\n"
                "\n"
                "echo \"Removing all .so files...\"\n"
                "rm -f *.so\n"
                "echo \"Done.\"\n";
        printf("%s\n", script);
}

void
create_repo(const char *repo_name,
            const char *repo_url)
{
        char *new_repo_path = forge_str_builder(C_MODULE_DIR_PARENT, repo_name, NULL);
        char *copy_cmd = forge_str_builder("cp ", C_MODULE_USER_DIR, "*.c ", new_repo_path, " 2>/dev/null || true", NULL); // Handle no .c files
        char *del_cmd = forge_str_builder("rm -f ", C_MODULE_USER_DIR, "*.c", NULL);
        char *add_origin_cmd = forge_str_builder("git remote add origin ", repo_url, NULL);

        // Create the new repository directory
        if (mkdir_p_wmode(new_repo_path, 0755) != 0) {
                fprintf(stderr, "Failed to create directory %s: %s\n", new_repo_path, strerror(errno));
                goto cleanup;
        }

        // Copy .c files from user_modules to new repo
        if (!cmd(copy_cmd)) {
                fprintf(stderr, "Warning: No .c files found in %s or failed to copy: %s\n", C_MODULE_USER_DIR, strerror(errno));
        }

        // Remove .c files from user_modules
        if (!cmd(del_cmd)) {
                fprintf(stderr, "Warning: Failed to remove .c files from %s: %s\n", C_MODULE_USER_DIR, strerror(errno));
        }

        if (!cd(new_repo_path)) {
                fprintf(stderr, "Failed to change to directory %s: %s\n", new_repo_path, strerror(errno));
                goto cleanup;
        }

        if (!cmd("git init")) {
                fprintf(stderr, "Failed to initialize Git repository in %s: %s\n", new_repo_path, strerror(errno));
                goto cleanup;
        }

        // Rename default branch to 'main'
        if (!cmd("git branch -m master main")) {
                fprintf(stderr, "Failed to rename branch from master to main: %s\n", strerror(errno));
                goto cleanup;
        }

        // Set local Git user identity
        if (!cmd("git config user.name \"Forge User\"")) {
                fprintf(stderr, "Failed to set Git user.name: %s\n", strerror(errno));
                goto cleanup;
        }
        if (!cmd("git config user.email \"forge@forge.com\"")) {
                fprintf(stderr, "Failed to set Git user.email: %s\n", strerror(errno));
                goto cleanup;
        }

        if (!cmd("git add .")) {
                fprintf(stderr, "Failed to add files to Git: %s\n", strerror(errno));
                goto cleanup;
        }

        if (!cmd("git commit -m 'Initial commit'")) {
                fprintf(stderr, "Failed to commit changes: %s\n", strerror(errno));
                goto cleanup;
        }

        // Add remote origin
        if (!cmd(add_origin_cmd)) {
                fprintf(stderr, "Failed to add remote origin %s: %s\n", repo_url, strerror(errno));
                goto cleanup;
        }

        // Verify remote
        if (!cmd("git remote -v")) {
                fprintf(stderr, "Failed to verify remote: %s\n", strerror(errno));
                goto cleanup;
        }

        // Pull with allow-unrelated-histories (in case the remote has history)
        if (!cmd("git pull origin main --allow-unrelated-histories --no-rebase")) {
                fprintf(stderr, "Warning: Failed to pull from origin main, repository might be empty or branch mismatch: %s\n", strerror(errno));
                // Continue even if pull fails, as the remote might be empty
        }

        // Push to remote
        if (!cmd("git push -u origin main")) {
                fprintf(stderr, "Failed to push to %s: %s\n", repo_url, strerror(errno));
                fprintf(stderr, "Ensure the repository exists, you have write access, and authentication is configured (e.g., SSH key or personal access token).\n");
                goto cleanup;
        }

        printf(GREEN BOLD "*** Successfully created and pushed repository %s to %s\n" RESET, repo_name, repo_url);

 cleanup:
        fprintf(stderr, "Cleaning up...\n");
        free(new_repo_path);
        free(copy_cmd);
        free(del_cmd);
        free(add_origin_cmd);
}

void
show_lib(void)
{
        printf("-lforge\n");
}

void
browse_api(void)
{
        char **apis = ls(FORGE_API_HEADER_DIR);
        str_array combined = dyn_array_empty(str_array);

        for (size_t i = 0; apis[i]; ++i) {
                char *path = forge_str_builder(FORGE_API_HEADER_DIR, "/", apis[i], NULL);
                char **lines = forge_io_read_file_to_lines(path);

                for (size_t j = 0; lines[j]; ++j) {
                        dyn_array_append(combined, api_colorize_to_string(lines[j]));
                        free(lines[j]);
                }

                free(lines);
                free(path);
                free(apis[i]);
        }

        forge_viewer *m = forge_viewer_alloc(combined.data, combined.len);
        forge_viewer_display(m);
        forge_viewer_free(m);

        for (size_t i = 0; i < combined.len; ++i) {
                free(combined.data[i]);
        }
        dyn_array_free(combined);
        free(apis);
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
                printf("Superuser access is required the first time forge is ran.\n");
                assert_sudo();
                init_env();
                printf("Done. You can now invoke forge regularly.\n");
                printf("Note: You may want to edit your config, run:\n");
                printf("          forge editconf\n");
                printf("      After this, do:\n");
                printf("          forge updateforge\n");
                printf("Note: If you want to add the official repository, run:\n");
                printf("          forge add-repo https://github.com/malloc-nbytes/forge-modules.git\n");
                printf("      or don't if you just want to start from scratch.\n");
                printf("      To get started, run:\n");
                printf("          forge -r new author@pkgname\n");
                printf("      to start forging your packages.\n");
                printf("Do `forge -h` to view all help information.\n");
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
                        if (arg.eq) { help(arg.eq); }
                        usage();
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_HELP)) {
                        if (arg.eq) { help(arg.eq); }
                        usage();
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_FORCE)) {
                        g_config.flags |= FT_FORCE;
                }

                else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_LIST)) {
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
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_UNINSTALL);
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
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_EDIT);
                        assert_sudo();
                        edit_c_module(&names);
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
                        for (size_t i = 0; i < names.len; ++i)
                                api_dump(names.data[i], 0);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_SYNC)) {
                        g_config.flags |= FT_SYNC;
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_DROP_BROKEN_PKGS)) {
                        g_config.flags |= FT_DROP_BROKEN_PKGS;
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_DROP)) {
                        assert_sudo();
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_DROP);
                        for (size_t i = 0; i < names.len; ++i)
                                drop_pkg(&ctx, names.data[i]);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_FILES)) {
                        if (!clap_next(&arg)) { err_wargs("flag `%s` requires an argument", FLAG_2HY_FILES); }
                        list_files(&ctx, arg.start, 0);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_COPYING)) {
                        copying();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_DEPGRAPH)) {
                        depgraph_dump(&ctx.dg);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_API)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) {
                                browse_api();
                        } else {
                                for (size_t i = 0; i < names.len; ++i)
                                        api_dump(names.data[i], 1);
                                for (size_t i = 0; i < names.len; ++i)
                                        free(names.data[i]);
                        }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_EDITCONF)) {
                        assert_sudo();
                        editconf();
                        printf(YELLOW BOLD "=== NOTE ===\n" RESET YELLOW);
                        printf(YELLOW "For these changes to take effect, you need\n");
                        printf(YELLOW "to rebuild forge. To do this, run `forge %s`\n", FLAG_2HY_UPDATEFORGE);
                        printf(YELLOW "This action requires an internet connection.\n");
                        printf(YELLOW BOLD "============\n" RESET);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_UPDATEFORGE)) {
                        assert_sudo();
                        updateforge();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_RESTORE)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires an argument", FLAG_2HY_DEPS);
                        }
                        assert_sudo();
                        restore_pkg(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_APILIST)) {
                        apilist();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_SEARCH)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", FLAG_2HY_SEARCH);
                        pkg_search(&names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_ADD_REPO)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires a Github repo link", FLAG_2HY_ADD_REPO);
                        }
                        assert_sudo();
                        g_config.flags |= FT_REBUILD;
                        add_repo(arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_DROP_REPO)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires an argument", FLAG_2HY_DROP_REPO);
                        }
                        assert_sudo();
                        g_config.flags |= FT_REBUILD;
                        drop_repo(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_LIST_REPOS)) {
                        list_repos();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_REPO_COMPILE_TEMPLATE)) {
                        create_repo_compile_template();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_CREATE_REPO)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires a repo name", FLAG_2HY_CREATE_REPO);
                        }
                        char *repo_name = strdup(arg.start);
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires a repo url", FLAG_2HY_CREATE_REPO);
                        }
                        char *repo_url = strdup(arg.start);
                        assert_sudo();
                        create_repo(repo_name, repo_url);
                        g_config.flags |= FT_REBUILD;
                        free(repo_name);
                        free(repo_url);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_LIB)) {
                        show_lib();
                }

                else if (arg.hyphc == 1) { // one hyph options
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
        if (g_config.flags & FT_DROP_BROKEN_PKGS) {
                assert_sudo();
                for (size_t i = 0; i < ctx.pkgs.len; ++i) {
                        pkg *pkg = ctx.pkgs.data[i];
                        if (!pkg->ver || !pkg->desc || !pkg->download) {
                                drop_pkg(&ctx, pkg->name());
                        }
                }
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
