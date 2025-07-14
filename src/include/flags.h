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

#define FLAG_1HY_HELP 'h'
#define FLAG_1HY_REBUILD 'r'
#define FLAG_1HY_SYNC 's'

#define FLAG_2HY_HELP "help"
#define FLAG_2HY_REBUILD "rebuild"
#define FLAG_2HY_SYNC "sync"
#define FLAG_2HY_LIST "list"
#define FLAG_2HY_DEPS "deps"
#define FLAG_2HY_INSTALL "install"
#define FLAG_2HY_UNINSTALL "uninstall"
#define FLAG_2HY_NEW "new"
#define FLAG_2HY_EDIT "edit"
#define FLAG_2HY_UPDATE "update"
#define FLAG_2HY_DUMP "dump"
#define FLAG_2HY_DROP_BROKEN_PKGS "drop-broken-pkgs"
#define FLAG_2HY_DROP "drop"
#define FLAG_2HY_FILES "files"
#define FLAG_2HY_COPYING "copying"
#define FLAG_2HY_DEPGRAPH "depgraph"
#define FLAG_2HY_API "api"
#define FLAG_2HY_EDITCONF "editconf"
#define FLAG_2HY_UPDATEFORGE "updateforge"
#define FLAG_2HY_RESTORE "restore"
#define FLAG_2HY_APILIST "api-list"
#define FLAG_2HY_SEARCH "search"
#define FLAG_2HY_ADD_REPO "add-repo"
#define FLAG_2HY_DROP_REPO "drop-repo"
#define FLAG_2HY_LIST_REPOS "list-repos"
#define FLAG_2HY_REPO_COMPILE_TEMPLATE "repo-compile-template"
#define FLAG_2HY_CREATE_REPO "create-repo"
#define FLAG_2HY_FORCE "force"
#define FLAG_2HY_LIB "lib"
#define FLAG_2HY_CLEAN "clean"
#define FLAG_2HY_SAVE_DEP "save-dep"
#define FLAG_2HY_LIST_DEPS "list-deps"

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
