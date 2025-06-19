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

#ifndef COLORS_H_INCLUDED
#define COLORS_H_INCLUDED

// FORGROUNDS
#define YELLOW "\033[93m"
#define GREEN "\033[32m"
#define BRIGHT_GREEN "\033[92m"
#define GRAY "\033[90m"
#define RED "\033[31m"
#define BLUE "\033[94m"
#define CYAN "\033[96m"
#define MAGENTA "\033[95m"
#define WHITE "\033[97m"
#define BLACK "\033[30m"
#define CYAN "\033[96m"
#define PINK "\033[95m"
#define BRIGHT_PINK "\033[38;5;213m"
#define PURPLE "\033[35m"
#define BRIGHT_PURPLE "\033[95m"
#define ORANGE "\033[38;5;214m"
#define BROWN "\033[38;5;94m"

// BACKGROUNDS
#define DARK_BLUE_BACKGROUND "\033[44m"

// EFFECTS
#define UNDERLINE "\033[4m"
#define BOLD "\033[1m"
#define ITALIC "\033[3m"
#define DIM "\033[2m"
#define INVERT "\033[7m"

// RESET
#define RESET "\033[0m"

void good_major(const char *msg, int newline);
void good_minor(const char *msg, int newline);
void info_major(const char *msg, int newline);
void info_minor(const char *msg, int newline);
void bad_major(const char *msg, int newline);
void bad_minor(const char *msg, int newline);

#endif // COLORS_H_INCLUDED
