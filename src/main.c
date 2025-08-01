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
get_absolute_files_in_dir(const char *fp, int err_on_noexist)
{
        str_array res = dyn_array_empty(str_array);
        DIR *dir = opendir(fp);
        if (!dir) {
                if (err_on_noexist) {
                        fprintf(stderr, "Failed to open directory %s: %s\n", fp, strerror(errno));
                }
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
        const char *common_install_dirs[] = {
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
                "/usr/local/etc",
                "/usr/local/src",
        };

        for (size_t i = 0;
             i < sizeof(common_install_dirs)/sizeof(*common_install_dirs); ++i) {
                str_array ar = get_absolute_files_in_dir(common_install_dirs[i], 0);
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
                "is_explicit INTEGER NOT NULL DEFAULT 0,"
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
                char *module_dir = forge_cstr_builder(C_MODULE_DIR_PARENT, dirs[i], NULL);
                if (forge_io_is_dir(module_dir)) {
                        CD(module_dir, {});
                        char *path = forge_cstr_builder(module_dir, "/", name, ".c", NULL);
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

                char *module_dir = forge_cstr_builder(C_MODULE_DIR_PARENT, dirs[i], NULL);
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
register_pkg(forge_context *ctx, pkg *pkg, int is_explicit)
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
                const char *sql_update = "UPDATE Pkgs SET version = ?, description = ?, is_explicit = ? WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, ver, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 3, is_explicit);
                sqlite3_bind_text(stmt, 4, name, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);
        } else {
                // New package
                printf(YELLOW BOLD "* " RESET "Registered package: " YELLOW BOLD "%s\n" RESET, name);

                const char *sql_insert = "INSERT INTO Pkgs (name, version, description, installed, is_explicit) VALUES (?, ?, ?, 0, ?);";
                rc = sqlite3_prepare_v2(ctx->db, sql_insert, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, ver, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, desc, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 4, is_explicit);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Insert error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                if (pkg->deps) {
                        char **deps = pkg->deps();
                        for (size_t i = 0; deps[i]; ++i) {
                                //printf(GREEN BOLD "*** Adding dependency %s for %s\n" RESET, deps[i], name);
                                add_dep_to_db(ctx, get_pkg_id(ctx, name), get_pkg_id(ctx, deps[i]));
                        }
                }
        }
}

void
deps(forge_context *ctx,
          const char    *pkg_name)
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

int
is_required_dependency(forge_context *ctx,
                       const char    *name)
{
        sqlite3_stmt *stmt;
        const char *sql = "SELECT COUNT(*) FROM Deps d "
                "JOIN Pkgs p ON d.pkg_id = p.id "
                "WHERE d.dep_id = (SELECT id FROM Pkgs WHERE name = ?) "
                "AND p.installed = 1;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return count > 0;
}

void
uninstall_pkg(forge_context *ctx,
              str_array     *names)
{
        for (size_t i = 0; i < names->len; ++i) {
                const char *name = names->data[i];
                printf(GREEN BOLD "\n*** Uninstalling package %s [%zu of %zu]\n" RESET, name, i+1, names->len);
                fflush(stdout);
                sleep(1);

                pkg *pkg = NULL;
                char *pkg_src_loc = NULL;
                int pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        err_wargs("unregistered package `%s`", name);
                }
                for (size_t j = 0; j < ctx->pkgs.len; ++j) {
                        if (!strcmp(ctx->pkgs.data[j]->name(), name)) {
                                pkg = ctx->pkgs.data[j];
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
                                fprintf(stderr, "Could not find source code for %s, please reinstall\n", name);
                                free(pkg_src_loc);
                                continue;
                        }
                } else {
                        fprintf(stderr, "Could not find source code for %s, please reinstall\n", name);
                        continue;
                }

                // Change to package directory
                if (!cd(base)) {
                        fprintf(stderr, "aborting...\n");
                        free(pkg_src_loc);
                        continue;
                }

                // Perform uninstall
                printf(GREEN "\n(%s)->uninstall()\n\n" RESET, name);
                pkg->uninstall();

                // Update installed status in database
                const char *sql_update = "UPDATE Pkgs SET installed = 0 WHERE id = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_int(stmt, 1, pkg_id);

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
clean_pkgs(forge_context *ctx)
{
        printf(GREEN BOLD "*** Cleaning unneeded dependency packages\n" RESET);

        // Get all installed dependency packages
        str_array pkgs_to_remove = dyn_array_empty(str_array);
        sqlite3_stmt *stmt;
        const char *sql = "SELECT name FROM Pkgs WHERE installed = 1 AND is_explicit = 0;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                if (!is_required_dependency(ctx, name)) {
                        dyn_array_append(pkgs_to_remove, strdup(name));
                }
        }
        sqlite3_finalize(stmt);

        if (pkgs_to_remove.len == 0) {
                printf(YELLOW "No unneeded dependency packages found.\n" RESET);
        } else {
                printf(GREEN BOLD "*** Found %zu unneeded dependency package(s) to remove\n" RESET, pkgs_to_remove.len);
                for (size_t i = 0; i < pkgs_to_remove.len; ++i) {
                        printf("* %s\n", pkgs_to_remove.data[i]);
                }
                uninstall_pkg(ctx, &pkgs_to_remove);
        }

        for (size_t i = 0; i < pkgs_to_remove.len; ++i) {
                free(pkgs_to_remove.data[i]);
        }
        dyn_array_free(pkgs_to_remove);
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
                        printf(GREEN BOLD "\n*** Installing dependency %s [%zu of %zu]\n" RESET,
                               name, i+1, names->len);
                } else {
                        printf(GREEN BOLD "\n*** Installing package %s [%zu of %zu]\n" RESET,
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
                for (size_t j = 0; j < ctx->pkgs.len; ++j) {
                        if (!strcmp(ctx->pkgs.data[j]->name(), name)) {
                                pkg = ctx->pkgs.data[j];
                                break;
                        }
                }
                assert(pkg);

                if (pkg_is_installed(ctx, name) && is_dep) {
                        printf(YELLOW "dependency %s is already installed\n" RESET, name);
                        continue; // Skip to next package
                }

                // Check current is_explicit status
                int is_explicit = is_dep ? 0 : 1; // Default: 0 for deps, 1 for explicit
                if (pkg_id != -1) { // Package exists in database
                        sqlite3_stmt *stmt;
                        const char *sql = "SELECT is_explicit FROM Pkgs WHERE name = ?;";
                        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                        CHECK_SQLITE(rc, ctx->db);

                        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                                int existing_is_explicit = sqlite3_column_int(stmt, 0);
                                if (!is_dep) { // For explicit installations, preserve is_explicit = 1
                                        is_explicit = existing_is_explicit || 1; // Keep 1 if already 1, else set to 1
                                } else { // For dependencies, keep existing is_explicit
                                        is_explicit = existing_is_explicit;
                                }
                        }
                        sqlite3_finalize(stmt);
                }

                // Register package with the determined is_explicit value
                register_pkg(ctx, pkg, is_explicit);

                // Install deps
                if (pkg->deps) {
                        good_major("Installing Dependencies", 1);
                        printf(GREEN "(%s)->deps()\n" RESET, name);
                        char **deps = pkg->deps();
                        str_array depnames = dyn_array_empty(str_array);
                        for (size_t j = 0; deps[j]; ++j) {
                                dyn_array_append(depnames, strdup(deps[j]));
                        }
                        if (!install_pkg(ctx, &depnames, 1)) {
                                err_wargs("could not install package %s, stopping...\n", names->data[i]);
                                for (size_t j = 0; j < depnames.len; ++j) {
                                        free(depnames.data[j]);
                                }
                                dyn_array_free(depnames);
                                return 0;
                        }
                        for (size_t j = 0; j < depnames.len; ++j) {
                                free(depnames.data[j]);
                        }
                        dyn_array_free(depnames);
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
                        free(pkg_src_loc);
                        return 0;
                }

                const char *pkgname = NULL;

                if (pkg_src_loc) {
                        pkgname = forge_io_basename(pkg_src_loc);
                } else {
                        printf(GREEN "\n(%s)->download()\n\n" RESET, name);
                        pkgname = pkg->download();
                }

                if (!cd_silent(pkgname)) {
                        pkg->download();
                        if (!cd(pkgname)) {
                                fprintf(stderr, "aborting...\n");
                                free(pkg_src_loc);
                                return 0;
                        }
                }

                char base[256] = {0};
                sprintf(base, PKG_SOURCE_DIR "%s", pkgname);

                printf(GREEN "\n(%s)->build()\n\n" RESET, name);
                pkg->build();
                if (!cd(base)) {
                        fprintf(stderr, "aborting...\n");
                        free(pkg_src_loc);
                        return 0;
                }

                forge_smap snapshot_before = snapshot_files();

                printf(GREEN "\n(%s)->install()\n\n" RESET, name);
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
                pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        fprintf(stderr, "Failed to find package ID for %s\n", name);
                        // Clean up and return
                        for (size_t j = 0; j < diff_files.len; ++j) {
                                free(diff_files.data[j]);
                        }
                        dyn_array_free(diff_files);
                        forge_smap_destroy(&snapshot_before);
                        forge_smap_destroy(&snapshot_after);
                        free(pkg_src_loc);
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
                const char *sql_update = "UPDATE Pkgs SET pkg_src_loc = ?, installed = 1 WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, base, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update pkg_src_loc error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                printf(PINK ITALIC BOLD "\n*** Installed:\n" RESET PINK ITALIC);
                list_files(ctx, name, 1);
                printf(RESET);

                free(pkg_src_loc);
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
                err(BOLD YELLOW "* " RESET "This action requires " BOLD YELLOW "superuser privileges" RESET);
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
                char *abspath = forge_cstr_builder(C_MODULE_DIR_PARENT, dirs[d], NULL);
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
        char *cmd = forge_cstr_builder(FORGE_EDITOR, " ", path, NULL);
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
                        char *cmd = forge_cstr_builder(FORGE_EDITOR, " ", path, NULL);
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
                        printf(YELLOW "\n*** Skipping update for %s: not installed\n\n" RESET, name);
                        free(pkg_src_loc);
                        continue;
                }

                printf(GREEN BOLD "*** Updating package %s [%zu of %zu]\n\n" RESET, name, i+1, pkg_names.len);
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
                printf(GREEN "(%s)->update()\n\n" RESET, name);
                if (pkg->update) {
                        updated = pkg->update();
                } else if ((g_config.flags & FT_FORCE) == 0) {
                        // We are not forcing the update, notify that we are skipping.
                        printf(YELLOW "\n*** No update function defined for %s, skipping update step\n" RESET, name);
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
                                printf(GREEN "\n(%s)->get_changes()\n\n" RESET, name);
                                pkg->get_changes();
                        } else {
                                printf(YELLOW "\nRemoving source directory: %s\n\n" RESET, base);
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
                printf(BOLD YELLOW "*** %zu package(s) need to be checked for an update manually:\n" RESET, skipped_pkgs.len);
                for (size_t i = 0; i < skipped_pkgs.len; ++i) {
                        printf(YELLOW "  %s\n" RESET, skipped_pkgs.data[i]);
                        free(skipped_pkgs.data[i]);
                }

                dyn_array_free(skipped_pkgs);
                printf(UNDERLINE YELLOW "\nUse the --force option to force the update\n\n" RESET);
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
        const char *kwds[] = FORGE_LEXER_C_KEYWORDS;
        for (size_t i = 0; i < line_count; ++i) {
                colored_lines[i] = forge_colors_code_to_string(lines[i], kwds);
                free(lines[i]);
        }
        free(lines);

        forge_viewer *m = forge_viewer_alloc(colored_lines, line_count, 0);
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

        // Find the longest filename length (excluding .h)
        size_t max_len = 0;
        for (size_t i = 0; files[i]; ++i) {
                if (strcmp(files[i], ".") && strcmp(files[i], "..") && strcmp(files[i], "forge.h")) {
                        size_t len = 0;
                        for (size_t j = 0; files[i][j]; ++j) {
                                if (files[i][j] == '.') {
                                        len = j; // Length up to the period
                                        break;
                                }
                        }
                        if (len > max_len) {
                                max_len = len;
                        }
                }
        }

        for (size_t i = 0; files[i]; ++i) {
                if (strcmp(files[i], ".") && strcmp(files[i], "..") && strcmp(files[i], "forge.h")) {
                        forge_str include = forge_str_from("#include <forge/");
                        forge_str_concat(&include, files[i]);
                        forge_str_append(&include, '>');

                        int per = 0;
                        for (size_t j = 0; files[i][j]; ++j) {
                                if (files[i][j] == '.') {
                                        per = j;
                                        break;
                                }
                        }
                        files[i][per] = 0;

                        char format[32] = {0};
                        snprintf(format, sizeof(format), "API %%-%zus: %%s\n", max_len);

                        printf(format, files[i], forge_str_to_cstr(&include));

                        forge_str_destroy(&include);
                }
                free(files[i]);
        }

        free(files);
        char format[32] = {0};
        snprintf(format, sizeof(format), "API %%-%zus: %%s\n", max_len);
        printf(format, "forge", "#include <forge/forge.h> [includes all headers]");
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
                        if (forge_utils_regex(names->data[i], name)) {
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
        char *clone = forge_cstr_builder("git clone ", name, NULL);
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
        char *repo_path = forge_cstr_builder(C_MODULE_DIR_PARENT, repo_name, NULL);
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

                char *repo_path = forge_cstr_builder(C_MODULE_DIR_PARENT, dirs[i], NULL);
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
        char *script = "set -e\n"
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
        char *new_repo_path = forge_cstr_builder(C_MODULE_DIR_PARENT, repo_name, NULL);
        char *copy_cmd = forge_cstr_builder("cp ", C_MODULE_USER_DIR, "*.c ", new_repo_path, " 2>/dev/null || true", NULL); // Handle no .c files
        char *del_cmd = forge_cstr_builder("rm -f ", C_MODULE_USER_DIR, "*.c", NULL);
        char *add_origin_cmd = forge_cstr_builder("git remote add origin ", repo_url, NULL);

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
        const char *kwds[] = FORGE_LEXER_C_KEYWORDS;

        for (size_t i = 0; apis[i]; ++i) {
                if (!strcmp(apis[i], "..") || !strcmp(apis[i], ".")) {
                        continue;
                }

                char *path = forge_cstr_builder(FORGE_API_HEADER_DIR, "/", apis[i], NULL);
                char **lines = forge_io_read_file_to_lines(path);

                for (size_t j = 0; lines[j]; ++j) {
                        dyn_array_append(combined, forge_colors_code_to_string(lines[j], kwds));
                        free(lines[j]);
                }

                free(lines);
                free(path);
                free(apis[i]);
        }

        forge_viewer *m = forge_viewer_alloc(combined.data, combined.len, 0);
        forge_viewer_display(m);
        forge_viewer_free(m);

        for (size_t i = 0; i < combined.len; ++i) {
                free(combined.data[i]);
        }
        dyn_array_free(combined);
        free(apis);
}

void
savedep(forge_context *ctx,
        const char    *name)
{
        sqlite3_stmt *stmt;
        const char *sql = "SELECT installed, is_explicit FROM Pkgs WHERE name = ?;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        int installed = 0, is_explicit = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                installed = sqlite3_column_int(stmt, 0);
                is_explicit = sqlite3_column_int(stmt, 1);
        } else {
                fprintf(stderr, RED "Package '%s' not found in database.\n" RESET, name);
                sqlite3_finalize(stmt);
                return;
        }
        sqlite3_finalize(stmt);

        if (!installed) {
                fprintf(stderr, YELLOW "Package '%s' is not installed.\n" RESET, name);
                return;
        }

        if (is_explicit) {
                printf(GREEN "Package '%s' is already explicitly installed.\n" RESET, name);
                return;
        }

        // Update the package to be explicitly installed
        const char *update_sql = "UPDATE Pkgs SET is_explicit = 1 WHERE name = ?;";
        rc = sqlite3_prepare_v2(ctx->db, update_sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
                printf(GREEN BOLD "*** Package '%s' has been saved.\n" RESET, name);
        } else {
                fprintf(stderr, RED "Error updating package '%s': %s\n" RESET, name, sqlite3_errmsg(ctx->db));
        }

        sqlite3_finalize(stmt);
}

void
list_deps(const forge_context *ctx)
{
        sqlite3 *db = ctx->db;
        sqlite3_stmt *stmt;

        // Query all installed packages with is_explicit = 0
        const char *sql_deps = "SELECT name FROM Pkgs WHERE installed = 1 AND is_explicit = 0 ORDER BY name;";
        int rc = sqlite3_prepare_v2(db, sql_deps, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

        typedef struct {
                char *name;
                str_array dependents;
        } dep_info;

        str_array dep_names = dyn_array_empty(str_array);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                dyn_array_append(dep_names, strdup(name));
        }
        sqlite3_finalize(stmt);

        if (dep_names.len == 0) {
                printf(YELLOW "No dependency packages found.\n" RESET);
                dyn_array_free(dep_names);
                return;
        }

        // Collect dependents for each dependency package
        dep_info *dep_infos = calloc(dep_names.len, sizeof(dep_info));
        size_t max_name_len = strlen("Dependency");
        size_t max_dependents_len = strlen("Required By");

        for (size_t i = 0; i < dep_names.len; ++i) {
                dep_infos[i].name = dep_names.data[i];
                dep_infos[i].dependents = dyn_array_empty(str_array);
                max_name_len = MAX(max_name_len, strlen(dep_infos[i].name));

                const char *sql_dependents =
                        "SELECT p.name FROM Pkgs p "
                        "JOIN Deps d ON p.id = d.pkg_id "
                        "WHERE d.dep_id = (SELECT id FROM Pkgs WHERE name = ?) "
                        "AND p.installed = 1 ORDER BY p.name;";
                rc = sqlite3_prepare_v2(db, sql_dependents, -1, &stmt, NULL);
                CHECK_SQLITE(rc, db);

                sqlite3_bind_text(stmt, 1, dep_infos[i].name, -1, SQLITE_STATIC);

                char dependents_buf[1024] = {0};
                size_t buf_len = 0;
                int first = 1;
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *dep_name = (const char *)sqlite3_column_text(stmt, 0);
                        size_t dep_len = strlen(dep_name);
                        if (buf_len + dep_len + (first ? 0 : 2) < sizeof(dependents_buf) - 1) {
                                if (!first) {
                                        strcat(dependents_buf, ", ");
                                        buf_len += 2;
                                }
                                strcat(dependents_buf, dep_name);
                                buf_len += dep_len;
                                dyn_array_append(dep_infos[i].dependents, strdup(dep_name));
                                first = 0;
                        }
                }
                sqlite3_finalize(stmt);

                if (dep_infos[i].dependents.len == 0) {
                        strcpy(dependents_buf, "(none)");
                        buf_len = strlen("(none)");
                }
                max_dependents_len = MAX(max_dependents_len, buf_len);
        }

        printf(GREEN BOLD "Dependency Packages:\n" RESET);
        printf("%-*s  %-*s\n",
               (int)max_name_len, "Dependency",
               (int)max_dependents_len, "Required By");
        printf("%-*s  %-*s\n",
               (int)max_name_len, "----------",
               (int)max_dependents_len, "-----------");

        for (size_t i = 0; i < dep_names.len; ++i) {
                char dependents_buf[1024] = {0};
                if (dep_infos[i].dependents.len == 0) {
                        strcpy(dependents_buf, "(none)");
                } else {
                        int first = 1;
                        for (size_t j = 0; j < dep_infos[i].dependents.len; ++j) {
                                if (!first) strcat(dependents_buf, ", ");
                                strcat(dependents_buf, dep_infos[i].dependents.data[j]);
                                first = 0;
                        }
                }
                printf("%-*s  %-*s\n",
                       (int)max_name_len, dep_infos[i].name,
                       (int)max_dependents_len, dependents_buf);
        }

        // Clean up
        for (size_t i = 0; i < dep_names.len; ++i) {
                for (size_t j = 0; j < dep_infos[i].dependents.len; ++j) {
                        free(dep_infos[i].dependents.data[j]);
                }
                dyn_array_free(dep_infos[i].dependents);
                free(dep_infos[i].name);
        }
        free(dep_infos);
        dyn_array_free(dep_names);
}

