#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <dirent.h>
#include <string.h>

#include "sqlite3.h"
#include "cpm.h"
#define CIO_IMPL
#include "cio.h"

#define DB_FP "cpm.db"
#define PACKAGE_DIR "pkgs/build/"
#define CHECK_SQLITE(rc, db) if (rc != SQLITE_OK) { fprintf(stderr, "SQLite error: %s\n", sqlite3_errmsg(db)); sqlite3_close(db); exit(1); }
#define MAX_PKGS 100

typedef struct {
        pkg *pkg;
        void *handle; // dlopen handle
        char *path;   // xxx.so path
} pkg_info;

typedef struct adj_node {
        int pkg_i; // Index in pkg array
        struct adj_node *next;
} adj_node;

typedef struct {
        adj_node *hd;
} adj_list;

typedef struct {
        adj_list *ar;
        int pkgs_n;
} dep_graph;

adj_node *
adj_node_alloc(int pkg_i)
{
        adj_node *node = malloc(sizeof(adj_node));
        if (!node) {
                fprintf(stderr, "Memory allocation failed for adjacency node\n");
                exit(1);
        }
        node->pkg_i = pkg_i;
        node->next = NULL;
        return node;
}

dep_graph *
dep_graph_alloc(int pkgs_n)
{
        dep_graph *dg = malloc(sizeof(dep_graph));
        if (!dg) {
                fprintf(stderr, "Memory allocation failed for graph\n");
                exit(1);
        }
        dg->pkgs_n = pkgs_n;
        dg->ar = malloc(pkgs_n * sizeof(adj_list));
        if (!dg->ar) {
                fprintf(stderr, "Memory allocation failed for graph array\n");
                free(dg);
                exit(1);
        }
        for (int i = 0; i < pkgs_n; ++i) {
                dg->ar[i].hd = NULL;
        }
        return dg;
}

void
dep_graph_free(dep_graph *dg)
{
        if (!dg) return;
        for (int i = 0; i < dg->pkgs_n; ++i) {
                adj_node *node = dg->ar[i].hd;
                while (node) {
                        adj_node *tmp = node;
                        node = node->next;
                        free(tmp);
                }
        }
        free(dg->ar);
        free(dg);
}

void
add_edge(dep_graph *dg, int src, int dest)
{
        adj_node *node = adj_node_alloc(dest);
        node->next = dg->ar[src].hd;
        dg->ar[src].hd = node;
}

void
topological_sort_util(dep_graph *dg, int v, int visited[], int stack[], int *stack_index, int in_stack[])
{
        visited[v] = 1;
        in_stack[v] = 1;

        adj_node *node = dg->ar[v].hd;
        while (node) {
                int u = node->pkg_i;
                if (!visited[u]) {
                        topological_sort_util(dg, u, visited, stack, stack_index, in_stack);
                } else if (in_stack[u]) {
                        fprintf(stderr, "Cyclic dependency detected involving package index %d\n", u);
                        exit(1);
                }
                node = node->next;
        }

        in_stack[v] = 0;
        stack[(*stack_index)++] = v;
}

void
topological_sort(dep_graph *dg, int result[], int *result_size)
{
        int *visited = calloc(dg->pkgs_n, sizeof(int));
        int *in_stack = calloc(dg->pkgs_n, sizeof(int));
        int *stack = malloc(dg->pkgs_n * sizeof(int));
        int stack_index = 0;

        if (!visited || !in_stack || !stack) {
                fprintf(stderr, "Memory allocation failed for topological sort\n");
                exit(1);
        }

        for (int i = 0; i < dg->pkgs_n; ++i) {
                if (!visited[i]) {
                        topological_sort_util(dg, i, visited, stack, &stack_index, in_stack);
                }
        }

        *result_size = stack_index;
        for (int i = 0; i < stack_index; ++i) {
                result[i] = stack[stack_index - 1 - i];
        }

        free(visited);
        free(in_stack);
        free(stack);
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
_insert_pkg(sqlite3 *db, const char *name, const char *ver, const char *desc, int installed)
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

int main(void)
{
        sqlite3 *db = init_db(DB_FP);
        scan_packages(db);
        list_deps(db, "gf");
        sqlite3_close(db);
        printf("*** Done.\n");
        return 0;
}
