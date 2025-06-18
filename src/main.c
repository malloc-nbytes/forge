#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>

#include "sqlite3.h"

#include "forge.h"
#include "depgraph.h"
#include "flags.h"
#include "utils.h"
#include "dyn_array.h"
#include "ds/array.h"
#define CIO_IMPL
#include "cio.h"
#define CLAP_IMPL
#include "clap.h"

#define DB_FP "forge.db"
#define PKG_DIR "pkgs/build/"
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
                "installed INTEGER NOT NULL DEFAULT 0);";
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
                printf("*** Registered package: %s\n", name);

                if (pkg->deps) {
                        char **deps = pkg->deps();
                        for (size_t i = 0; deps[i]; ++i) {
                                printf("*** Adding dependency: %s for %s\n", deps[i], name);
                                add_dep_to_db(ctx, get_pkg_id(ctx, name), get_pkg_id(ctx, deps[i]));
                        }
                }
        }
}

void
list_deps(forge_context *ctx, const char *pkg_name)
{
        sqlite3_stmt *stmt;
        const char *sql =
                "SELECT Pkgs.name, Pkgs.version, Pkgs.description, Pkgs.installed "
                "FROM Deps "
                "JOIN Pkgs ON Deps.dep_id = Pkgs.id "
                "WHERE Deps.pkg_id = (SELECT id FROM Pkgs WHERE name = ?);";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        sqlite3_bind_text(stmt, 1, pkg_name, -1, SQLITE_STATIC);

        printf("Dependencies for package '%s':\n", pkg_name);
        printf("Name\tVersion\tInstalled\tDescription\n");
        printf("----\t-------\t---------\t-----------\n");

        int found = 0;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                const char *version = (const char *)sqlite3_column_text(stmt, 1);
                const char *description = (const char *)sqlite3_column_text(stmt, 2);
                int installed = sqlite3_column_int(stmt, 3);

                printf("%s\t%s\t%d\t\t%s\n", name, version, installed, description ? description : "(none)");
                found = 1;
        }

        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(ctx->db));
        }

        if (!found) {
                printf("No dependencies found for package '%s'.\n", pkg_name);
        }

        sqlite3_finalize(stmt);
}

void
list_registerd_pkgs(forge_context *ctx)
{
        sqlite3_stmt *stmt;
        const char *sql = "SELECT name, version, description, installed FROM Pkgs;";
        int rc = sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, ctx->db);

        printf("Available packages:\n");
        printf("Name\tVersion\tInstalled\tDescription\n");
        printf("----\t-------\t---------\t-----------\n");

        int found = 0;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const char *name = (const char *)sqlite3_column_text(stmt, 0);
                const char *version = (const char *)sqlite3_column_text(stmt, 1);
                const char *description = (const char *)sqlite3_column_text(stmt, 2);
                int installed = sqlite3_column_int(stmt, 3);

                printf("%s\t%s\t%d\t\t%s\n",
                       name,
                       version,
                       installed,
                       description ? description : "(none)");
                found = 1;
        }

        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(ctx->db));
        }

        if (!found) {
                printf("No packages found in the database.\n");
        }

        sqlite3_finalize(stmt);
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
obtain_handles_and_pkgs_from_dll(forge_context *ctx)
{
        DIR *dir = opendir(PKG_DIR);
        if (!dir) {
                perror("Failed to open package directory");
                return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir))) {
                if (strstr(entry->d_name, ".so")) {
                        char *path = (char *)malloc(256);
                        snprintf(path, 256, "%s%s", PKG_DIR, entry->d_name);

                        void *handle = dlopen(path, RTLD_LAZY);
                        if (!handle) {
                                fprintf(stderr, "Error loading dll path: %s, \n", path, dlerror());
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

int
main(int argc, char **argv)
{
        ++argv, --argc;
        clap_init(argc, argv);
        if (cio_file_exists(DB_FP) && argc == 0) {
                usage();
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

        obtain_handles_and_pkgs_from_dll(&ctx);
        construct_depgraph(&ctx);

        size_t_array indices = depgraph_gen_order(&ctx.dg);
        for (size_t i = 0; i < indices.len; ++i) {
                register_pkg(&ctx, ctx.pkgs.data[indices.data[i]]);
        }

        Clap_Arg arg = {0};
        while (clap_next(&arg)) {
                if (arg.hyphc == 1 && arg.start[0] == FLAG_1HY_HELP) {
                        assert(0);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_HELP)) {
                        assert(0);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_LIST)) {
                        list_registerd_pkgs(&ctx);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_DEPS)) {
                        if (!clap_next(&arg)) {
                                err_wargs("flag `%s` requires an argument", FLAG_2HY_DEPS);
                        }
                        list_deps(&ctx, arg.start);
                } else if (arg.hyphc == 0 && !strcmp(arg.start, FLAG_2HY_INSTALL)) {
                        assert(0);
                } else {
                        err_wargs("unknown flag `%s`", arg.start);
                }
        }

        dyn_array_free(indices);
        cleanup_forge_context(&ctx);
        return 0;
}
