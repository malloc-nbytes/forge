/*
 * forge: Forge your system
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

#include "forge/pkg.h"
#include "forge/colors.h"
#include "forge/chooser.h"
#include "forge/cstr.h"
#include "forge/cmd.h"
#include "forge/err.h"
#include "forge/arg.h"
#include "forge/io.h"
#include "forge/smap.h"
#include "forge/conf.h"
#include "forge/ctrl.h"
#include "forge/lexer.h"
#include "forge/viewer.h"
#include "forge/str.h"
#include "forge/utils.h"
#include "forge/str.h"

#include "config.h"
#include "depgraph.h"
#include "flags.h"
#include "utils.h"
#include "paths.h"
#include "msgs.h"

#include "sqlite3.h"

#include <sys/mount.h>
#include <sys/types.h>
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
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
/*#define _GNU_SOURCE*/
#define __USE_GNU
#include <sched.h>
#include <time.h>

#define FORGE_C_MODULE_TEMPLATE                                         \
        "#include <forge/forge.h>\n"                                    \
        "\n"                                                            \
        "char *deps[] = {NULL}; // Must be NULL terminated\n"           \
        "\n"                                                            \
        "char *getname(void) { /*return \"author@pkg_name\";*/ }\n"     \
        "char *getver(void) { return \"1.0.0\"; }\n"                    \
        "char *getdesc(void) { return \"My Description\"; }\n"          \
        "char *getweb(void) { return \"Package Website\"; }\n"          \
        "char **getdeps(void) { return deps; }\n"                       \
        "char *download(void) {\n"                                      \
        "        return NULL; // should return the name of the final directory!\n" \
        "}\n"                                                           \
        "int build(void) { return 1; }\n"                               \
        "int install(void) { return 1; }\n"                             \
        "int uninstall(void) { return 1; }\n"                           \
        "int update(void) {\n"                                          \
        "        return 0; // return 1 if it needs a rebuild, 0 otherwise\n" \
        "}\n"                                                           \
        "int get_changes(void) {\n"                                     \
        "        // pull in the new changes if update() returns 1\n"    \
        "        return 0;\n"                                           \
        "}\n"                                                           \
        "\n"                                                            \
        "FORGE_GLOBAL pkg package = {\n"                                \
        "        .name = getname,\n"                                    \
        "        .ver = getver,\n"                                      \
        "        .desc = getdesc,\n"                                    \
        "        .web = getweb,\n"                                      \
        "        .deps = NULL,\n"                                       \
        "        .download = download,\n"                               \
        "        .build = build,\n"                                     \
        "        .install = install,\n"                                 \
        "        .uninstall = uninstall,\n"                             \
        "        .update = forge_pkg_git_update, // or define your own if not using git\n" \
        "         \n"                                                   \
        "         // Make this NULL to just re-download the source code\n" \
        "         // or define your own if not using git\n"             \
        "        .get_changes = forge_pkg_git_pull,\n"                  \
        "};"

#define CHECK_SQLITE(rc, db)                                    \
        do {                                                    \
                if (rc != SQLITE_OK) {                          \
                        fprintf(stderr, "SQLite error: %s\n",   \
                                sqlite3_errmsg(db));            \
                        sqlite3_close(db);                      \
                        exit(1);                                \
                }                                               \
        } while (0)

DYN_ARRAY_TYPE(void *, handle_array);
DYN_ARRAY_TYPE(pkg *, pkg_ptr_array);

typedef struct {
        sqlite3 *db;
        struct {
                handle_array handles; // assert(handles.len == paths.len)
                str_array paths;      // assert(paths.len == handles.len)
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
        .flags = 0x0000,
};

// unistd.h
extern char **environ;

char *g_fakeroot = NULL;
static char **g_saved_argv = NULL;
static int   g_saved_argc = 0;

void
assert_sudo(void)
{
        if (geteuid() != 0) {
                forge_err(BOLD YELLOW "* " RESET "This action requires " BOLD YELLOW "superuser privileges" RESET);
        }
}

void
clear_package_files_from_db(forge_context *ctx,
                            const char    *name,
                            int            pkg_id)
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
                "id      INTEGER PRIMARY KEY AUTOINCREMENT,"
                "pkg_id  INTEGER NOT NULL,"
                "path    TEXT    NOT NULL,"
                "size    INTEGER,"
                "mode    INTEGER,"
                "mtime   INTEGER,"
                "FOREIGN KEY (pkg_id) REFERENCES Pkgs(id) ON DELETE CASCADE,"
                "UNIQUE(pkg_id, path));";
        rc = sqlite3_exec(db, create_files, NULL, NULL, NULL);
        CHECK_SQLITE(rc, db);

        return db;
}

void
construct_depgraph(forge_context *ctx)
{
        for (size_t i = 0; i < ctx->pkgs.len; ++i) {
                // TODO: assert name
                depgraph_insert_pkg(&ctx->dg, ctx->pkgs.data[i]->name());
        }

        for (size_t i = 0; i < ctx->pkgs.len; ++i) {
                if (!ctx->pkgs.data[i]->deps) continue;

                char *name = ctx->pkgs.data[i]->name();
                char **deps = ctx->pkgs.data[i]->deps();
                for (size_t j = 0; deps[j]; ++j) {
                        depgraph_add_dep(&ctx->dg, name, deps[j]);
                }
        }
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
                        snprintf(path, 512, "%s/%s", MODULE_LIB_DIR, entry->d_name);

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
                if (ctx->dll.handles.data[i]) {
                        dlclose(ctx->dll.handles.data[i]);
                        free(ctx->dll.paths.data[i]);
                }
        }
        dyn_array_free(ctx->dll.handles);
        dyn_array_free(ctx->dll.paths);
        dyn_array_free(ctx->pkgs);
        depgraph_destroy(&ctx->dg);
}

