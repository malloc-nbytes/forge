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

#include "config.h"
#include "depgraph.h"
#include "flags.h"
#include "utils.h"
#include "paths.h"

#include "sqlite3.h"

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
#include <sys/mount.h>
#include <libgen.h>

#define FORGE_C_MODULE_TEMPLATE \
        "#include <forge/forge.h>\n" \
        "\n" \
        "char *deps[] = {NULL}; // Must be NULL terminated\n" \
        "\n" \
        "char *getname(void) { /*return \"author@pkg_name\";*/ }\n" \
        "char *getver(void) { return \"1.0.0\"; }\n" \
        "char *getdesc(void) { return \"My Description\"; }\n" \
        "char *getweb(void) { return \"Package Website\"; }\n" \
        "char **getdeps(void) { return deps; }\n" \
        "char *download(void) {\n" \
        "        return NULL; // should return the name of the final directory!\n" \
        "}\n" \
        "int build(void) { return 1; }\n" \
        "int install(void) { return 1; }\n" \
        "int uninstall(void) { return 1; }\n" \
        "int update(void) {\n" \
        "        return 0; // return 1 if it needs a rebuild, 0 otherwise\n" \
        "}\n" \
        "int get_changes(void) {\n" \
        "        // pull in the new changes if update() returns 1\n" \
        "        return 0;\n" \
        "}\n" \
        "\n" \
        "FORGE_GLOBAL pkg package = {\n" \
        "        .name = getname,\n" \
        "        .ver = getver,\n" \
        "        .desc = getdesc,\n" \
        "        .web = getweb,\n" \
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
                handle_array handles; // assert(handles.len == paths.len)
                str_array paths;      // assert(paths.len == handles.len)
        } dll;
        depgraph dg;
        pkg_ptr_array pkgs;
} forge_context;

struct {
        uint32_t flags;
} g_config = {
        .flags = 0x0000,
};

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

void
assert_sudo(void)
{
        if (geteuid() != 0) {
                err(BOLD YELLOW "* " RESET "This action requires " BOLD YELLOW "superuser privileges" RESET);
        }
}

void
init_env(void)
{
        // Database location
        if (mkdir(DATABASE_DIR, 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "could not create path: %s, %s\n", DATABASE_DIR, strerror(errno));
        }
        sqlite3 *db = init_db(DATABASE_FP);
        if (!db) {
                fprintf(stderr, "could not initialize database: %s\n", DATABASE_FP);
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

static void
unmount_all(const char *root)
{
        const char *subs[] = {
                "/usr/bin", "/usr/lib", "/usr/include",
                "/bin", "/lib", "/lib64", "/buildsrc",
                "/usr/x86_64-pc-linux-gnu", "/usr/libexec/", NULL
        };
        for (size_t i = 0; subs[i]; ++i) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "%s%s", root, subs[i]);
                if (umount2(path, MNT_DETACH) && errno != EINVAL && errno != ENOENT)
                        perror("umount2");
        }
}

void
rm_rf(const char *path)
{
        char cmd[PATH_MAX * 2];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
        if (system(cmd) != 0) {
                fprintf(stderr, "Failed to clean up %s\n", path);
        }
}

void
drop_privileges(void)
{
        if (setgid(getgid()) == -1) perror("setgid");
        if (setuid(getuid()) == -1) perror("setuid");
}

void
bind_mount(const char    *src,
           const char    *dst,
           unsigned long  extra_flags)
{
        printf("bind_mount(%s, %s)\n", src, dst);

        unsigned long flags = MS_BIND | MS_REC | extra_flags;
        if (mount(src, dst, NULL, flags, NULL) == -1) {
                perror("mount --bind");
                exit(1);
        }
}

void
create_skeleton(const char *root)
{
        const char *paths[] = {
                "bin",
                "etc",
                "lib",
                "usr",
                "usr/bin",
                "usr/lib",
                "usr/include",
                "var",
                "/usr/x86_64-pc-linux-gnu",
                "/usr/libexec/",
                NULL
        };

        for (size_t i = 0; paths[i]; ++i) {
                char path[PATH_MAX] = {0};
                snprintf(path, sizeof(path), "%s/%s", root, paths[i]);
                if (mkdir(path, 0755) && errno != EEXIST) {
                        perror("mkdir");
                        // Don't exit — keep trying
                }
        }
}

void
mkdir_p(const char *path)
{
        char tmp[PATH_MAX];
        const char *p;

        snprintf(tmp, sizeof(tmp), "%s", path);

        for (p = tmp; *p == '/'; p++) continue;

        for (; *p; p++) {
                if (*p == '/') {
                        char c = *p;
                        *(char *)p = '\0';
                        if (mkdir(tmp, 0755) && errno != EEXIST) {
                                perror("mkdir");
                        }
                        *(char *)p = c;
                }
        }

        if (mkdir(tmp, 0755) && errno != EEXIST) {
                perror("mkdir");
        }
}

