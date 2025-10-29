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
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
/*#define _GNU_SOURCE*/
#define __USE_GNU
#include <sched.h>

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

char *g_fakeroot = NULL;
static int old_root_fd = -1;

void
assert_sudo(void)
{
        if (geteuid() != 0) {
                forge_err(BOLD YELLOW "* " RESET "This action requires " BOLD YELLOW "superuser privileges" RESET);
        }
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
                dlclose(ctx->dll.handles.data[i]);
                free(ctx->dll.paths.data[i]);
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
                "bin", "etc", "lib", "usr", "usr/bin", "usr/lib", "usr/include", "usr/lib64",
                "usr/local", "usr/local/include", "usr/local/bin", "usr/local/lib", "usr/local/lib64"
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
                printf("%s\n", name);

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

void destroy_fakeroot(const char *root)
{
        if (!root) return;

        info(0, "Destroying fakeroot\n");
        // Ensure no bind mounts remain
        char check[512] = {0};
        snprintf(check, sizeof(check), "mount | grep '%s/'", root);
        int has_mounts = system(check);

        if (has_mounts == 0) {
                fprintf(stderr, "Cannot destroy fakeroot: mounts still active in %s\n", root);
                return;
        }

        char command[512];
        snprintf(command, 512, "rm -rf --one-file-system %s", root);
        if (system(command) != 0) {
                fprintf(stderr, "Failed to remove fakeroot %s: %s\n", root, strerror(errno));
        } else {
                printf("* Destroyed fakeroot: %s\n", root);
        }
}

// static void
// destroy_fakeroot(void)
// {
//         if (g_fakeroot) {
//                 info(1, "Destroying fakeroot\n\n");
//                 rmrf(g_fakeroot);
//                 free(g_fakeroot);
//                 g_fakeroot = NULL;
//         }
// }

void mount_fakeroot_essentials(const char *root)
{
        char command[512] = {0};

        /* Make mounts private (isolated from host) */
        if (unshare(CLONE_NEWNS) != 0) {
                perror("unshare(CLONE_NEWNS)");
                exit(EXIT_FAILURE);
        }

        if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
                perror("mount --make-rprivate /");
                exit(EXIT_FAILURE);
        }

        /* Bind-mount essential dirs */
        const char *dirs[] = { "/bin", "/lib", "/lib64", "/etc", "/dev", "/sys", "/run" };
        for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); ++i) {
                snprintf(command, 512, "mount --bind %s %s%s", dirs[i], root, dirs[i]);
                if (system(command) != 0) {
                        fprintf(stderr, "mount failed for %s -> %s%s: %s\n",
                                dirs[i], root, dirs[i], strerror(errno));
                }
        }

        /* Mount proc */
        snprintf(command, 512, "mount -t proc proc %s/proc", root);
        if (system(command) != 0)
                fprintf(stderr, "could not mount proc\n");

        snprintf(command, 512, "chmod 1777 %s/tmp", root);
        if (system(command) != 0)
                fprintf(stderr, "could not mount tmp\n");
}

static void
unmount_fakeroot_essentials(void)
{
        if (!g_fakeroot) return;
        info(0, "Unmounting fakeroot");
        // const char *mounts[] = {
        //     "proc", "sys", "dev", "run", "etc", "lib64", "lib", "bin"
        // };
        const char *mounts[] = { "/bin", "/lib", "/lib64", "/etc", "/dev", "/sys", "/run" };
        char path[512];
        char command[600];

        /* Unmount in reverse order */
        for (size_t i = 0; i < sizeof(mounts) / sizeof(mounts[0]); ++i) {
                snprintf(path, sizeof(path), "%s/%s", g_fakeroot, mounts[i]);
                snprintf(command, sizeof(command), "umount -l %s 2>/dev/null", path);
                if (system(command) != 0)
                        fprintf(stderr, "could not unmount %s\n", path);
        }
}

static void
chroot_fakeroot(const char *root)
{
        info(0, "Entering chroot\n");

        old_root_fd = open("/", O_RDONLY);
        if (old_root_fd == -1) {
                perror("open /");
                exit(EXIT_FAILURE);
        }

        if (chroot(root) != 0) {
                perror("chroot");
                exit(EXIT_FAILURE);
        }

        if (chdir("/buildsrc") != 0) {
                perror("chdir /buildsrc");
                exit(EXIT_FAILURE);
        }
}