void
show_commands_for_bash_completion(void)
{
        const char *s[] = CLI_CMDS;
        for (size_t i = 0; i < sizeof(s)/sizeof(*s); ++i) {
                if (i != 0) putchar(' ');
                printf("%s", s[i]);
        }
}

void
show_options_for_bash_completion(void)
{
        const char *s[] = CLI_OPTIONS;
        for (size_t i = 0; i < sizeof(s)/sizeof(*s); ++i) {
                if (i != 0) putchar(' ');
                printf("%s", s[i]);
        }
}

void
edit_install(forge_context *ctx)
{
        {
                struct termios term;
                forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term);
                forge_ctrl_clear_terminal();
                printf(YELLOW BOLD "Note:\n" RESET);
                printf(YELLOW BOLD "*" RESET " You are about to enter a choice mode.\n");
                printf(YELLOW BOLD "*" RESET " Use the up/down arrow keys to choose different\n");
                printf(YELLOW BOLD "*" RESET " packages and use [enter] or [space] to select that package.\n");
                printf(YELLOW BOLD "*" RESET " Packages prefixed with `<*>` are marked as installed\n");
                printf(YELLOW BOLD "*" RESET " and ones that are marked with `< >` are uninstalled.\n");
                printf(YELLOW BOLD "*" RESET " Use `q` to end or C-c to cancel\n\n");
                printf("Press any key to continue...\n");
                char ch;
                (void)forge_ctrl_get_input(&ch);
                forge_ctrl_clear_terminal();
                forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);
        }

        sqlite3_stmt *stmt;
        const char *sql = "SELECT name, installed FROM Pkgs ORDER BY name;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        const int installed_flag = 1 << 0;
        const int modified_flag = 1 << 1;

        str_array pkgnames = dyn_array_empty(str_array);
        int_array installed = dyn_array_empty(int_array);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                int is_installed = sqlite3_column_int(stmt, 1);
                char *entry = forge_cstr_builder(is_installed ? "<*> " : "< > ", name, NULL);
                dyn_array_append(pkgnames, entry);
                dyn_array_append(installed, is_installed ? installed_flag : 0);
        }

        sqlite3_finalize(stmt);

        assert(pkgnames.len == installed.len);

        if (pkgnames.len == 0) {
                info_major("No packages are registered", 1);
                dyn_array_free(pkgnames);
                dyn_array_free(installed);
                return;
        }

        size_t cpos = 0;
        while (1) {
                int idx = forge_chooser("Toggle `installed` flag", (const char **)pkgnames.data, pkgnames.len, cpos);
                if (idx == -1) break;
                if (installed.data[idx] & installed_flag) {
                        pkgnames.data[idx][1] = ' ';
                        installed.data[idx] &= ~installed_flag;
                        installed.data[idx] |= modified_flag;
                } else {
                        pkgnames.data[idx][1] = '*';
                        installed.data[idx] |= installed_flag;
                        installed.data[idx] |= modified_flag;
                }
                cpos = (size_t)idx;
        }

        good_major("Updating installed status...", 1);

        // Update database for modified entries
        const char *sql_update = "UPDATE Pkgs SET installed = ? WHERE name = ?;";
        rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        for (size_t i = 0; i < installed.len; ++i) {
                if (installed.data[i] & modified_flag) {
                        // Extract package name (skip the "<*> " or "< > " prefix)
                        const char *name = pkgnames.data[i] + 4;
                        int is_installed = (installed.data[i] & installed_flag) ? 1 : 0;

                        sqlite3_bind_int(stmt, 1, is_installed);
                        sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

                        rc = sqlite3_step(stmt);
                        if (rc != SQLITE_DONE) {
                                fprintf(stderr, "Failed to update installed status for %s: %s\n",
                                        name, sqlite3_errmsg(ctx->db));
                        }
                        sqlite3_reset(stmt);

                        printf(GREEN "Updated %s: %s\n" RESET, name,
                               is_installed ? "Installed" : "Uninstalled");
                }
        }

        sqlite3_finalize(stmt);

        dyn_array_free(pkgnames);
        dyn_array_free(installed);
}