static int
init_env(void)
{
        // Database location
        if (mkdir(DATABASE_DIR, 0755) != 0) {
                if (errno == EEXIST) return 0;
                fprintf(stderr, "could not create path: %s, %s\n", DATABASE_DIR, strerror(errno));
        }
        sqlite3 *db = init_db(DATABASE_FP);
        if (!db) {
                fprintf(stderr, "could not initialize database: %s\n", DATABASE_FP);
        }
        sqlite3_close(db);

        // Module locations
        if (mkdir_p_wmode(MODULE_LIB_DIR, 0755) != 0) {
                if (errno == EEXIST) return 0;
                fprintf(stderr, "could not create path: %s, %s\n", MODULE_LIB_DIR, strerror(errno));
        }
        if (mkdir_p_wmode(C_MODULE_USER_DIR, 0755) != 0) {
                if (errno == EEXIST) return 0;
                fprintf(stderr, "could not create path: %s, %s\n", C_MODULE_USER_DIR, strerror(errno));
        }

        // Pkg source location
        if (mkdir_p_wmode(PKG_SOURCE_DIR, 0755) != 0) {
                if (errno == EEXIST) return 0;
                fprintf(stderr, "could not create path: %s, %s\n", PKG_SOURCE_DIR, strerror(errno));
        }

        return 1;
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
create_skeleton(const char *root)
{
        assert(root);

        info(0, "Creating fakeroot skeleton\n");

        const char *paths[] = {
                "bin", "etc", "lib", "opt", "home",
                "usr", "usr/bin", "usr/lib", "usr/include", "usr/lib64", "usr/share", "usr/libexec",
                "usr/local", "usr/local/share", "usr/local/src", "usr/local/include", "usr/local/bin",
                "usr/local/lib", "usr/local/lib64", "usr/local/sbin", "usr/local/opt",
                "var", "dev", "proc", "sys", "run", "tmp", "sbin", "lib64", "buildsrc", NULL,
        };

        for (size_t i = 0; paths[i]; ++i) {
                char path[PATH_MAX] = {0};
                snprintf(path, sizeof(path), "%s/%s", root, paths[i]);
                if (mkdir(path, 0755) == -1 && errno != EEXIST) {
                        perror("mkdir");
                }
        }
}

void die(const char *msg) { perror(msg); exit(1); }

void
sync(void)
{
        assert_sudo();

        CD(C_MODULE_DIR_PARENT, {
                fprintf(stderr, "could cd to path: %s, %s\n", C_MODULE_DIR, strerror(errno));
                return;
        });

        char **files = ls(".");

        for (size_t i = 0; files[i]; ++i) {
                if (is_git_dir(files[i])) {
                        info_builder(0, "Syncing [", YELLOW, files[i], RESET, "]\n", NULL);
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
rebuild_pkgs(void)
{
        assert_sudo();

        info(1, "Rebuilding package modules\n");

        char **dirs = ls(C_MODULE_DIR_PARENT);
        for (size_t d = 0; dirs[d]; ++d) {
                if (!strcmp(dirs[d], ".") || !strcmp(dirs[d], "..")) {
                        free(dirs[d]);
                        continue;
                }
                char *abspath = forge_cstr_builder(C_MODULE_DIR_PARENT, "/", dirs[d], NULL);
                DIR *dir = opendir(abspath);
                if (!dir) {
                        perror("opendir");
                        return;
                }

                str_array files = dyn_array_empty(str_array);
                struct dirent *entry;
                while ((entry = readdir(dir))) {
                        // Check if the file name ends with ".c"
                        if (entry->d_type == DT_REG && strstr(entry->d_name, ".c") != NULL) {
                                size_t len = strlen(entry->d_name);
                                if (len >= 2 && strcmp(entry->d_name + len - 2, ".c") == 0) {
                                        char *filename = strdup(entry->d_name);
                                        filename[len - 2] = '\0'; // Remove ".c"
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
                        size_t loading = (size_t)(((float)i/(float)files.len)*10.f);
                        putchar('[');
                        for (size_t i = 0; i < 10; ++i) {
                                if (i > loading) {
                                        putchar(' ');
                                } else {
                                        putchar('*');
                                }
                        }
                        printf("] ");

                        char buf[256] = {0};
                        sprintf(buf, "gcc -Wextra -Wall -Werror -shared -fPIC %s.c -lforge -L/usr/local/lib -o" MODULE_LIB_DIR "/%s.so -I../include",
                                files.data[i], files.data[i]);
                        printf("%s.c\n", files.data[i]);
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
                for (size_t i = 0; i < files.len; ++i) {
                        free(files.data[i]);
                }
                dyn_array_free(files);
                closedir(dir);
                free(dirs[d]);
                free(abspath);
        }
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

void
register_pkg(forge_context *ctx, pkg *pkg, int is_explicit)
{
        if (!pkg->name) {
                forge_err("register_pkg(): pkg (unknown) does not have a name");
        }
        if (!pkg->ver) {
                forge_err_wargs("register_pkg(): pkg %s does not have a version", pkg->name());
        }
        if (!pkg->desc) {
                forge_err_wargs("register_pkg(): pkg %s does not have a description", pkg->name());
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
                //info_builder(1, "Registered package: ", YELLOW, name, RESET, "\n", NULL);
                printf(YELLOW "*" RESET " Registered package: " YELLOW "%s" RESET "\n", name);

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
                                add_dep_to_db(ctx, get_pkg_id(ctx, name), get_pkg_id(ctx, deps[i]));
                        }
                }
        }
}

static char *
get_c_module_filepath_from_basic_name(const char *name)
{
        char **dirs = ls(C_MODULE_DIR_PARENT);
        for (size_t i = 0; dirs[i]; ++i) {
                char *module_dir = forge_cstr_builder(C_MODULE_DIR_PARENT, "/", dirs[i], NULL);
                if (forge_io_is_dir(module_dir)) {
                        CD(module_dir, {});
                        char *path = forge_cstr_builder(module_dir, "/", name, ".c", NULL);
                        if (forge_io_filepath_exists(path)) {
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

static void
restore_pkg(str_array names)
{
        for (size_t i = 0; i < names.len; ++i) {
                const char *name = names.data[i];

                // Construct the pattern to match backup files: <name>.c-<timestamp>
                char pattern[256] = {0};
                snprintf(pattern, sizeof(pattern), "%s.c-", name);

                // Get all directories under C_MODULE_DIR_PARENT
                char **dirs = ls(C_MODULE_DIR_PARENT);
                if (!dirs) {
                        forge_err_wargs("Failed to list directories in %s: %s\n", C_MODULE_DIR_PARENT, strerror(errno));
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

                        char *module_dir = forge_cstr_builder(C_MODULE_DIR_PARENT, "/", dirs[i], NULL);
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
                if (forge_io_filepath_exists(original_path)) {
                        fprintf(stderr, "Original file %s already exists, cannot restore\n", original_path);
                        free(latest_file);
                        free(target_dir);
                        return;
                }

                // Rename the latest backup file to the original name
                info_builder(0, "Restoring C module: ", YELLOW BOLD, latest_file, RESET " to ", YELLOW BOLD, original_path, RESET "\n", NULL);
                if (rename(latest_file, original_path) != 0) {
                        fprintf(stderr, "Failed to restore file %s to %s: %s\n",
                                latest_file, original_path, strerror(errno));
                        free(latest_file);
                        free(target_dir);
                        return;
                }

                info_builder(0, "Successfully restored package " YELLOW BOLD, name, RESET, "\n", NULL);
                free(latest_file);
                free(target_dir);
        }
}

static void
drop_pkg(forge_context *ctx, str_array names)
{
        for (size_t i = 0; i < names.len; ++i) {
                const char *name = names.data[i];

                // Check if package exists
                int pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        forge_err_wargs("package `%s` not found in database", name);
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
                        forge_err_wargs("Failed to delete dependencies for %s: %s\n", name, sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                // Delete the package
                const char *sql_delete_pkg = "DELETE FROM Pkgs WHERE id = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_delete_pkg, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_int(stmt, 1, pkg_id);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        forge_err_wargs("Failed to delete package %s: %s\n", name, sqlite3_errmsg(ctx->db));
                } else {
                        info_builder(0, "Successfully dropped package ", YELLOW BOLD, name, RESET, " from database\n", NULL);
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

                info_builder(0, "Creating backup of C module: ", YELLOW BOLD, pkg_new_filename.data, RESET, "\n", NULL);
                if (rename(forge_str_to_cstr(&pkg_filename), forge_str_to_cstr(&pkg_new_filename)) != 0) {
                        fprintf(stderr, "failed to rename file: %s to %s: %s\n",
                                forge_str_to_cstr(&pkg_filename), forge_str_to_cstr(&pkg_new_filename), strerror(errno));
                }

                forge_str_destroy(&pkg_filename);
                forge_str_destroy(&pkg_new_filename);

                // Remove .so file
                forge_str so_path = forge_str_from(MODULE_LIB_DIR "/");
                forge_str_concat(&so_path, name);
                forge_str_concat(&so_path, ".so");

                info_builder(0, "Removing library file: ", YELLOW BOLD, so_path.data, RESET "\n", NULL);
                if (remove(forge_str_to_cstr(&so_path)) != 0) {
                        fprintf(stderr, "failed to remove file: %s: %s\n",
                                forge_str_to_cstr(&so_path), strerror(errno));
                        //return;
                }

                forge_str_destroy(&so_path);
        }
}

static int
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

static void
sandbox(const char *pkgname)
{
        info(0, "Creating sandbox\n");

        char tmpl[256] = {0};
        sprintf(tmpl, "/tmp/pkg-%s-XXXXXX", pkgname);
        g_fakeroot = strdup(mkdtemp(tmpl));
        if (!g_fakeroot) die("mkdtemp");
        create_skeleton(g_fakeroot);
}

static void
destroy_fakeroot(void)
{
        if (g_fakeroot) {
                info(1, "Destroying fakeroot\n\n");
                unsetenv("DESTDIR");
                rmrf(g_fakeroot);
                free(g_fakeroot);
                g_fakeroot = NULL;
        }
}

static void
build_manifest(str_array *ar, const char *path)
{
        assert(g_fakeroot);

        char **basefiles = ls(path);

        for (size_t i = 0; basefiles[i]; ++i) {
                char abspath[PATH_MAX] = {0};
                const char *file = basefiles[i];

                if (!strcmp("buildsrc", file)) continue;
                if (!strcmp("..", file))       continue;
                if (!strcmp(".", file))        continue;

                strcpy(abspath, path);
                strcat(abspath, "/");
                strcat(abspath, file);

                if (forge_io_is_dir(abspath)) {
                        build_manifest(ar, abspath);
                } else {
                        dyn_array_append(*ar, strdup(abspath));
                }
        }

        for (size_t i = 0; basefiles[i]; ++i) {
                free(basefiles[i]);
        }
}

static int
copy_file_to_root(const char *src_abs,
                  const char *dst_abs,
                  sqlite3    *db,
                  int         pkg_id)
{
        struct stat st;
        if (lstat(src_abs, &st) != 0) {
                perror("lstat(src)");
                return 0;
        }

        // Handle symlinks
        if (S_ISLNK(st.st_mode)) {
                char target[PATH_MAX] = {0};
                ssize_t len = readlink(src_abs, target, sizeof(target) - 1);
                if (len < 0) {
                        perror("readlink");
                        return 0;
                }
                target[len] = '\0';

                // Remove existing file/symlink if any
                unlink(dst_abs);

                if (symlink(target, dst_abs) != 0) {
                        perror("symlink");
                        return 0;
                }

                goto db_insert;
        }

        // Regular file
        if (!S_ISREG(st.st_mode)) {
                fprintf(stderr, "Skipping non-regular file: %s\n", src_abs);
                return 1; // skip but don't fail
        }

        // Create destination directory
        {
                char *dir = strdup(dst_abs);
                char *p = strrchr(dir, '/');
                if (p) {
                        *p = '\0';
                        mkdir_p_wmode(dir, 0755);
                }
                free(dir);
        }

        // Open source
        int fd_src = open(src_abs, O_RDONLY);
        if (fd_src < 0) {
                perror("open(src)");
                return 0;
        }

        // Open/create destination (regular file)
        int fd_dst = open(dst_abs,
                          O_WRONLY | O_CREAT | O_TRUNC,
                          st.st_mode & 07777);
        if (fd_dst < 0) {
                close(fd_src);
                perror("open(dst)");
                return 0;
        }

        // Copy data
        char buf[8192];
        ssize_t n;
        while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
                if (write(fd_dst, buf, n) != n) {
                        perror("write");
                        close(fd_src);
                        close(fd_dst);
                        unlink(dst_abs);
                        return 0;
                }
        }
        close(fd_src);
        close(fd_dst);
        if (n < 0) {
                unlink(dst_abs);
                return 0;
        }

        // Restore timestamps and mode
        struct timespec ts[2] = {
                { .tv_sec = st.st_atim.tv_sec, .tv_nsec = st.st_atim.tv_nsec },
                { .tv_sec = st.st_mtim.tv_sec, .tv_nsec = st.st_mtim.tv_nsec }
        };
        utimensat(AT_FDCWD, dst_abs, ts, 0);
        chmod(dst_abs, st.st_mode & 07777);

 db_insert:
        (void)0; // silence warning

        // Insert into Files
        sqlite3_stmt *stmt;
        const char *sql =
                "INSERT OR REPLACE INTO Files (pkg_id, path, size, mode, mtime) "
                "VALUES (?, ?, ?, ?, ?);";

        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
                fprintf(stderr, "prepare failed: %s\n", sqlite3_errmsg(db));
                return 0;
        }

        sqlite3_bind_int(  stmt, 1, pkg_id);
        sqlite3_bind_text( stmt, 2, dst_abs, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, st.st_size);  // 0 for symlinks
        sqlite3_bind_int(  stmt, 4, st.st_mode & 07777);
        sqlite3_bind_int64(stmt, 5, st.st_mtim.tv_sec);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Files insert error: %s\n", sqlite3_errmsg(db));
                return 0;
        }

        return 1;
}

static void
print_file_progress(const char *realpath, size_t i, size_t total, int add) {
#define BAR_WIDTH 30

        float progress = (float)(i + 1) / (float)total;
        int filled = (int)(progress * BAR_WIDTH);

        // Move cursor up one line and clear it (only after the first iteration)
        if (i > 0) printf("\033[1A\033[2K");

        printf("%s %s\n", add ? "+++" : "---", realpath);

        printf("[");
        for (int j = 0; j < BAR_WIDTH; j++) {
                if (j < filled - 1)
                        printf("=");
                else if (j == filled - 1)
                        printf(">");
                else
                        printf(" ");
        }

        printf("] " YELLOW BOLD "%zu" RESET "/" YELLOW BOLD "%zu" RESET "\r\n",
               i + 1, total);

        fflush(stdout);
#undef BAR_WIDTH
}

static int
uninstall_pkg(forge_context *ctx, str_array names, int remove_src)
{
        assert_sudo();

        for (size_t i = 0; i < names.len; ++i) {
                const char *name = names.data[i];

                int pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        forge_err_wargs("package `%s` is not registered", name);
                        goto fail;
                }

                if (!pkg_is_installed(ctx, name)) {
                        info_builder(0, "Package ", YELLOW BOLD, name, RESET, " is not installed\n", NULL);
                        continue; // Skip, not an error
                }

                char *pkg_src_loc = NULL;
                sqlite3_stmt *stmt;
                const char *sql = "SELECT pkg_src_loc FROM Pkgs WHERE id = ?;";
                int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);
                sqlite3_bind_int(stmt, 1, pkg_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *loc = (const char *)sqlite3_column_text(stmt, 0);
                        if (loc) pkg_src_loc = strdup(loc);
                }
                sqlite3_finalize(stmt);

                // Get all files for this package
                stmt = NULL;
                sql = "SELECT path FROM Files WHERE pkg_id = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_int(stmt, 1, pkg_id);

                str_array files = dyn_array_empty(str_array);
                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                        const char *path = (const char *)sqlite3_column_text(stmt, 0);
                        dyn_array_append(files, strdup(path));
                }
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(ctx->db));
                        sqlite3_finalize(stmt);
                        goto fail_files;
                }
                sqlite3_finalize(stmt);

                char *file_count_str = forge_cstr_of_int(files.len);
                info_builder(0, "Uninstalling ", YELLOW BOLD, name, RESET, " [", YELLOW, file_count_str, RESET, "] files\n\n", NULL);
                free(file_count_str);

                // Remove installed files
                str_array failed = dyn_array_empty(str_array);

                for (size_t j = 0; j < files.len; ++j) {
                        const char *path = files.data[j];

                        print_file_progress(path, j, files.len, /*add=*/0);

                        struct stat st;
                        if (lstat(path, &st) != 0) {
                                if (errno == ENOENT) continue;
                                perror("lstat");
                                dyn_array_append(failed, strdup(path));
                                continue;
                        }

                        if (S_ISDIR(st.st_mode)) {
                                continue; // Skip dirs
                        }

                        if (unlink(path) != 0 && errno != ENOENT) {
                                perror("unlink");
                                dyn_array_append(failed, strdup(path));
                        }
                }

                // Delete file entries from DB
                {
                        const char *del_files = "DELETE FROM Files WHERE pkg_id = ?;";
                        rc = sqlite3_prepare_v2(ctx->db, del_files, -1, &stmt, NULL);
                        CHECK_SQLITE(rc, ctx->db);
                        sqlite3_bind_int(stmt, 1, pkg_id);
                        rc = sqlite3_step(stmt);
                        if (rc != SQLITE_DONE) {
                                fprintf(stderr, "Failed to delete file entries: %s\n", sqlite3_errmsg(ctx->db));
                        }
                        sqlite3_finalize(stmt);
                }

                // Mark as uninstalled and clear source location
                {
                        const char *update_pkg = NULL;
                        if (remove_src) {
                                update_pkg = "UPDATE Pkgs SET installed = 0, pkg_src_loc = NULL WHERE id = ?;";
                        } else {
                                update_pkg = "UPDATE Pkgs SET installed = 0 WHERE id = ?;";
                        }
                        rc = sqlite3_prepare_v2(ctx->db, update_pkg, -1, &stmt, NULL);
                        CHECK_SQLITE(rc, ctx->db);
                        sqlite3_bind_int(stmt, 1, pkg_id);
                        rc = sqlite3_step(stmt);
                        if (rc != SQLITE_DONE) {
                                fprintf(stderr, "Failed to update package status: %s\n", sqlite3_errmsg(ctx->db));
                        }
                        sqlite3_finalize(stmt);
                }

                // Remove source directory
                if (pkg_src_loc && remove_src) {
                        char *src_path = forge_cstr_builder(PKG_SOURCE_DIR, "/", forge_io_basename(pkg_src_loc), NULL);
                        info(1, "Removing source directory\n");
                        char *rm_cmd = forge_cstr_builder("rm -rf ", src_path, NULL);
                        if (system(rm_cmd) != 0) {
                                char msg[PATH_MAX] = {0};
                                sprintf(msg, "Failed to remove source directory: %s\n", src_path);
                                bad(1, msg);
                                // Not fatal — continue
                        }
                        free(rm_cmd);
                        free(src_path);
                        free(pkg_src_loc);
                        pkg_src_loc = NULL;
                }

                if (failed.len > 0) {
                        bad(1, "Some files could not be removed:\n");
                        for (size_t j = 0; j < failed.len; ++j) {
                                fprintf(stderr, "  - %s\n", failed.data[j]);
                                free(failed.data[j]);
                        }
                        dyn_array_free(failed);
                        goto fail_files;
                }

                char *succ_msg = forge_cstr_builder("Successfully uninstalled ", YELLOW BOLD, name, RESET, "\n\n", NULL);
                good(1, succ_msg);
                free(succ_msg);

                // Cleanup
                for (size_t j = 0; j < files.len; ++j) {
                        free(files.data[j]);
                }
                dyn_array_free(files);
                if (pkg_src_loc) free(pkg_src_loc);
                continue;

        fail_files:
                for (size_t j = 0; j < files.len; ++j) {
                        free(files.data[j]);
                }
                dyn_array_free(files);
                if (pkg_src_loc) free(pkg_src_loc);
        fail:
                ; // continue to next package
        }

        return 1;
}

static void
__list_to_be_installed(forge_context *ctx,
                       str_array      names)
{
        for (size_t i = 0; i < names.len; ++i) {
                const char *name = names.data[i];

                pkg  *pkg         = NULL;
                int   pkg_id      = get_pkg_id(ctx, name);

                if (pkg_id == -1) {
                        forge_err_wargs("unregistered package `%s`", name);
                }

                for (size_t j = 0; j < ctx->pkgs.len; ++j) {
                        if (!strcmp(ctx->pkgs.data[j]->name(), name)) {
                                pkg = ctx->pkgs.data[j];
                                break;
                        }
                }

                assert(pkg);

                if (pkg->deps) {
                        if (!pkg_is_installed(ctx, name)) {
                                str_array ar = dyn_array_empty(str_array);
                                char **deps = pkg->deps();
                                for (size_t i = 0; deps[i]; ++i) {
                                        dyn_array_append(ar, deps[i]);
                                }
                                __list_to_be_installed(ctx, ar);
                                dyn_array_free(ar);
                        }
                }

                printf(YELLOW BOLD "*" RESET YELLOW "    %s" RESET "\n", name);
        }
}

static void
list_to_be_installed(forge_context *ctx,
                     str_array      names)
{
        __list_to_be_installed(ctx, names);
        int choice = forge_chooser_yesno("\n" PINK BOLD "Continue?" RESET, NULL, 1);
        if (!choice) {
                info(0, "Canceling...\n");
                exit(0);
        }
}

static int
install_pkg(forge_context *ctx,
            str_array      names,
            int            is_dep,
            int            skip_ask)
{
        assert_sudo();

        if (!skip_ask) {
                info(0, "To be installed:\n");
                list_to_be_installed(ctx, names);
        }

        const char *failed_pkgname = NULL;

        for (size_t i = 0; i < names.len; ++i) {
                const char *name = names.data[i];

                // Printing
                char *current = forge_cstr_of_int(i+1);
                char *outof = forge_cstr_of_int(names.len);
                info_builder(0, "Installing ", BOLD BRIGHT_PINK, !is_dep ? "package " : "dependency ", RESET, YELLOW, BOLD,
                             name, RESET, " [", YELLOW, current, RESET, "/",
                             YELLOW, outof, RESET, "]\n", NULL);
                free(current);
                free(outof);

                pkg *pkg = NULL;
                char *pkg_src_loc = NULL;
                int pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        forge_err_wargs("unregistered package `%s`", name);
                }
                for (size_t j = 0; j < ctx->pkgs.len; ++j) {
                        if (!strcmp(ctx->pkgs.data[j]->name(), name)) {
                                pkg = ctx->pkgs.data[j];
                                break;
                        }
                }
                assert(pkg);

                if (pkg_is_installed(ctx, name) && is_dep) {
                        info_builder(0, "Dependency ", YELLOW BOLD, name, RESET, " is already installed\n", NULL);
                        continue; // Skip to next package
                }

                // Check current is_explicit status
                int is_explicit = !is_dep; // Default: 0 for deps, 1 for explicit
                if (pkg_id != -1) {
                        sqlite3_stmt *stmt;
                        const char *sql = "SELECT is_explicit FROM Pkgs WHERE name = ?;";
                        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                        CHECK_SQLITE(rc, ctx->db);

                        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                        /* if (sqlite3_step(stmt) == SQLITE_ROW) { */
                        /*         int existing_is_explicit = sqlite3_column_int(stmt, 0); */
                        /*         if (!is_dep) { // For explicit installations, preserve is_explicit = 1 */
                        /*                 is_explicit = existing_is_explicit || 1; // Keep 1 if already 1, else set to 1 */
                        /*         } else { // For dependencies, keep existing is_explicit */
                        /*                 is_explicit = existing_is_explicit; */
                        /*         } */
                        /* } */
                        sqlite3_finalize(stmt);
                }

                // Register package with the determined is_explicit value
                register_pkg(ctx, pkg, is_explicit);

                char *orig_fakeroot = g_fakeroot;

                // Install deps
                if (pkg->deps) {
                        info(0, "Installing Dependencies\n");
                        info_builder(0, "deps(", YELLOW BOLD, name, RESET, ")\n", NULL);
                        char **deps = pkg->deps();
                        str_array depnames = dyn_array_empty(str_array);
                        for (size_t j = 0; deps[j]; ++j) {
                                dyn_array_append(depnames, strdup(deps[j]));
                        }

                        // Install dependencies
                        if (!install_pkg(ctx, depnames, /*is_dep=*/1, /*skip_ask=*/1)) {
                                forge_err_wargs("could not install package %s, stopping...\n", names.data[i]);
                                for (size_t j = 0; j < depnames.len; ++j) free(depnames.data[j]);
                                dyn_array_free(depnames);
                                goto bad;
                        }

                        // Record dependency relationships in Deps table
                        int pkg_id = get_pkg_id(ctx, name);
                        if (pkg_id == -1) {
                                forge_err_wargs("package `%s` not registered after install", name);
                                for (size_t j = 0; j < depnames.len; ++j) free(depnames.data[j]);
                                dyn_array_free(depnames);
                                goto bad;
                        }

                        sqlite3_stmt *stmt;
                        const char *sql_insert_dep = ""
                                "INSERT OR IGNORE INTO Deps (pkg_id, dep_id) "
                                "SELECT ?1, id FROM Pkgs WHERE name = ?2;";

                        int rc = sqlite3_prepare_v2(ctx->db, sql_insert_dep, -1, &stmt, NULL);
                        CHECK_SQLITE(rc, ctx->db);

                        for (size_t j = 0; j < depnames.len; ++j) {
                                const char *dep_name = depnames.data[j];
                                int dep_id = get_pkg_id(ctx, dep_name);
                                if (dep_id == -1) {
                                        info_builder(1, "Warning: dependency `", dep_name, "` not found in DB (should be registered)\n", NULL);
                                        continue;
                                }

                                sqlite3_bind_int(stmt, 1, pkg_id);
                                sqlite3_bind_text(stmt, 2, dep_name, -1, SQLITE_STATIC);

                                rc = sqlite3_step(stmt);
                                if (rc != SQLITE_DONE) {
                                        fprintf(stderr, "Failed to record dependency %s -> %s: %s\n",
                                                name, dep_name, sqlite3_errmsg(ctx->db));
                                }
                                sqlite3_reset(stmt);
                        }

                        sqlite3_finalize(stmt);

                        // Cleanup
                        for (size_t j = 0; j < depnames.len; ++j) free(depnames.data[j]);
                        dyn_array_free(depnames);
                }

                g_fakeroot = orig_fakeroot;

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
                        goto bad;
                }

                sandbox(name);
                char *buildsrc = forge_cstr_builder(g_fakeroot, "/buildsrc", NULL);

                const char *pkgname = NULL;

                if (pkg_src_loc) {
                        pkgname = forge_io_basename(pkg_src_loc);
                } else {
                        info_builder(1, "download(", YELLOW BOLD, name, RESET, ")\n\n", NULL);
                        pkgname = pkg->download();
                        failed_pkgname = pkgname;
                        if (!pkgname) {
                                fprintf(stderr, "could not download package, aborting...\n");
                                free(pkg_src_loc);
                                goto bad;
                        }
                }

                if (!cd_silent(pkgname)) {
                        info_builder(1, "download(", YELLOW BOLD, name, RESET, ")\n\n", NULL);
                        if (!pkg->download()) {
                                fprintf(stderr, "could not download package, aborting...\n");
                                free(pkg_src_loc);
                                goto bad;
                        }
                        if (!cd(pkgname)) {
                                fprintf(stderr, "aborting...\n");
                                free(pkg_src_loc);
                                goto bad;
                        }
                }

                {
                        /* char *copy = forge_cstr_builder("cp -v -a . \"", buildsrc, "\"", NULL); */
                        info(1, "Copying build source\n");
                        char **cpfiles = ls(".");
                        for (size_t j = 0; cpfiles[j]; ++j) {
                                if (!strcmp(cpfiles[j], "..")) continue;
                                if (!strcmp(cpfiles[j], ".")) continue;
                                if (!strcmp(cpfiles[j], ".git")) continue;
                                if (!strcmp(cpfiles[j], ".gitignore")) continue;
                                char *copy = forge_cstr_builder("cp -rv \"", cpfiles[j], "\" \"", buildsrc, "\"", NULL);
                                printf("%s\n", cmdout(copy));
                                free(copy);
                        }
                }

                char src_loc[256] = {0};
                sprintf(src_loc, PKG_SOURCE_DIR "/%s", pkgname);

                CD(buildsrc, {
                                fprintf(stderr, "aborting...\n");
                                free(pkg_src_loc);
                                goto bad;
                        });

                if (pkg->build) {
                        info_builder(1, "build(", YELLOW BOLD, name, RESET, ")\n\n", NULL);
                        int buildres = pkg->build();
                        if (!buildres) {
                                fprintf(stderr, "could not build package, aborting...\n");
                                goto bad;
                        }
                } else {
                        info_builder(1, "Skipping build phase for ", YELLOW, name, RESET, "\n", NULL);
                }

                // Back to top-level of the package to reset our CWD.
                if (!cd(buildsrc)) {
                        fprintf(stderr, "aborting...\n");
                        free(pkg_src_loc);
                        goto bad;
                }

                free(buildsrc);

                info_builder(1, "install(", YELLOW BOLD, name, RESET, ")\n\n", NULL);

                setenv("DESTDIR", g_fakeroot, 1);
                if (!pkg->install()) {
                        fprintf(stderr, "failed to install package, aborting...\n");
                        free(pkg_src_loc);
                        goto bad;
                }

                // Ensure pkg_id is available
                pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        fprintf(stderr, "Failed to find package ID for %s\n", name);
                        free(pkg_src_loc);
                        goto bad;
                }

                // Walk through fakeroot and move over files.
                info(1, "Creating manifest\n");
                str_array manifest = dyn_array_empty(str_array);
                build_manifest(&manifest, g_fakeroot);

                // Keep a list of files we successfully installed for possible rollback.
                str_array installed = dyn_array_empty(str_array);

                info(0, "Installing files\n");
                for (size_t i = 0; i < manifest.len; ++i) {
                        if (i == 0) putchar('\n');

                        char *fakepath = manifest.data[i]; // /tmp/pkg-.../usr/bin/foo
                        char *realpath = fakepath + strlen(g_fakeroot);   // /usr/bin/foo

                        print_file_progress(realpath, i, manifest.len, /*add=*/1);

                        if (!copy_file_to_root(fakepath, realpath, ctx->db, pkg_id)) {
                                forge_err_wargs("failed to install %s", realpath);
                                /* remove everything we already copied */
                                for (size_t j = 0; j < installed.len; ++j) {
                                        bad(1, forge_cstr_builder("removing ", installed.data[j], "\n", NULL));
                                        unlink(installed.data[j]);
                                        free(installed.data[j]);
                                }
                                dyn_array_free(installed);
                                goto bad;

                        }

                        dyn_array_append(installed, realpath);
                }

                // Destroy manifest
                for (size_t i = 0; i < manifest.len; ++i) {
                        free(manifest.data[i]);
                } dyn_array_free(manifest); dyn_array_free(installed);

                // Update pkg_src_loc in datasrc_loc
                const char *sql_update = "UPDATE Pkgs SET pkg_src_loc = ?, installed = 1 WHERE name = ?;";
                rc = sqlite3_prepare_v2(ctx->db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, src_loc, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update pkg_src_loc error: %s\n", sqlite3_errmsg(ctx->db));
                }
                sqlite3_finalize(stmt);

                free(pkg_src_loc);

                char *succ_msg = forge_cstr_builder("Successfully installed ", YELLOW BOLD, name, RESET, "\n", NULL);
                good(1, succ_msg);
                free(succ_msg);

                destroy_fakeroot();
        }

        return 1;
 bad:
        if (failed_pkgname) {
                if (cd(PKG_SOURCE_DIR)) {
                        char *rmcmd = forge_cstr_builder("rm -r ", failed_pkgname, NULL);
                        bad(1, "Removing source due to installation failure\n");
                        cmd(rmcmd);
                        free(rmcmd);
                }
                destroy_fakeroot();
        }
        return 0;
}

static int
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

static void
clean_pkgs(forge_context *ctx)
{
        info(0, "Cleaning unneeded dependency packages\n");

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
                info(0, "No unneeded dependency packages found.\n");
        } else {
                info_builder(0, "Found ", YELLOW, forge_cstr_of_int(pkgs_to_remove.len), RESET, " unneeded dependency package(s) to remove\n", NULL);
                for (size_t i = 0; i < pkgs_to_remove.len; ++i) {
                        printf("* %s\n", pkgs_to_remove.data[i]);
                }
                uninstall_pkg(ctx, pkgs_to_remove, 1);
        }

        for (size_t i = 0; i < pkgs_to_remove.len; ++i) {
                free(pkgs_to_remove.data[i]);
        }
        dyn_array_free(pkgs_to_remove);
}