static void
leave_fakeroot(void)
{
        if (old_root_fd == -1)
                return;

        info(0, "Leaving chroot\n");

        if (fchdir(old_root_fd) != 0) {
                perror("fchdir");
                exit(EXIT_FAILURE);
        }

        if (chroot(".") != 0) {
                perror("restore chroot");
                exit(EXIT_FAILURE);
        }

        close(old_root_fd);
        old_root_fd = -1;
}

static int
install_pkg(forge_context *ctx, str_array names, int is_dep)
{
        (void)ctx;

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
                        if (!install_pkg(ctx, depnames, /*is_dep=*/1)) {
                                forge_err_wargs("could not install package %s, stopping...\n", names.data[i]);
                                for (size_t j = 0; j < depnames.len; ++j) {
                                        free(depnames.data[j]);
                                }
                                dyn_array_free(depnames);
                                goto bad;
                        }
                        for (size_t j = 0; j < depnames.len; ++j) {
                                free(depnames.data[j]);
                        }
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
                        info_builder(1, "(", YELLOW, name, RESET, ")->download()\n\n", NULL);
                        pkgname = pkg->download();
                        if (!pkgname) {
                                fprintf(stderr, "could not download package, aborting...\n");
                                free(pkg_src_loc);
                                goto bad;
                        }
                }

                if (!cd_silent(pkgname)) {
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
                        char *copy = forge_cstr_builder("cp -r ./* ", buildsrc, NULL);
                        info(0, "Copying build source\n");
                        cmd_s(copy);
                        free(copy);
                }

                char src_loc[256] = {0};
                sprintf(src_loc, PKG_SOURCE_DIR "/%s", pkgname);

                CD(buildsrc, {
                                fprintf(stderr, "aborting...\n");
                                free(pkg_src_loc);
                                goto bad;
                        });

                info_builder(1, "build(", YELLOW BOLD, name, RESET, ")\n\n", NULL);
                if (!pkg->build()) {
                        fprintf(stderr, "could not build package, aborting...\n");
                        goto bad;
                }

                // Back to top-level of the package to reset our CWD.
                if (!cd(buildsrc)) {
                        fprintf(stderr, "aborting...\n");
                        free(pkg_src_loc);
                        goto bad;
                }

                free(buildsrc);

                info_builder(1, "install(", YELLOW BOLD, name, RESET, ")\n\n", NULL);

                mount_fakeroot_essentials(g_fakeroot);
                chroot_fakeroot(g_fakeroot);

                if (!pkg->install()) {
                        fprintf(stderr, "failed to install package, aborting...\n");
                        free(pkg_src_loc);
                        goto bad;
                }

                leave_fakeroot();
                unmount_fakeroot_essentials();

                // Ensure pkg_id is available
                pkg_id = get_pkg_id(ctx, name);
                if (pkg_id == -1) {
                        fprintf(stderr, "Failed to find package ID for %s\n", name);
                        free(pkg_src_loc);
                        goto bad;
                }

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

                //destroy_fakeroot(g_fakeroot);
        }

        return 1;
 bad:
        return 0;
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
        int choice = forge_chooser_yesno("Would you like to install the default forge repository", NULL, 0);
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
        atexit(unmount_fakeroot_essentials);

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
                                else                               forge_err_wargs("unknown option `%c`", c);
                        }
                } else if (arg->h == 2) {
                        assert(0);
                } else {
                        char *argcmd = arg->s;
                        arg = arg->n;
                        if (streq(argcmd, CMD_INSTALL)) {
                                install_pkg(&ctx, fold_args(&arg), /*is_dep=*/0);
                        } else if (streq(argcmd, CMD_LIST)) {
                                list_pkgs(&ctx);
                        } else if (streq(argcmd, CMD_LIB)) {
                                printf("-lforge\n");
                        } else if (streq(argcmd, CMD_NEW)) {
                                new_pkg(&ctx, fold_args(&arg));
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

        dyn_array_free(indices);
        cleanup_forge_context(&ctx);
        return 0;
}
