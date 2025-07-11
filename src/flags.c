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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flags.h"
#include "colors.h"
#include "config.h"
#include "copying.h"
#include "utils.h"

static void
help_help(void)
{
}

static void
help_rebuild(void)
{
}

static void
help_sync(void)
{
}

static void
help_list(void)
{
}

static void
help_search(void)
{
}

static void
help_install(void)
{
}

static void
help_uninstall(void)
{
}

static void
help_update(void)
{
}

static void
help_add_repo(void)
{
}

static void
help_drop_repo(void)
{
}

static void
help_list_repos(void)
{
}

static void
help_deps(void)
{
}

static void
help_new(void)
{
}

static void
help_edit(void)
{
}

static void
help_dump(void)
{
}

static void
help_drop(void)
{
}

static void
help_files(void)
{
}

static void
help_api(void)
{
}

static void
help_restore(void)
{
}

static void
help_copying(void)
{
}

static void
help_depgraph(void)
{
}

static void
help_apilist(void)
{
}

static void
help_editconf(void)
{
}

static void
help_updateforge(void)
{
}

void
help(const char *flag)
{
        void (*hs[])(void) = {
                help_help,
                help_rebuild,
                help_sync,
                help_list,
                help_search,
                help_install,
                help_uninstall,
                help_update,
                help_add_repo,
                help_drop_repo,
                help_list_repos,
                help_deps,
                help_new,
                help_edit,
                help_dump,
                help_drop,
                help_files,
                help_api,
                help_restore,
                help_copying,
                help_depgraph,
                help_apilist,
                help_editconf,
                help_updateforge,
        };

        size_t n = strlen(flag);

        if (n == 2 && flag[0] == '-' && flag[1] == FLAG_1HY_HELP) {
        } else if (n == 1 && flag[0] == '-' && flag[1] == FLAG_1HY_REBUILD) {
        } else if (n == 1 && flag[0] == '-' && flag[1] == FLAG_1HY_SYNC) {
        }
        else if (!strcmp(flag, FLAG_2HY_HELP)) {}
        else if (!strcmp(flag, FLAG_2HY_LIST)) {}
        else if (!strcmp(flag, FLAG_2HY_DEPS)) {}
        else if (!strcmp(flag, FLAG_2HY_INSTALL)) {}
        else if (!strcmp(flag, FLAG_2HY_UNINSTALL)) {}
        else if (!strcmp(flag, FLAG_2HY_REBUILD)) {}
        else if (!strcmp(flag, FLAG_2HY_NEW)) {}
        else if (!strcmp(flag, FLAG_2HY_EDIT)) {}
        else if (!strcmp(flag, FLAG_2HY_UPDATE)) {}
        else if (!strcmp(flag, FLAG_2HY_DUMP)) {}
        else if (!strcmp(flag, FLAG_2HY_SYNC)) {}
        else if (!strcmp(flag, FLAG_2HY_DROP_BROKEN_PKGS)) {}
        else if (!strcmp(flag, FLAG_2HY_DROP)) {}
        else if (!strcmp(flag, FLAG_2HY_FILES)) {}
        else if (!strcmp(flag, FLAG_2HY_COPYING)) {}
        else if (!strcmp(flag, FLAG_2HY_DEPGRAPH)) {}
        else if (!strcmp(flag, FLAG_2HY_API)) {}
        else if (!strcmp(flag, FLAG_2HY_EDITCONF)) {}
        else if (!strcmp(flag, FLAG_2HY_UPDATEFORGE)) {}
        else if (!strcmp(flag, FLAG_2HY_RESTORE)) {}
        else if (!strcmp(flag, FLAG_2HY_APILIST)) {}
        else if (!strcmp(flag, FLAG_2HY_SEARCH)) {}
        else if (!strcmp(flag, FLAG_2HY_ADD_REPO)) {}
        else if (!strcmp(flag, FLAG_2HY_DROP_REPO)) {}
        else if (!strcmp(flag, FLAG_2HY_LIST_REPOS)) {}
        else if (!strcmp(flag, "*")) {
                for (size_t i = 0; i < sizeof(hs)/sizeof(*hs); ++i) {
                        hs[i]();
                }
        } else {
                err_wargs("unknown flag `%s`", flag);
        }
}