static void
show_pkg_deps(str_array names)
{
        for (size_t i = 0; i < names.len; ++i) {
                const char *pkg_name = names.data[i];

                sqlite3 *db;
                int rc = sqlite3_open_v2(DATABASE_FP, &db, SQLITE_OPEN_READONLY, NULL);
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
                info_builder(0, "Dependencies for package ", YELLOW BOLD, pkg_name, RESET "\n", NULL);
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
                        info_builder(0, "No dependencies found for package ", YELLOW BOLD, pkg_name, RESET "\n", NULL);
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
}

static void
list_pkgs(const forge_context *ctx)
{
        (void)ctx;

        sqlite3 *db;
        int rc = sqlite3_open_v2(DATABASE_FP, &db, SQLITE_OPEN_READONLY, NULL);
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

        // Find out max lengths
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
        info(0, "Available packages:\n");
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
                info(0, "No packages found in the database.\n");
        } else {
                for (size_t i = 0; i < rows.len; ++i) {
                        pkg_info *info = &rows.data[i];
                        printf("%s%-*s%s  %s%-*s%s  %s%*d%s  %s%-*s%s\n",
                               YELLOW,      (int)max_name_len,      info->name,      RESET,
                               GRAY,        (int)max_version_len,   info->version,   RESET,
                               BOLD,        (int)max_installed_len, info->installed, RESET,
                               PINK,        (int)max_desc_len, info->description,    RESET);
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

static void
first_time_reposync(void)
{
        assert_sudo();

        info(1, INVERT "IMPORTANT:" RESET " You are about to be asked if you want to install the default repository.\n");
        info(0, "It contains a module simply called " YELLOW "forge" RESET ", which allows automatic updating of the package manager itself.\n");
        info(0, "If you choose to install it, wait for the modules compile and do:\n");
        info(0, "    " YELLOW "forge install forge" RESET "\n");
        info(0, "Then you can update " YELLOW "forge" RESET " at any point by doing " YELLOW "forge --force update forge" RESET "\n");
        info(0, "(you can see this update hint again by doing " YELLOW "forge --help=update" RESET ").\n");
        info(1, YELLOW "forge" RESET " uses a C header as the configuration file. So if you need to change\n");
        info(0, "something in your config, you " BOLD "must" RESET " recompile " YELLOW "forge" RESET ".\n");
        info(0, "The main repository auto-magically does this for you, but if you choose to not\n");
        info(0, "use it then is up to you to save your header config (" YELLOW PREFIX "/include/forge/conf.h" RESET ")\n");
        info(0, "and recompile " YELLOW "forge" RESET " yourself (maybe you want this if you truly want to manage everything yourself?).\n");
        info(1, "Press " YELLOW "any key" RESET " to continue...\n");
        struct termios term;
        (void)forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term);
        (void)getchar();
        (void)forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);

        int choice = forge_chooser_yesno("Would you like to install the default forge repository", "recommended (yes)", 2);
        if (choice) {
                CD(C_MODULE_DIR_PARENT,
                   forge_err_wargs("could not `cd` into %s, aborting...", C_MODULE_DIR_PARENT));
                if (!git_clone("malloc-nbytes", "forge-modules")) {
                        forge_err("failed to clone the repository, aborting...");
                }
                g_config.flags |= FT_REBUILD;
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

static void
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

                if (!strcmp(name, "forge")) continue;

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
                info(0, "No packages are registered\n");
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
                int idx = forge_chooser("Select packages to install/uninstall",
                                        (const char **)display_entries.data, display_entries.len, cpos);
                if (idx == -1) break;
                status.data[idx] ^= installed_flag;
                status.data[idx] |= modified_flag;
                // Update display entry
                display_entries.data[idx][1] = (status.data[idx] & installed_flag) ? '*' : ' ';
                cpos = (size_t)idx;
        }

        forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);

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

        for (size_t i = 0; i < to_install.len; ++i) {
                printf(GREEN "+++ %s\n", to_install.data[i]);
        }
        for (size_t i = 0; i < to_uninstall.len; ++i) {
                printf(RED "--- %s\n", to_uninstall.data[i]);
        }
        printf(RESET);

        if (!to_install.len && !to_uninstall.len) goto clean;

        int proceed = forge_chooser_yesno("Proceed?", NULL, 1);
        if (proceed <= 0) {
                goto clean;
        }

        // Perform uninstallations
        if (to_uninstall.len > 0) {
                info(0, "Processing Uninstallations\n");
                uninstall_pkg(ctx, to_uninstall, 1);
        }

        // Perform installations
        if (to_install.len > 0) {
                info(0, "Processing Installations\n");
                install_pkg(ctx, to_install, 0, /*skip_ask=*/1);
        }

 clean:
        for (size_t i = 0; i < display_entries.len; ++i) free(display_entries.data[i]);
        for (size_t i = 0; i < pkg_names.len; ++i)       free(pkg_names.data[i]);
        for (size_t i = 0; i < names.len; ++i)           free(names.data[i]);
        for (size_t i = 0; i < versions.len; ++i)        free(versions.data[i]);
        for (size_t i = 0; i < descriptions.len; ++i)    free(descriptions.data[i]);
        for (size_t i = 0; i < to_install.len; ++i)      free(to_install.data[i]);
        for (size_t i = 0; i < to_uninstall.len; ++i)    free(to_uninstall.data[i]);

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
                info(0, "No packages are registered\n");
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

        info(0, "Updating installed status...\n");

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

                        info_builder(0, "Updated ", YELLOW BOLD, name, RESET ": ", is_installed ? "Installed" : "Uninstalled", "\n", NULL);
                }
        }

        sqlite3_finalize(stmt);

        dyn_array_free(pkgnames);
        dyn_array_free(installed);
}

