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
#include <unistd.h>

#include "colors.h"

void
good_major(const char *msg, int newline)
{
        printf(GREEN BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
good_minor(const char *msg, int newline)
{
        printf(GREEN "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}

void
info_major(const char *msg, int newline)
{
        printf(YELLOW BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
info_minor(const char *msg, int newline)
{
        printf(YELLOW "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}

void
bad_major(const char *msg, int newline)
{
        printf(RED BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
bad_minor(const char *msg, int newline)
{
        printf(RED "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}