void
usage(void)
{
        printf(UNDERLINE BOLD "Forge your System\n\n" RESET);

        printf("Forge version 1.0, Copyright (C) 2025 malloc-nbytes.\n");
        printf("Forge comes with ABSOLUTELY NO WARRANTY.\n");
        printf("This is free software, and you are welcome to redistribute it\n");
        printf("under certain conditions; see command `copying`.\n\n");

        printf("Compilation Information:\n");
        printf("| cc: " COMPILER_NAME "\n");
        printf("| path: " COMPILER_PATH "\n");
        printf("| ver.: " COMPILER_VERSION "\n");
        printf("| flags: " COMPILER_FLAGS "\n\n");

        printf("Github repository: https://www.github.com/malloc-nbytes/forge.git/\n\n");
        printf("Send bug reports to:\n");
        printf("  - https://github.com/malloc-nbytes/forge/issues\n");
        printf("  - or " PACKAGE_BUGREPORT "\n\n");

        printf("The documentation can be found at https://malloc-nbytes.github.io/forge-web/index.html\n\n");

        printf("Usage: forge " YELLOW BOLD "[options...] " RESET GREEN BOLD "<command>" RESET " [arguments...]\n");
        printf("Options:\n");
        printf(YELLOW BOLD "    -%c, --%s[=<flag>|*]" RESET "     display this message or view help on a command or option\n", FLAG_1HY_HELP, FLAG_2HY_HELP);
        printf(YELLOW BOLD "    -%c, --%s           R"                         RESET  " rebuild package modules\n", FLAG_1HY_REBUILD, FLAG_2HY_REBUILD);
        printf(YELLOW BOLD "    -%c, --%s              R"                         RESET " sync the C modules repository\n", FLAG_1HY_SYNC, FLAG_2HY_SYNC);
        printf(YELLOW BOLD "        --%s  R"                         RESET " remove all broken packages\n", FLAG_2HY_DROP_BROKEN_PKGS);
        printf("Commands:\n");
        printf(GREEN BOLD "    %s          " RESET                                "            list available packages\n", FLAG_2HY_LIST);
        printf(GREEN BOLD "    %s <pkg...> "                         RESET "          search for packages\n", FLAG_2HY_SEARCH);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "       R "     RESET  "install packages\n", FLAG_2HY_INSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "     R "       RESET  "uninstall packages\n", FLAG_2HY_UNINSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "        R "    RESET  "update packages or leave empty to update all\n", FLAG_2HY_UPDATE);
        printf(GREEN BOLD "    %s <git-link> " RESET YELLOW BOLD "    R "    RESET  "add a github repository to forge\n", FLAG_2HY_ADD_REPO);
        printf(GREEN BOLD "    %s <name>     " RESET YELLOW BOLD "   R "    RESET  "drop a repository from forge\n", FLAG_2HY_DROP_REPO);
        printf(GREEN BOLD "    %s" RESET                                "                list all repositories that forge is using\n", FLAG_2HY_LIST_REPOS);
        printf(GREEN BOLD "    %s <pkg>    " RESET                                "            list dependencies of `pkg`\n", FLAG_2HY_DEPS);
        printf(GREEN BOLD "    %s <name...>   " RESET YELLOW BOLD "        R " RESET  "create a new package module\n", FLAG_2HY_NEW);
        printf(GREEN BOLD "    %s <name...>   " RESET YELLOW BOLD "       R "  RESET  "edit an existing package module\n", FLAG_2HY_EDIT);
        printf(GREEN BOLD "    %s <name...>" RESET                                "            dump a package module\n", FLAG_2HY_DUMP);
        printf(GREEN BOLD "    %s <name...>" RESET                                "            drop a package\n", FLAG_2HY_DROP);
        printf(GREEN BOLD "    %s <name>" RESET                                   "              list all installed files for package <name>\n", FLAG_2HY_FILES);
        printf(GREEN BOLD "    %s <name...>" RESET                                "             show the header files for the Forge API\n", FLAG_2HY_API);
        printf(GREEN BOLD "    %s <name>" RESET                                   "            restore a recently dropped package\n", FLAG_2HY_RESTORE);
        printf(GREEN BOLD "    %s" RESET                                          "                   view copying information\n", FLAG_2HY_COPYING);
        printf(GREEN BOLD "    %s" RESET                                          "                  view the depgraph of all C modules\n", FLAG_2HY_DEPGRAPH);
        printf(GREEN BOLD "    %s" RESET                                          "                   view API headers\n", FLAG_2HY_APILIST);
        printf(GREEN BOLD "    %s             " RESET YELLOW BOLD "   R "  RESET  "edit the forge configuration header\n", FLAG_2HY_EDITCONF);
        printf(GREEN BOLD "    %s             " RESET YELLOW BOLD "R "  RESET  "update forge (used after %s)\n", FLAG_2HY_UPDATEFORGE, FLAG_2HY_EDITCONF);
        printf("Note: " YELLOW BOLD "R" RESET " requires root permissions\n");
        exit(0);
}

void
copying(void)
{
        printf(COPYING1);
        printf(COPYING2);
        printf(COPYING3);
        printf(COPYING4);
        printf(COPYING5);
        printf(COPYING6);
        exit(0);
}
