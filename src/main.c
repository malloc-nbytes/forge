#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>

#include "sqlite3.h"
#include "cpm.h"
#include "graph.h"
#include "flags.h"
#include "utils.h"
#define CIO_IMPL
#include "cio.h"
#define CLAP_IMPL
#include "clap.h"

#define DB_FP "cpm.db"
#define PACKAGE_DIR "pkgs/build/"
#define CHECK_SQLITE(rc, db) if (rc != SQLITE_OK) { fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db)); sqlite3_close(db); exit(1); }
#define MAX_PKGS 100

static char *cpm_filepath = NULL;

void
cd(const char *path)
{
        if (cpm_filepath) {
                free(cpm_filepath);
        }
        cpm_filepath = strdup(path);
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
is_pkg_installed(sqlite3 *db, const char *name)
{
        sqlite3_stmt *stmt;
        const char *sql = "SELECT installed FROM Pkgs WHERE name = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        int installed = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                installed = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return installed;
}

void
_insert_pkg(sqlite3    *db,
            const char *name,
            const char *ver,
            const char *desc,
            int         installed)
{
        // Check if package exists
        sqlite3_stmt *stmt;
        const char *sql_select = "SELECT id FROM Pkgs WHERE name = ?;";
        int rc = sqlite3_prepare_v2(db, sql_select, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        int id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (id != -1) {
                // Update existing package
                const char *sql_update = "UPDATE Pkgs SET version = ?, description = ?, installed = ? WHERE name = ?;";
                rc = sqlite3_prepare_v2(db, sql_update, -1, &stmt, NULL);
                CHECK_SQLITE(rc, db);

                sqlite3_bind_text(stmt, 1, ver, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, desc, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 3, installed);
                sqlite3_bind_text(stmt, 4, name, -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Update error: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);

                // Clear existing dependencies to avoid foreign key issues
                const char *sql_delete = "DELETE FROM Deps WHERE pkg_id = ?;";
                rc = sqlite3_prepare_v2(db, sql_delete, -1, &stmt, NULL);
                CHECK_SQLITE(rc, db);

                sqlite3_bind_int(stmt, 1, id);
                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Delete dependencies error: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
        } else {
                // Insert new package
                const char *sql_insert = "INSERT INTO Pkgs (name, version, description, installed) VALUES (?, ?, ?, ?);";
                rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL);
                CHECK_SQLITE(rc, db);

                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, ver, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, desc, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 4, installed);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                        fprintf(stderr, "Insert error: %s\n", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
        }
}

void
_insert_dep(sqlite3 *db, int pkg_id, int dep_id)
{
        sqlite3_stmt *stmt;
        const char *sql = "INSERT OR IGNORE INTO Deps (pkg_id, dep_id) VALUES (?, ?);";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

        sqlite3_bind_int(stmt, 1, pkg_id);
        sqlite3_bind_int(stmt, 2, dep_id);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
                fprintf(stderr, "Dependency insert error: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_finalize(stmt);
}

void
list_deps(sqlite3 *db, const char *pkg_name)
{
        sqlite3_stmt *stmt;
        const char *sql =
                "SELECT Pkgs.name, Pkgs.version, Pkgs.description, Pkgs.installed "
                "FROM Deps "
                "JOIN Pkgs ON Deps.dep_id = Pkgs.id "
                "WHERE Deps.pkg_id = (SELECT id FROM Pkgs WHERE name = ?);";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

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
                fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(db));
        }

        if (!found) {
                printf("No dependencies found for package '%s'.\n", pkg_name);
        }

        sqlite3_finalize(stmt);
}

void
install_pkg(sqlite3 *db, pkg pkg)
{
        _insert_pkg(db, pkg.name(), pkg.ver(), pkg.desc(), pkg.installed);
}

int
get_pkg_id(sqlite3 *db, const char *name)
{
        sqlite3_stmt *stmt;
        const char *sql = "SELECT id FROM Pkgs WHERE name = ?;";
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        CHECK_SQLITE(rc, db);

        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

        int id = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
                id = sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return id;
}

void
load_package(sqlite3 *db, const char *path)
{
        void *handle = dlopen(path, RTLD_LAZY);
        if (!handle) {
                fprintf(stderr, "Error loading %s: %s\n", path, dlerror());
                return;
        }

        dlerror();
        pkg *package = dlsym(handle, "package");
        char *error = dlerror();
        if (error != NULL) {
                fprintf(stderr, "Error finding 'package' symbol in %s: %s\n", path, error);
                dlclose(handle);
                return;
        }

        // Skip if already installed
        if (is_pkg_installed(db, package->name())) {
                printf("Skipping installed package: %s v%s\n", package->name(), package->ver());
                dlclose(handle);
                return;
        }

        _insert_pkg(db, package->name(), package->ver(), package->desc(), package->installed);
        printf("Loaded package: %s v%s\n", package->name(), package->ver());

        if (package->deps) {
                int pkg_id = get_pkg_id(db, package->name());
                if (pkg_id == -1) {
                        fprintf(stderr, "Failed to get package ID for %s\n", package->name());
                        dlclose(handle);
                        return;
                }

                char **deps = package->deps();
                for (int i = 0; deps && deps[i]; i++) {
                        int dep_id = get_pkg_id(db, deps[i]);
                        if (dep_id == -1) {
                                fprintf(stderr, "Dependency %s not found for %s\n", deps[i], package->name());
                                continue;
                        }
                        _insert_dep(db, pkg_id, dep_id);
                }
        }

        // dlclose(handle); // Keep handle open for now
}

