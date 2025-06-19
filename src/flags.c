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

void
usage(void)
{
        printf("Usage: forge [options...] <command> [arguments...]\n");
        printf("Commands:\n");
        printf("    %s                  list installed packages\n", FLAG_2HY_LIST);
        printf("    %s <pkg...>      install packages\n", FLAG_2HY_INSTALL);
        printf("    %s <pkg...>    uninstall packages\n", FLAG_2HY_UNINSTALL);
        printf("    %s <pkg...>       update packages or leave empty to update all\n", FLAG_2HY_UPDATE);
        printf("    %s <pkg>            list dependencies of `pkg`\n", FLAG_2HY_DEPS);
        printf("    %s <name>            create a new package module\n", FLAG_2HY_NEW);
        printf("    %s <name>           edit an existing package module\n", FLAG_2HY_EDIT);
        printf("Options:\n");
        printf("    -%c, --%s            display this message\n", FLAG_1HY_HELP, FLAG_2HY_HELP);
        printf("    -%c, --%s         rebuild package modules\n", FLAG_1HY_REBUILD, FLAG_2HY_REBUILD);
        exit(0);
}