void
interactive(forge_context *ctx)
{
        struct termios term;
        forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term);
        forge_ctrl_clear_terminal();
        printf(YELLOW BOLD "Note:\n" RESET);
        printf(YELLOW BOLD "*" RESET " You are about to enter interactive mode.\n");
        printf(YELLOW BOLD "*" RESET " Use up/down arrow keys to select packages.\n");
        printf(YELLOW BOLD "*" RESET " Press [enter] or [space] to toggle install/uninstall.\n");
        printf(YELLOW BOLD "*" RESET " Packages prefixed with `<*>` are installed, `< >` are not.\n");
        printf(YELLOW BOLD "*" RESET " Press `q` to confirm selections or C-c to cancel.\n\n");
        printf("Press any key to continue...\n");
        char ch;
        (void)forge_ctrl_get_input(&ch);
        forge_ctrl_clear_terminal();

        // Query all packages
        sqlite3_stmt *stmt;
        const char *sql = "SELECT name, version, description, installed FROM Pkgs ORDER BY name;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        const uint32_t installed_flag = 1 << 0;
        const uint32_t modified_flag = 1 << 1;

        str_array display_entries = dyn_array_empty(str_array);
        str_array pkg_names = dyn_array_empty(str_array);
        int_array status = dyn_array_empty(int_array);
        str_array names = dyn_array_empty(str_array);
        str_array versions = dyn_array_empty(str_array);
        str_array descriptions = dyn_array_empty(str_array);
        int_array installed = dyn_array_empty(int_array);
        size_t max_name_len = strlen("Name");
        size_t max_version_len = strlen("Version");
        size_t max_desc_len = strlen("Description");

        // Collect package data and calculate max widths
        while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                const char *version = (const char *)sqlite3_column_text(stmt, 1);
                const char *description = (const char *)sqlite3_column_text(stmt, 2);
                int is_installed = sqlite3_column_int(stmt, 3);

                // Update max lengths
                max_name_len = MAX(max_name_len, strlen(name));
                max_version_len = MAX(max_version_len, version ? strlen(version) : 0);
                max_desc_len = MAX(max_desc_len, description ? strlen(description) : strlen("(none)"));

                // Store data for later entry creation
                dyn_array_append(names, strdup(name));
                dyn_array_append(versions, version ? strdup(version) : strdup(""));
                dyn_array_append(descriptions, description ? strdup(description) : strdup("(none)"));
                dyn_array_append(installed, is_installed);
                dyn_array_append(pkg_names, strdup(name));
                dyn_array_append(status, is_installed ? installed_flag : 0);
        }
        sqlite3_finalize(stmt);

        if (pkg_names.len == 0) {
                info_major("No packages are registered", 1);
                goto dyn_clean;
        }

        // Create display entries after calculating max lengths
        for (size_t i = 0; i < names.len; ++i) {
                char *entry = (char *)malloc(max_name_len + max_version_len + max_desc_len + 20);
                snprintf(entry, max_name_len + max_version_len + max_desc_len + 20,
                         "%s %-*s  %-*s  %-*s",
                         installed.data[i] ? "<*>" : "< >",
                         (int)max_name_len, names.data[i],
                         (int)max_version_len, versions.data[i],
                         (int)max_desc_len, descriptions.data[i]);
                dyn_array_append(display_entries, entry);
        }

        size_t cpos = 0;
        while (1) {
                int idx = forge_chooser("Select packages to install/uninstall", (const char **)display_entries.data, display_entries.len, cpos);
                if (idx == -1) break;
                status.data[idx] ^= installed_flag; // Toggle installed flag
                status.data[idx] |= modified_flag;  // Mark as modified
                // Update display entry
                display_entries.data[idx][1] = (status.data[idx] & installed_flag) ? '*' : ' ';
                cpos = (size_t)idx;
        }

        forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);

        // Collect packages to install/uninstall
        str_array to_install = dyn_array_empty(str_array);
        str_array to_uninstall = dyn_array_empty(str_array);

        for (size_t i = 0; i < status.len; ++i) {
                if (status.data[i] & modified_flag) {
                        int is_installed = pkg_is_installed(ctx, pkg_names.data[i]);
                        int target_state = (status.data[i] & installed_flag) ? 1 : 0;
                        if (is_installed != target_state) {
                                if (target_state) {
                                        dyn_array_append(to_install, strdup(pkg_names.data[i]));
                                } else {
                                        dyn_array_append(to_uninstall, strdup(pkg_names.data[i]));
                                }
                        }
                }
        }

        // Perform uninstallations
        if (to_uninstall.len > 0) {
                printf(GREEN BOLD "*** Processing Uninstallations\n" RESET);
                uninstall_pkg(ctx, &to_uninstall);
        }

        // Perform installations
        if (to_install.len > 0) {
                printf(GREEN BOLD "*** Processing Installations\n" RESET);
                install_pkg(ctx, &to_install, 0);
        }

        for (size_t i = 0; i < display_entries.len; ++i) free(display_entries.data[i]);
        for (size_t i = 0; i < pkg_names.len; ++i) free(pkg_names.data[i]);
        for (size_t i = 0; i < names.len; ++i) free(names.data[i]);
        for (size_t i = 0; i < versions.len; ++i) free(versions.data[i]);
        for (size_t i = 0; i < descriptions.len; ++i) free(descriptions.data[i]);
        for (size_t i = 0; i < to_install.len; ++i) free(to_install.data[i]);
        for (size_t i = 0; i < to_uninstall.len; ++i) free(to_uninstall.data[i]);

        dyn_array_free(to_install);
        dyn_array_free(to_uninstall);
 dyn_clean:
        dyn_array_free(display_entries);
        dyn_array_free(pkg_names);
        dyn_array_free(names);
        dyn_array_free(versions);
        dyn_array_free(descriptions);
        dyn_array_free(status);
}

