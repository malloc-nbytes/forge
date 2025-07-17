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

#ifndef FLAGS_H_INCLUDED
#define FLAGS_H_INCLUDED

#define FLAG_1HY_HELP "h"
#define FLAG_1HY_REBUILD "r"
#define FLAG_1HY_SYNC "s"

#define FLAG_2HY_HELP "help"
#define FLAG_2HY_REBUILD "rebuild"
#define FLAG_2HY_SYNC "sync"
#define FLAG_2HY_DROP_BROKEN_PKGS "drop-broken-pkgs"

#define CLI_OPTIONS {                           \
                "-" FLAG_1HY_HELP,              \
                "-" FLAG_1HY_REBUILD,           \
                "-" FLAG_1HY_SYNC,              \
                "--" FLAG_2HY_HELP,             \
                "--" FLAG_2HY_REBUILD,          \
                "--" FLAG_2HY_SYNC,             \
                "--" FLAG_2HY_DROP_BROKEN_PKGS, \
        }

#define CMD_LIST "list"
#define CMD_DEPS "deps"
#define CMD_INSTALL "install"
#define CMD_UNINSTALL "uninstall"
#define CMD_NEW "new"
#define CMD_EDIT "edit"
#define CMD_UPDATE "update"
#define CMD_DUMP "dump"
#define CMD_DROP "drop"
#define CMD_FILES "files"
#define CMD_COPYING "copying"
#define CMD_DEPGRAPH "depgraph"
#define CMD_API "api"
#define CMD_EDITCONF "editconf"
#define CMD_UPDATEFORGE "updateforge"
#define CMD_RESTORE "restore"
#define CMD_APILIST "api-list"
#define CMD_SEARCH "search"
#define CMD_ADD_REPO "add-repo"
#define CMD_DROP_REPO "drop-repo"
#define CMD_LIST_REPOS "list-repos"
#define CMD_REPO_COMPILE_TEMPLATE "repo-compile-template"
#define CMD_CREATE_REPO "create-repo"
#define CMD_FORCE "force"
#define CMD_LIB "lib"
#define CMD_CLEAN "clean"
#define CMD_SAVE_DEP "save-dep"
#define CMD_LIST_DEPS "list-deps"

#define CLI_CMDS {                              \
                CMD_LIST,                       \
                CMD_DEPS,                       \
                CMD_INSTALL,                    \
                CMD_UNINSTALL,                  \
                CMD_NEW,                        \
                CMD_EDIT,                       \
                CMD_UPDATE,                     \
                CMD_DUMP,                       \
                CMD_DROP,                       \
                CMD_FILES,                      \
                CMD_COPYING,                    \
                CMD_DEPGRAPH,                   \
                CMD_API,                        \
                CMD_EDITCONF,                   \
                CMD_UPDATEFORGE,                \
                CMD_RESTORE,                    \
                CMD_APILIST,                    \
                CMD_SEARCH,                     \
                CMD_ADD_REPO,                   \
                CMD_DROP_REPO,                  \
                CMD_LIST_REPOS,                 \
                CMD_REPO_COMPILE_TEMPLATE,      \
                CMD_CREATE_REPO,                \
                CMD_FORCE,                      \
                CMD_LIB,                        \
                CMD_CLEAN,                      \
                CMD_SAVE_DEP,                   \
                CMD_LIST_DEPS,                  \
        }

#define CMD_COMMANDS "COMMANDS" // not included in CLI_COMMANDS (hidden)
#define CMD_OPTIONS "OPTIONS"   // not included in CLI_COMMANDS (hidden)

typedef enum {
        FT_REBUILD = 1 << 0,
        FT_SYNC = 1 << 1,
        FT_DROP_BROKEN_PKGS = 1 << 2,
        FT_FORCE = 1 << 3,
} flag_type;

void usage(void);
void copying(void);
void help(const char *flag);

#endif // FLAGS_H_INCLUDED