void
view_pkg_info(const forge_context *ctx,
              str_array            names)
{
        for (size_t i = 0; i < names.len; ++i) {
                const char *pkgname = names.data[i];

                pkg *pkg = NULL;
                for (size_t i = 0; i < ctx->pkgs.len; ++i) {
                        if (!strcmp(ctx->pkgs.data[i]->name(), pkgname)) {
                                pkg = ctx->pkgs.data[i];
                                break;
                        }
                }

                if (!pkg) {
                        fprintf(stderr, RED "Package '%s' not found in loaded modules.\n" RESET, pkgname);
                        return;
                }

                sqlite3_stmt *stmt;
                const char *sql = "SELECT installed, is_explicit, pkg_src_loc FROM Pkgs WHERE name = ?;";
                int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);

                sqlite3_bind_text(stmt, 1, pkgname, -1, SQLITE_STATIC);

                int installed = 0, is_explicit = 0;
                char *pkg_src_loc = NULL;
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                        installed = sqlite3_column_int(stmt, 0);
                        is_explicit = sqlite3_column_int(stmt, 1);
                        const char *src_loc = (const char *)sqlite3_column_text(stmt, 2);
                        if (src_loc) {
                                pkg_src_loc = strdup(src_loc);
                        }
                } else {
                        fprintf(stderr, RED "Package '%s' not found in database.\n" RESET, pkgname);
                        sqlite3_finalize(stmt);
                        return;
                }
                sqlite3_finalize(stmt);

                info_builder(0, "Package Information for ", YELLOW BOLD, pkgname, RESET, "\n", NULL);
                printf("%-15s %s\n", "Name:", pkg->name());
                printf("%-15s %s\n", "Version:", pkg->ver());
                printf("%-15s %s\n", "Description:", pkg->desc());
                printf("%-15s %s\n", "Website:", pkg->web ? pkg->web() : "(none)");
                printf("%-15s %s\n", "Installed:", installed ? "Yes" : "No");
                printf("%-15s %s\n", "Explicit:", is_explicit ? "Yes" : "No");
                if (pkg_src_loc) {
                        printf("%-15s %s\n", "Source Location:", pkg_src_loc);
                        free(pkg_src_loc);
                } else {
                        printf("%-15s %s\n", "Source Location:", "(none)");
                }

                // List dependencies
                printf("\n" GREEN BOLD "Dependencies:\n" RESET);
                char **deps = pkg->deps ? pkg->deps() : NULL;
                if (deps && deps[0]) {
                        for (size_t i = 0; deps[i]; ++i) {
                                printf("  - %s\n", deps[i]);
                        }
                } else {
                        printf("  (none)\n");
                }
        }
}

