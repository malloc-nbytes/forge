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

void
usage(void)
{
        printf("Forge version 1.0, Copyright (C) 2025 malloc-nbytes.\n");
        printf("Forge comes with ABSOLUTELY NO WARRANTY.\n");
        printf("This is free software, and you are welcome to redistribute it\n");
        printf("under certain conditions; see --copying\n\n");

        printf("Usage: forge " YELLOW BOLD "[options...] " RESET GREEN BOLD "<command>" RESET " [arguments...]\n");
        printf("Options:\n");
        printf(YELLOW BOLD "    -%c, --%s" RESET "          display this message\n", FLAG_1HY_HELP, FLAG_2HY_HELP);
        printf(YELLOW BOLD "    -%c, --%s" RESET "       rebuild package modules\n", FLAG_1HY_REBUILD, FLAG_2HY_REBUILD);
        printf("Commands:\n");
        printf(GREEN BOLD "    %s          " RESET "      list installed packages\n", FLAG_2HY_LIST);
        printf(GREEN BOLD "    %s <pkg...> " RESET "   install packages\n", FLAG_2HY_INSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET " uninstall packages\n", FLAG_2HY_UNINSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET "    update packages or leave empty to update all\n", FLAG_2HY_UPDATE);
        printf(GREEN BOLD "    %s <pkg>    " RESET "      list dependencies of `pkg`\n", FLAG_2HY_DEPS);
        printf(GREEN BOLD "    %s <name>   " RESET "       create a new package module\n", FLAG_2HY_NEW);
        printf(GREEN BOLD "    %s <name>   " RESET "      edit an existing package module\n", FLAG_2HY_EDIT);
        printf(GREEN BOLD "    %s <name...>" RESET "      dump a package module\n", FLAG_2HY_DUMP);
        exit(0);
}
