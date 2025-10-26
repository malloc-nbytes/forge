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

#define FORGE_C_MODULE_TEMPLATE \
        "#include <forge/forge.h>\n" \
        "\n" \
        "char *deps[] = {NULL}; // Must be NULL terminated\n" \
        "\n" \
        "char *getname(void) { /*return \"author@pkg_name\";*/ }\n"     \
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

int
try_first_time_startup(int argc)
{
        assert(0);
        //int exists = cio_file_exists(DB_FP);
        int exists = 1;

        if (exists && argc == 0) {
                forge_flags_usage();
        } else if (!exists) {
                printf("Superuser access is required the first time forge is ran.\n");
                assert_sudo();

                int choice = forge_chooser_yesno("Would you like to install the offical forge repository?", NULL, 1);

                init_env();

                if (choice == -1) {
                        printf("Something went wrong... :(\n");
                } else if (choice == 1) {
                        assert(0);
                        //add_repo("https://github.com/malloc-nbytes/forge-modules.git");
                }

                return choice;
        }

        return -1;
}

int
main(int argc, char **argv)
{
        ++argv, --argc;

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

        // args...

        dyn_array_free(indices);
        cleanup_forge_context(&ctx);
        return 0;
}