static void
new_pkg(forge_context *ctx, str_array names)
{
        (void)ctx;

        for (size_t i = 0; i < names.len; ++i) {
                const char *n = names.data[i];
                int hitat = 0;
                for (size_t j = 0; n[j]; ++j) {
                        if (n[j] == '@') {
                                if (hitat) {
                                        forge_err_wargs("only a single '@' is allowed in a package name: %s", n);
                                } else if (!n[j+1]) {
                                        forge_err_wargs("'@' is not allowed in the last position of a package name: %s", n);
                                } else if (j == 0) {
                                        forge_err_wargs("'@' is not allowed in the first position of a package name: %s", n);
                                }
                                hitat = 1;
                        }
                }
                if (!hitat) {
                        forge_err_wargs("Missing '@'. Expected name in the format of `author@name`, got: %s", n);
                }
        }

        for (size_t i = 0; i < names.len; ++i) {
                char fp[256] = {0};
                sprintf(fp, C_MODULE_USER_DIR "/%s.c", names.data[i]);
                if (forge_io_filepath_exists(fp)) {
                        forge_err_wargs("file %s already exists", fp);
                }
                if (!forge_io_write_file(fp, FORGE_C_MODULE_TEMPLATE)) {
                        forge_err_wargs("failed to write to file %s, %s", fp, strerror(errno));
                }
                edit_file_in_editor(fp);
        }
}