void
safe_bind_mount(const char    *src,
                const char    *fakeroot,
                const char    *rel,
                unsigned long  extra_flags)
{
        printf("safe_bind_mount(%s, %s%s)\n", src, fakeroot, rel);

        char dst[PATH_MAX];
        snprintf(dst, sizeof(dst), "%s%s", fakeroot, rel);

        // Ensure destination directory exists
        char *dir = strdup(dst);
        mkdir_p(dirname(dir));
        free(dir);

        unsigned long flags = MS_BIND | MS_REC | extra_flags;
        if (mount(src, dst, NULL, flags, NULL) == -1) {
                perror("mount --bind");
                exit(1);
        }
}

void bind_dev(const char *fakeroot)
{
        const char *devs[] = { "null", "zero", "full", "random", "urandom", "tty" };
        char src[PATH_MAX], dst[PATH_MAX];

        snprintf(dst, sizeof(dst), "%s/dev", fakeroot);
        mkdir_p(dst);

        for (size_t i = 0; i < sizeof(devs)/sizeof(devs[0]); ++i) {
                snprintf(src, sizeof(src), "/dev/%s", devs[i]);
                snprintf(dst, sizeof(dst), "%s/dev/%s", fakeroot, devs[i]);
                if (mount(src, dst, NULL, MS_BIND, NULL) == -1) {
                        if (errno != ENOENT) perror("mount dev");
                }
        }
}

/* Bind /lib64 only if it exists on the host */
static void
bind_lib64_if_present(const char *fakeroot)
{
        const char *host = "/lib64";
        if (access(host, F_OK) != 0)
                return;

        char dst[PATH_MAX];
        snprintf(dst, sizeof(dst), "%s/lib64", fakeroot);

        /* 1. create the destination directory */
        mkdir_p(dst);                         /* your mkdir -p helper */

        /* 2. bind (read-only is fine) */
        unsigned long flags = MS_BIND | MS_REC | MS_RDONLY;
        if (mount(host, dst, NULL, flags, NULL) == -1) {
                perror("mount --bind /lib64");
                exit(1);
        }
}

void die(const char *msg) { perror(msg); exit(1); }

char *fakeroot;

void
clean_fakeroot(void)
{
        unmount_all(fakeroot);
        rm_rf(fakeroot);
}

int
main(int argc, char **argv)
{
        char *src_dir = "/home/zdh/dev/c/playground";
        char tmpl[] = "/tmp/pkgbuild-XXXXXX";
        fakeroot = mkdtemp(tmpl);
        if (!fakeroot) die("mkdtemp");

        atexit(clean_fakeroot);

        create_skeleton(fakeroot);

        safe_bind_mount("/usr/bin",     fakeroot, "/usr/bin",     MS_RDONLY);
        safe_bind_mount("/usr/lib",     fakeroot, "/usr/lib",     MS_RDONLY);
        safe_bind_mount("/usr/include", fakeroot, "/usr/include", MS_RDONLY);
        safe_bind_mount("/bin",         fakeroot, "/bin",         0);
        safe_bind_mount("/lib",         fakeroot, "/lib",         0);
        bind_lib64_if_present(fakeroot);
        safe_bind_mount("/usr/x86_64-pc-linux-gnu", fakeroot, "/usr/x86_64-pc-linux-gnu", MS_RDONLY);
        safe_bind_mount("/usr/libexec/", fakeroot, "/usr/libexec/", MS_RDONLY);

        char src_bind[PATH_MAX];
        snprintf(src_bind, sizeof(src_bind), "%s/buildsrc", fakeroot);
        mkdir_p(src_bind);
        safe_bind_mount(src_dir, fakeroot, "/buildsrc", MS_RDONLY);

        bind_dev(fakeroot);

        pid_t pid = fork();
        if (pid == -1) {
                perror("fork");
                return 1;
        }

        if (pid == 0) {
                printf("chroot: %s\n", fakeroot);

                // CHILD: inside sandbox
                if (chroot(fakeroot) == -1) {
                        perror("chroot");
                        _exit(1);
                }
                if (chdir("/buildsrc") == -1) {
                        perror("chdir to source");
                        _exit(1);
                }

                /* setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1); */
                /* setenv("SHELL", "/bin/sh", 1); */
                setenv("HOME", "/buildsrc", 1);
                setenv("PATH", "/usr/bin:/bin", 1);
                setenv("SHELL", "/bin/sh", 1);

                drop_privileges();

                /* Exec make with user-provided args */
                char *make_args[] = {NULL};
                execvp("./build.sh", make_args);
                perror("execvp");
                _exit(127);
        }

        int status;
        if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
                return 1;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Build failed with status %d\n", WEXITSTATUS(status));
                return 1;
        }

        unmount_all(fakeroot);
        printf("Build succeeded! Staged files are in: %s\n", fakeroot);
        printf("   -> Package ready for packaging (tar, etc.)\n");

        return 0;




















        ++argv, --argc;

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

        // args...

        dyn_array_free(indices);
        cleanup_forge_context(&ctx);
        return 0;
}
