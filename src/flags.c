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

#include "flags.h"
#include "colors.h"
#include "config.h"
#include "copying.h"

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

        printf("Usage: forge " YELLOW BOLD "[options...] " RESET GREEN BOLD "<command>" RESET " [arguments...]\n");
        printf("Options:\n");
        printf(YELLOW BOLD "    -%c, --%s" RESET "                display this message\n", FLAG_1HY_HELP, FLAG_2HY_HELP);
        printf(YELLOW BOLD "    -%c, --%s           R"                         RESET  " rebuild package modules\n", FLAG_1HY_REBUILD, FLAG_2HY_REBUILD);
        printf(YELLOW BOLD "    -%c, --%s              R"                         RESET " sync the C modules repository\n", FLAG_1HY_SYNC, FLAG_2HY_SYNC);
        printf(YELLOW BOLD "        --%s  R"                         RESET " remove all broken packages\n", FLAG_2HY_DROP_BROKEN_PKGS);
        printf("Commands:\n");
        printf(GREEN BOLD "    %s          " RESET                                "            list available packages\n", FLAG_2HY_LIST);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "       R "     RESET  "install packages\n", FLAG_2HY_INSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "     R "       RESET  "uninstall packages\n", FLAG_2HY_UNINSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "        R "    RESET  "update packages or leave empty to update all\n", FLAG_2HY_UPDATE);
        printf(GREEN BOLD "    %s <pkg>    " RESET                                "            list dependencies of `pkg`\n", FLAG_2HY_DEPS);
        printf(GREEN BOLD "    %s <name...>   " RESET YELLOW BOLD "        R " RESET  "create a new package module\n", FLAG_2HY_NEW);
        printf(GREEN BOLD "    %s <name...>   " RESET YELLOW BOLD "       R "  RESET  "edit an existing package module\n", FLAG_2HY_EDIT);
        printf(GREEN BOLD "    %s <name...>" RESET                                "            dump a package module\n", FLAG_2HY_DUMP);
        printf(GREEN BOLD "    %s <name...>" RESET                                "            drop a package\n", FLAG_2HY_DROP);
        printf(GREEN BOLD "    %s <name>" RESET                                   "              list all installed files for package <name>\n", FLAG_2HY_FILES);
        printf(GREEN BOLD "    %s" RESET                                          "                   view copying information\n", FLAG_2HY_COPYING);
        printf(GREEN BOLD "    %s" RESET                                          "                  view the depgraph of all C modules\n", FLAG_2HY_DEPGRAPH);
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