void
scan_packages(sqlite3 *db)
{
        pkg_info packages[MAX_PKGS];
        int num_packages = 0;

        DIR *dir = opendir(PACKAGE_DIR);
        if (!dir) {
                perror("Failed to open package directory");
                return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) && num_packages < MAX_PKGS) {
                if (strstr(entry->d_name, ".so")) {
                        char *path = malloc(256);
                        snprintf(path, 256, "%s%s", PACKAGE_DIR, entry->d_name);

                        void *handle = dlopen(path, RTLD_LAZY);
                        if (!handle) {
                                fprintf(stderr, "Error loading %s: %s\n", path, dlerror());
                                free(path);
                                continue;
                        }

                        dlerror();
                        pkg *package = dlsym(handle, "package");
                        char *error = dlerror();
                        if (error != NULL) {
                                fprintf(stderr, "Error finding 'package' symbol in %s: %s\n", path, error);
                                dlclose(handle);
                                free(path);
                                continue;
                        }

                        // Skip if already installed
                        if (is_pkg_installed(db, package->name())) {
                                printf("Skipping installed package: %s v%s\n", package->name(), package->ver());
                                dlclose(handle);
                                free(path);
                                continue;
                        }

                        packages[num_packages].pkg = package;
                        packages[num_packages].handle = handle;
                        packages[num_packages].path = path;
                        num_packages++;
                }
        }
        closedir(dir);

        dep_graph *graph = dep_graph_alloc(num_packages);
        for (int i = 0; i < num_packages; i++) {
                if (!packages[i].pkg->deps) { continue; }
                char **deps = packages[i].pkg->deps();
                if (deps) {
                        for (int j = 0; deps[j]; j++) {
                                int dep_index = -1;
                                for (int k = 0; k < num_packages; k++) {
                                        if (strcmp(packages[k].pkg->name(), deps[j]) == 0) {
                                                dep_index = k;
                                                break;
                                        }
                                }
                                if (dep_index != -1) {
                                        add_edge(graph, i, dep_index);
                                } else {
                                        fprintf(stderr, "Warning: Dependency %s for %s not found in packages\n",
                                                deps[j], packages[i].pkg->name());
                                }
                        }
                }
        }

        int *order = malloc(num_packages * sizeof(int));
        int order_size = 0;
        topological_sort(graph, order, &order_size);

        for (int i = 0; i < order_size; i++) {
                int idx = order[i];
                load_package(db, packages[idx].path);
        }

        dep_graph_free(graph);
        free(order);
        for (int i = 0; i < num_packages; i++) {
                dlclose(packages[i].handle);
                free(packages[i].path);
        }
}

void
install_from_source(sqlite3 *db, const char *source_file)
{
        char *base_name = strdup(source_file);
        char *dot = strstr(base_name, ".c");
        if (dot) *dot = '\0';

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "gcc -shared -fPIC %s -o %s%s.so -I.", source_file, PACKAGE_DIR, base_name);
        if (system(cmd) == 0) {
                char path[256];
                snprintf(path, sizeof(path), "%s%s.so", PACKAGE_DIR, base_name);
                load_package(db, path);
        } else {
                fprintf(stderr, "Failed to compile %s\n", source_file);
        }
        free(base_name);
}


int main(int argc, char **argv)
{
        ++argv, --argc;

        if (cio_file_exists(DB_FP) && argc == 0) {
                usage();
        }

        sqlite3 *db = init_db(DB_FP);
        scan_packages(db);

        Clap_Arg arg = {0};
        while (clap_next(&arg)) {
                if (arg.hyphc == 1 && arg.start[0] == FLAG_1HY_HELP) {
                        usage();
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_HELP)) {
                        usage();
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_LIST)) {
                        assert(0);
                } else if (arg.hyphc == 2 && !strcmp(arg.start, FLAG_2HY_DEPS)) {
                        if (!arg.eq) {
                                err_wargs("--%s requires argument after (=)", FLAG_2HY_DEPS);
                        }
                        list_deps(db, arg.eq);
                } else {
                        err_wargs("unknown flag %s", arg.start);
                }
        }

        sqlite3_close(db);
        return 0;
}