static void
edit_c_module(str_array names)
{
        assert_sudo();

        for (size_t i = 0; i < names.len; ++i) {
                char *path = get_c_module_filepath_from_basic_name(names.data[i]);
                if (path) {
                        char *cmd = forge_cstr_builder(FORGE_EDITOR, " ", path, NULL);
                        if (system(cmd) == -1) {
                                fprintf(stderr, "Failed to open %s in %s: %s\n", path, FORGE_EDITOR, strerror(errno));
                        }
                        free(cmd);
                } else {
                        forge_err_wargs("package %s does not exist", names.data[i]);
                }
                free(path);
        }
}

static void
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
                        forge_err_wargs("API `%s` does not exist", name);
                }
        } else {
                char *abspath = get_c_module_filepath_from_basic_name(name);
                if (!abspath) {
                        forge_err_wargs("package `%s` does not exist", name);
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

static void
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

static void
savedep(forge_context *ctx,
        str_array      names)
{
        for (size_t i = 0; i < names.len; ++i) {
                const char *name = names.data[i];

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
}

static void
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

static int
update_pkgs(forge_context *ctx, str_array names)
{
        assert_sudo();

        str_array to_update = dyn_array_empty(str_array);
        str_array skipped   = dyn_array_empty(str_array);

        // Build the list of packages we have to examine
        if (names.len == 0) {
                sqlite3_stmt *stmt;
                const char *sql = "SELECT name FROM Pkgs WHERE installed = 1;";
                int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                CHECK_SQLITE(rc, ctx->db);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *n = (const char *)sqlite3_column_text(stmt, 0);
                        dyn_array_append(to_update, strdup(n));
                }
                sqlite3_finalize(stmt);
        } else {
                for (size_t i = 0; i < names.len; ++i)
                        dyn_array_append(to_update, strdup(names.data[i]));
        }

        if (to_update.len == 0) {
                info(0, "No packages to update.\n");
                dyn_array_free(to_update);
                return 1;
        }

        // Process each package
        int any_updated = 0;
        for (size_t i = 0; i < to_update.len; ++i) {
                const char *name = to_update.data[i];
                pkg *p = NULL;

                for (size_t j = 0; j < ctx->pkgs.len; ++j) {
                        if (!strcmp(ctx->pkgs.data[j]->name(), name)) {
                                p = ctx->pkgs.data[j];
                                break;
                        }
                }
                if (!p) {
                        forge_err_wargs("package `%s` not found in loaded modules", name);
                        continue;
                }

                if (!pkg_is_installed(ctx, name)) {
                        info_builder(0, "Package ", YELLOW BOLD, name,
                                     RESET, " is not installed – skipping\n", NULL);
                        continue;
                }

                // Skip if no update() and not forced
                if (!p->update && !(g_config.flags & FT_FORCE)) {
                        dyn_array_append(skipped, strdup(name));
                        continue;
                }

                char *src_loc = NULL;
                {
                        sqlite3_stmt *stmt;
                        const char *sql = "SELECT pkg_src_loc FROM Pkgs WHERE name = ?;";
                        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
                        CHECK_SQLITE(rc, ctx->db);
                        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                                const char *loc = (const char *)sqlite3_column_text(stmt, 0);
                                if (loc) src_loc = strdup(loc);
                        }
                        sqlite3_finalize(stmt);
                }

                if (!src_loc) {
                        forge_err_wargs("no source location recorded for %s – reinstall the package", name);
                        continue;
                }

                if (!cd(src_loc)) {
                        forge_err_wargs("source directory `%s` for %s does not exist – reinstall the package", src_loc, name);
                        free(src_loc);
                        continue;
                }

                int needs_rebuild = 0;
                if (p->update && (g_config.flags & FT_FORCE) == 0) {
                        info_builder(1, "Checking update for ", YELLOW BOLD, name, RESET, "\n", NULL);
                        needs_rebuild = p->update();
                } else {
                        needs_rebuild = 1;   /* forced */
                }

                if (!needs_rebuild) {
                        info_builder(0, "Package ", YELLOW BOLD, name,
                                     RESET, " is already up-to-date\n", NULL);
                        cd_silent("..");
                        free(src_loc);
                        continue;
                }

                any_updated = 1;

                int pull_ok = 1;
                if (p->get_changes) {
                        info_builder(1, "Pulling changes for ", YELLOW BOLD, name, RESET, "\n", NULL);
                        pull_ok = p->get_changes();
                } else {
                        pull_ok = 0;   /* force re-download */
                }

                if (!pull_ok) {
                        info_builder(1, "Re-downloading source for ", YELLOW BOLD, name, RESET, "\n", NULL);
                        char *rm = forge_cstr_builder("rm -rf ", src_loc, NULL);
                        cmd_s(rm);
                        free(rm);
                }

                cd_silent("..");
                free(src_loc);

                str_array single = dyn_array_empty(str_array);
                dyn_array_append(single, strdup(name));

                uninstall_pkg(ctx, single, 0);

                if (!install_pkg(ctx, single, /*is_dep=*/0, /*skip_ask=*/1)) {
                        //forge_err_wargs("update failed for %s", name);
                        return 0;
                } else {
                        good(0, forge_cstr_builder("Updated ", YELLOW BOLD, name, RESET, "\n", NULL));
                }

                free(single.data[0]);
                dyn_array_free(single);
        }

        if (skipped.len > 0) {
                info_builder(1, "Skipped (no update routine, use ", BOLD "--force", RESET, " to rebuild):\n", NULL);
                for (size_t i = 0; i < skipped.len; ++i)
                        printf("  * %s\n", skipped.data[i]);
        }

        // Cleanup
        for (size_t i = 0; i < to_update.len; ++i) free(to_update.data[i]);
        for (size_t i = 0; i < skipped.len; ++i)   free(skipped.data[i]);
        dyn_array_free(to_update);
        dyn_array_free(skipped);

        if (!any_updated && skipped.len == 0)
                info(0, "All checked packages are already up-to-date.\n");

        return 1;
}

static void
pkg_search(str_array names)
{
        sqlite3 *db;
        int rc = sqlite3_open_v2(DATABASE_FP, &db, SQLITE_OPEN_READONLY, NULL);
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
                for (size_t i = 0; i < names.len; ++i) {
                        if (forge_utils_regex(names.data[i], name)) {
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

static void
show_pkg_files(str_array names)
{
        if (names.len == 0) {
                forge_err("show_pkg_files(): no package names provided");
        }

        sqlite3 *db;
        int rc = sqlite3_open_v2(DATABASE_FP, &db, SQLITE_OPEN_READONLY, NULL);
        if (rc != SQLITE_OK) {
                forge_err_wargs("Cannot open database: %s\n", sqlite3_errmsg(db));
        }

        for (size_t i = 0; i < names.len; ++i) {
                const char *pkgname = names.data[i];

                // Check if package exists
                int pkg_id = get_pkg_id(&(forge_context){.db = db}, pkgname);
                if (pkg_id == -1) {
                        fprintf(stderr, RED "Package '%s' not found in database.\n" RESET, pkgname);
                        continue;
                }

                sqlite3_stmt *stmt;
                const char *sql = "SELECT path FROM Files WHERE pkg_id = ? ORDER BY path;";
                rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
                if (rc != SQLITE_OK) {
                        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
                        continue;
                }

                sqlite3_bind_int(stmt, 1, pkg_id);

                str_array files = dyn_array_empty(str_array);
                size_t file_count = 0;

                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                        const char *path = (const char *)sqlite3_column_text(stmt, 0);
                        dyn_array_append(files, strdup(path));
                        file_count++;
                }

                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(db));
                        sqlite3_finalize(stmt);
                        continue;
                }
                sqlite3_finalize(stmt);

                if (file_count == 0) {
                        info_builder(0, "Package ", YELLOW BOLD, pkgname, RESET, " has no tracked files.\n", NULL);
                        dyn_array_free(files);
                        continue;
                }

                // Header
                char *count_str = forge_cstr_of_int(file_count);
                info_builder(0, "Files installed by ", YELLOW BOLD, pkgname, RESET, " [", YELLOW, count_str, RESET, "]\n", NULL);
                free(count_str);

                // Print files
                int is_forge = !strcmp(pkgname, "forge");
                for (size_t j = 0; j < files.len; ++j) {
                        // Hack because of the self-updating nature
                        // of the PM, we cannot track conf.h.
                        if (is_forge && !strcmp(files.data[j], PREFIX "/include/forge/arg.h"))
                                printf(PREFIX "/include/forge/conf.h\n");
                        printf("%s\n", files.data[j]);
                }

                // Cleanup
                for (size_t j = 0; j < files.len; ++j) {
                        free(files.data[j]);
                }
                dyn_array_free(files);

                if (i < names.len - 1) {
                        putchar('\n');  // Separator between packages
                }
        }

        sqlite3_close(db);
}

static void
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

static void
editconf(void)
{
        assert_sudo();

        if (!forge_io_filepath_exists(FORGE_CONF_HEADER_FP)) {
                forge_err_wargs("fatal: somehow the path %s does not exist",
                                FORGE_CONF_HEADER_FP);
        }

        edit_file_in_editor(FORGE_CONF_HEADER_FP);

        info(0, "Because the configuration file is a C header,\n");
        info(0, "you must recompile forge.\n");
        info(0, "To do this, run " YELLOW "forge --force update forge" RESET ".\n");
        info(0, "This assumes that you have already ran " YELLOW "forge install forge" RESET ".\n");
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

static void
drop_repo(forge_context *ctx,
          str_array      repo_names)
{
        assert_sudo();

        for (size_t i = 0; i < repo_names.len; ++i) {
                const char *repo_name = repo_names.data[i];

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
                                uninstall_pkg(ctx, names_to_uninstall, 1);
                                for (size_t i = 0; i < names_to_uninstall.len; ++i) {
                                        free(names_to_uninstall.data[i]);
                                }
                                dyn_array_free(names_to_uninstall);
                        } else {
                                printf(YELLOW "No installed packages found in repository %s\n" RESET, repo_name);
                        }
                }

                printf(GREEN BOLD "*** Dropping packages from repository %s\n" RESET, repo_name);
                str_array to_be_dropped = dyn_array_empty(str_array);
                for (size_t i = 0; i < pkg_names.len; ++i) {
                        if (get_pkg_id(ctx, pkg_names.data[i]) != -1) {
                                dyn_array_append(to_be_dropped, pkg_names.data[i]);
                        }
                }
                drop_pkg(ctx, to_be_dropped);

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
}

static void
add_repo(str_array names)
{
        assert_sudo();

        for (size_t i = 0; i < names.len; ++i) {
                const char *name = names.data[i];

                CD(C_MODULE_DIR_PARENT, goto bad);
                char *clone = forge_cstr_builder("git clone ", name, NULL);
                CMD(clone, goto bad);
                goto ok;
        bad:
                printf("aborting...\n");
        ok:
                free(clone);
        }
}

static void
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

static void
create_repo(const char *repo_name,
            const char *repo_url)
{
        assert_sudo();

        char *new_repo_path = forge_cstr_builder(C_MODULE_DIR_PARENT, "/", repo_name, NULL);
        char *copy_cmd = forge_cstr_builder("cp ", C_MODULE_USER_DIR, "/*.c ", new_repo_path, " 2>/dev/null || true", NULL); // Handle no .c files
        char *del_cmd = forge_cstr_builder("rm -f ", C_MODULE_USER_DIR, "/*.c", NULL);
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

        info_builder(1, "Successfully created and pushed repository ", YELLOW BOLD, repo_name, RESET " to " YELLOW BOLD, repo_url, RESET "\n" RESET, NULL);

 cleanup:
        info(0, "Cleaning up...\n");
        free(new_repo_path);
        free(copy_cmd);
        free(del_cmd);
        free(add_origin_cmd);
}

static void
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

                char *repo_path = forge_cstr_builder(C_MODULE_DIR_PARENT, "/", dirs[i], NULL);
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

static str_array
fold_args(forge_arg **hd)
{
        str_array args = dyn_array_empty(str_array);
        while (*hd) {
                dyn_array_append(args, strdup((*hd)->s));
                *hd = (*hd)->n;
        }
        return args;
}

int
main(int argc, char **argv)
{
        setenv("FORGE_PREFIX", PREFIX, 1);
        setenv("FORGE_LIBDIR", LIBDIR, 1);

        g_saved_argc = argc;
        g_saved_argv = argv;

        if (argc > 0 && strcmp(argv[0], "/usr/bin/forge.new") == 0) {
                if (rename(PREFIX "/bin/forge.new", PREFIX "/bin/forge") != 0) {
                        perror("rename forge.new -> forge");
                } else {
                        info(0, "Activated new forge version\n");
                        argv[0] = "/usr/bin/forge";
                }

                exit(0);
        }

        if (init_env()) {
                first_time_reposync();
        }

        forge_context ctx = (forge_context) {
                .db = init_db(DATABASE_FP),
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

        forge_arg *arghd = forge_arg_alloc(argc, argv, 1);
        forge_arg *arg = arghd;
        while (arg) {
                if (arg->h == 1) {
                        for (size_t i = 0; arg->s[i]; ++i) {
                                char c = arg->s[i];
                                if      (c == FLAG_1HY_REBUILD[0]) g_config.flags |= FT_REBUILD;
                                else if (c == FLAG_1HY_SYNC[0])    g_config.flags |= FT_SYNC;
                                else if (c == FLAG_1HY_HELP[0]) {
                                        if (arg->eq) forge_flags_help(arg->eq);
                                        forge_flags_usage();
                                }
                                else forge_err_wargs("unknown option `%c`", c);
                        }
                } else if (arg->h == 2) {
                        if (streq(arg->s, FLAG_2HY_HELP)) {
                                if (arg->eq) {
                                        forge_flags_help(arg->eq);
                                }
                                forge_flags_usage();
                        } else if (streq(arg->s, FLAG_2HY_REBUILD)) {
                                g_config.flags |= FT_REBUILD;
                        } else if (streq(arg->s, FLAG_2HY_SYNC)) {
                                g_config.flags |= FT_SYNC;
                        } else if (streq(arg->s, FLAG_2HY_FORCE)) {
                                g_config.flags |= FT_FORCE;
                        } else {
                        }
                } else {
                        char *argcmd = arg->s;
                        arg = arg->n;
                        if (streq(argcmd, CMD_INSTALL)) {
                                str_array pkgs = fold_args(&arg);
                                int install_ok = install_pkg(&ctx, pkgs, /*is_dep=*/0, /*skip_ask=*/0);

                                if (install_ok && pkgs.len == 1 && !strcmp(pkgs.data[0], "forge")) {
                                        info(0, "forge updated - restarting with the new binary\n");

                                        // Switch to a safe cwd
                                        if (chdir("/") != 0) {
                                                perror("chdir(/)");
                                        }

                                        // Build argv for the new binary (forge.new)
                                        char **new_argv = calloc(g_saved_argc + 1, sizeof(char *));
                                        new_argv[0] = "/usr/bin/forge.new";
                                        for (int i = 1; i < g_saved_argc; ++i)
                                                new_argv[i] = g_saved_argv[i];

                                        execve(new_argv[0], new_argv, environ);

                                        // If we are still here, execve failed
                                        perror("execve(/usr/bin/forge.new)");
                                        free(new_argv);
                                }
                        } else if (streq(argcmd, CMD_LIST)) {
                                list_pkgs(&ctx);
                        } else if (streq(argcmd, CMD_LIB)) {
                                printf("-lforge\n");
                        } else if (streq(argcmd, CMD_NEW)) {
                                new_pkg(&ctx, fold_args(&arg));
                        } else if (streq(argcmd, CMD_UNINSTALL)) {
                                uninstall_pkg(&ctx, fold_args(&arg), 1);
                        } else if (streq(argcmd, CMD_INT)) {
                                interactive(&ctx);
                        } else if (streq(argcmd, CMD_INFO)) {
                                view_pkg_info(&ctx, fold_args(&arg));
                        } else if (streq(argcmd, CMD_EDIT_INSTALL)) {
                                edit_install(&ctx);
                        } else if (streq(argcmd, CMD_LIST_DEPS)) {
                                list_deps(&ctx);
                        } else if (streq(argcmd, CMD_SAVE_DEP)) {
                                savedep(&ctx, fold_args(&arg));
                        } else if (streq(argcmd, CMD_API)) {
                                str_array names = dyn_array_empty(str_array);
                                while (arg) {
                                        dyn_array_append(names, strdup(arg->s));
                                        arg = arg->n;
                                }
                                if (names.len == 0) {
                                        browse_api();
                                } else {
                                        for (size_t i = 0; i < names.len; ++i) {
                                                api_dump(names.data[i], 1);
                                        }
                                        for (size_t i = 0; i < names.len; ++i) {
                                                free(names.data[i]);
                                        }
                                        dyn_array_free(names);
                                }
                        } else if (streq(argcmd, CMD_EDIT)) {
                                edit_c_module(fold_args(&arg));
                        } else if (streq(argcmd, CMD_NEW)) {
                                new_pkg(&ctx, fold_args(&arg));
                        } else if (streq(argcmd, CMD_CLEAN)) {
                                clean_pkgs(&ctx);
                        } else if (streq(argcmd, CMD_DEPS)) {
                                show_pkg_deps(fold_args(&arg));
                        } else if (streq(argcmd, CMD_DROP)) {
                                drop_pkg(&ctx, fold_args(&arg));
                        } else if (streq(argcmd, CMD_RESTORE)) {
                                restore_pkg(fold_args(&arg));
                        } else if (streq(argcmd, CMD_DUMP)) {
                                while (arg) {
                                        api_dump(arg->s, 0);
                                        arg = arg->n;
                                }
                        } else if (streq(argcmd, CMD_UPDATE)) {
                                str_array pkgs = fold_args(&arg);
                                int install_ok = update_pkgs(&ctx, pkgs);

                                if (install_ok && pkgs.len == 1 && !strcmp(pkgs.data[0], "forge")) {
                                        info(0, "forge updated - restarting with the new binary\n");

                                        // Switch to a safe cwd
                                        if (chdir("/") != 0) {
                                                perror("chdir(/)");
                                        }

                                        // Build argv for the new binary (forge.new)
                                        char **new_argv = calloc(g_saved_argc + 1, sizeof(char *));
                                        new_argv[0] = "/usr/bin/forge.new";
                                        for (int i = 1; i < g_saved_argc; ++i)
                                                new_argv[i] = g_saved_argv[i];

                                        execve(new_argv[0], new_argv, environ);

                                        // If we are still here, execve failed
                                        perror("execve(/usr/bin/forge.new)");
                                        free(new_argv);
                                }
                        } else if (streq(argcmd, CMD_SEARCH)) {
                                pkg_search(fold_args(&arg));
                        } else if (streq(argcmd, CMD_COPYING)) {
                                forge_flags_copying();
                        } else if (streq(argcmd, CMD_FILES)) {
                                show_pkg_files(fold_args(&arg));
                        } else if (streq(argcmd, CMD_APILIST)) {
                                apilist();
                        } else if (streq(argcmd, CMD_EDITCONF)) {
                                editconf();
                        } else if (streq(argcmd, CMD_ADD_REPO)) {
                                g_config.flags |= FT_REBUILD;
                                add_repo(fold_args(&arg));
                        } else if (streq(argcmd, CMD_DROP_REPO)) {
                                g_config.flags |= FT_REBUILD;
                                drop_repo(&ctx, fold_args(&arg));
                        } else if (streq(argcmd, CMD_CREATE_REPO)) {
                                if (!arg) forge_err_wargs("flag `%s` requires a repo name", CMD_CREATE_REPO);
                                if (!arg->n) forge_err_wargs("flag `%s` requires a repo url", CMD_CREATE_REPO);
                                create_repo(arg->s, arg->n->s);
                                arg = arg->n->n;
                        } else if (streq(argcmd, CMD_REPO_COMPILE_TEMPLATE)) {
                                create_repo_compile_template();
                        } else if (streq(argcmd, CMD_LIST_REPOS)) {
                                list_repos();
                        }

                        // Solely for BASH completion
                        else if (streq(argcmd, CMD_COMMANDS)) {
                                show_commands_for_bash_completion();
                        } else if (streq(argcmd, CMD_OPTIONS)) {
                                show_options_for_bash_completion();
                        }
                        else {
                                forge_err_wargs("unknown command `%s`", argcmd);
                        }
                        break;
                }
                arg = arg->n;
        }
        forge_arg_free(arghd);

        if (g_config.flags & FT_SYNC) {
                sync();
        }

        if (g_config.flags & FT_REBUILD) {
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
                rebuild_pkgs();
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

        unsetenv("FORGE_PREFIX");
        unsetenv("FORGE_LIBDIR");

        dyn_array_free(indices);
        cleanup_forge_context(&ctx);
        return 0;
}