int
try_first_time_startup(int argc)
{
        int exists = cio_file_exists(DB_FP);

        if (exists && argc == 0) {
                forge_flags_usage();
        } else if (!exists) {
                printf("Superuser access is required the first time forge is ran.\n");
                assert_sudo();

                /* const char *ans[] = {"Yes, I want some premade packages", "No, I want to start from scratch"}; */
                /* int choice = forge_chooser("Would you like to install the offical forge repository?", */
                /*                            (const char **)ans, sizeof(ans)/sizeof(*ans), 0); */
                int choice = forge_chooser_yesno("Would you like to install the offical forge repository?", NULL, 1);

                init_env();

                if (choice == -1) {
                        printf("Something went wrong... :(\n");
                } else if (choice == 1) {
                        add_repo("https://github.com/malloc-nbytes/forge-modules.git");
                }

                return choice;
        }

        return -1;
}

int
main(int argc, char **argv)
{
        ++argv, --argc;
        clap_init(argc, argv);

        int first_time_setup = -1;
        if ((first_time_setup = try_first_time_startup(argc)) != -1) {
                // first_time_setup will be > -1 if it is first time
                g_config.flags |= FT_REBUILD;
        }

        forge_context ctx = (forge_context) {
                .db = init_db(DB_FP),
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
                if (arg.hyphc == 1 && arg.start[0] == FLAG_1HY_HELP[0]) {
                        if (arg.eq) { forge_flags_help(arg.eq); }
                        forge_flags_usage();
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_HELP)) {
                        if (arg.eq) { forge_flags_help(arg.eq); }
                        forge_flags_usage();
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_FORCE)) {
                        g_config.flags |= FT_FORCE;
                }

                else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_LIST)) {
                        list_registerd_pkgs(&ctx);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_DEPS)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires an argument", CMD_DEPS);
                        }
                        deps(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_INSTALL)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", CMD_INSTALL);
                        assert_sudo();
                        install_pkg(&ctx, &names, 0);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_UNINSTALL)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", CMD_UNINSTALL);
                        assert_sudo();
                        uninstall_pkg(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_REBUILD)) {
                        g_config.flags |= FT_REBUILD;
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_NEW)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", CMD_NEW);
                        assert_sudo();
                        new_pkg(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_EDIT)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", CMD_EDIT);
                        assert_sudo();
                        edit_c_module(&names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_UPDATE)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        assert_sudo();
                        update_pkgs(&ctx, &names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_DUMP)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", CMD_DUMP);
                        for (size_t i = 0; i < names.len; ++i)
                                api_dump(names.data[i], 0);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_SYNC)) {
                        g_config.flags |= FT_SYNC;
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_DROP_BROKEN_PKGS)) {
                        g_config.flags |= FT_DROP_BROKEN_PKGS;
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_DROP)) {
                        assert_sudo();
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", CMD_DROP);
                        for (size_t i = 0; i < names.len; ++i)
                                drop_pkg(&ctx, names.data[i]);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_FILES)) {
                        if (!clap_next(&arg)) { err_wargs("flag `%s` requires an argument", CMD_FILES); }
                        list_files(&ctx, arg.start, 0);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_COPYING)) {
                        forge_flags_copying();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_DEPGRAPH)) {
                        depgraph_dump(&ctx.dg);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_API)) {
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
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_EDITCONF)) {
                        assert_sudo();
                        editconf();
                        printf(YELLOW BOLD "=== NOTE ===\n" RESET YELLOW);
                        printf(YELLOW "For these changes to take effect, you need\n");
                        printf(YELLOW "to rebuild forge. To do this, run `forge %s`\n", CMD_UPDATEFORGE);
                        printf(YELLOW "This action requires an internet connection.\n");
                        printf(YELLOW BOLD "============\n" RESET);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_UPDATEFORGE)) {
                        assert_sudo();
                        updateforge();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_RESTORE)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires an argument", CMD_DEPS);
                        }
                        assert_sudo();
                        restore_pkg(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_APILIST)) {
                        apilist();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_SEARCH)) {
                        str_array names = dyn_array_empty(str_array);
                        while (clap_next(&arg)) {
                                dyn_array_append(names, strdup(arg.start));
                        }
                        if (names.len == 0) err_wargs("flag `%s` requires an argument", CMD_SEARCH);
                        pkg_search(&names);
                        for (size_t i = 0; i < names.len; ++i) { free(names.data[i]); }
                        dyn_array_free(names);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_ADD_REPO)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires a Github repo link", CMD_ADD_REPO);
                        }
                        assert_sudo();
                        g_config.flags |= FT_REBUILD;
                        add_repo(arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_DROP_REPO)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires an argument", CMD_DROP_REPO);
                        }
                        assert_sudo();
                        g_config.flags |= FT_REBUILD;
                        drop_repo(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_LIST_REPOS)) {
                        list_repos();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_REPO_COMPILE_TEMPLATE)) {
                        create_repo_compile_template();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_CREATE_REPO)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires a repo name", CMD_CREATE_REPO);
                        }
                        char *repo_name = strdup(arg.start);
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires a repo url", CMD_CREATE_REPO);
                        }
                        char *repo_url = strdup(arg.start);
                        assert_sudo();
                        create_repo(repo_name, repo_url);
                        g_config.flags |= FT_REBUILD;
                        free(repo_name);
                        free(repo_url);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_CLEAN)) {
                        assert_sudo();
                        clean_pkgs(&ctx);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_LIB)) {
                        show_lib();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_SAVE_DEP)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires a name", CMD_SAVE_DEP);
                        }
                        assert_sudo();
                        savedep(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_LIST_DEPS)) {
                        list_deps(&ctx);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_COMMANDS)) {
                        show_commands_for_bash_completion();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_OPTIONS)) {
                        show_options_for_bash_completion();
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_EDIT_INSTALL)) {
                        assert_sudo();
                        edit_install(&ctx);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, CMD_INT)) {
                        assert_sudo();
                        interactive(&ctx);
                }

                else if (arg.hyphc == 1) { // one hyph options
                        for (size_t i = 0; arg.start[i]; ++i) {
                                char c = arg.start[i];
                                if      (c == FLAG_1HY_REBUILD[0]) g_config.flags |= FT_REBUILD;
                                else if (c == FLAG_1HY_SYNC[0])    g_config.flags |= FT_SYNC;
                                else                               err_wargs("unknown option `%c`", c);
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

                // Register packages, preserving is_explicit status
                for (size_t i = 0; i < indices.len; ++i) {
                        pkg *pkg = ctx.pkgs.data[indices.data[i]];
                        const char *name = pkg->name();

                        // Query the current is_explicit status
                        int is_explicit = 0;
                        sqlite3_stmt *stmt;
                        const char *sql = "SELECT is_explicit FROM Pkgs WHERE name = ?;";
                        int rc = sqlite3_prepare_v2(ctx.db, sql, -1, &stmt, NULL);
                        CHECK_SQLITE(rc, ctx.db);

                        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                                is_explicit = sqlite3_column_int(stmt, 0);
                        }
                        sqlite3_finalize(stmt);

                        // Register package with the existing is_explicit value
                        register_pkg(&ctx, pkg, is_explicit);
                }
        }

        if (first_time_setup == 0) { // first time and chose not to sync
                printf(YELLOW BOLD "Remark:\n" RESET);
                printf(YELLOW BOLD "* " RESET "If you want to add the official repository, run:\n");
                printf(YELLOW BOLD "* " "  forge add-repo https://github.com/malloc-nbytes/forge-modules.git\n" RESET);
                printf(YELLOW BOLD "* " RESET "or don't if you just want to start from scratch.\n");
        }
        if (first_time_setup > -1) { // first time and chose to sync
                printf(YELLOW BOLD "Done!\n" RESET);
                printf(YELLOW BOLD "* " RESET "You can now invoke forge regularly.\n");
                printf(YELLOW BOLD "* " RESET "Note: You may want to edit your config, run:\n");
                printf(YELLOW BOLD "* " "  forge editconf\n" RESET);
                printf(YELLOW BOLD "* " RESET "After this, do:\n");
                printf(YELLOW BOLD "* " "  forge updateforge\n" RESET);
                printf(YELLOW BOLD "* " RESET "\n");
                printf(YELLOW BOLD "* " RESET "To get started, run:\n");
                printf(YELLOW BOLD "* " "  forge -r new author@pkgname\n" RESET);
                printf(YELLOW BOLD "* " RESET "to start forging your packages.\n");
                printf(YELLOW BOLD "* " RESET "Do " YELLOW BOLD "forge -h" RESET " to view all help information.\n");
        }

        dyn_array_free(indices);
        cleanup_forge_context(&ctx);
        return 0;
}
