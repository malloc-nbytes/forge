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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <sys/stat.h>

#include "forge/io.h"
#include "forge/cmd.h"

int
forge_io_filepath_exists(const char *fp)
{
        FILE *f = fopen(fp, "r");
        if (f) {
                fclose(f);
                return 1;
        }
        return 0;
}

void
forge_io_create_file(const char *fp,
                     int         force_overwrite)
{
        if (!force_overwrite && forge_io_filepath_exists(fp)) { return; }
        FILE *f = fopen(fp, "w");
        fclose(f);
}

char *
forge_io_read_file_to_cstr(const char *fp)
{
        FILE *f = fopen(fp, "r");
        char *line = NULL;
        size_t len = 0;
        ssize_t read;

        struct {
                char *data;
                size_t len;
                size_t cap;
        } buf = {
                .data = NULL,
                .cap = 0,
                .len = 0,
        };

        while ((read = getline(&line, &len, f)) != -1) {
                //if (!strcmp(line, "\n")) { continue; }
                for (size_t i = 0; line[i]; ++i) {
                        if (buf.len >= buf.cap) {
                                buf.cap = buf.cap == 0 ? 2 : buf.cap*2;
                                buf.data = realloc(buf.data, buf.cap);
                        }
                        buf.data[buf.len++] = line[i];
                }
        }

        if (buf.len >= buf.cap) {
                buf.cap = buf.cap == 0 ? 2 : buf.cap*2;
                buf.data = realloc(buf.data, buf.cap);
        }
        buf.data[buf.len++] = '\0';

        free(line);
        fclose(f);

        return buf.data;
}

// Result will be guaranteed to be NULL-terminated.
char **
forge_io_read_file_to_lines(const char *fp)
{
        FILE *f = fopen(fp, "r");
        char *line = NULL;
        size_t len = 0;
        ssize_t read;

        struct {
                char **data;
                size_t len;
                size_t cap;
        } buf = {
                .data = NULL,
                .cap = 0,
                .len = 0,
        };

        while ((read = getline(&line, &len, f)) != -1) {
                if (line[read - 1] == '\n') {
                        line[read - 1] = '\0';
                }
                if (buf.len >= buf.cap) {
                        buf.cap = buf.cap == 0 ? 2 : buf.cap*2;
                        buf.data = realloc(buf.data, buf.cap * sizeof(char *));
                }
                buf.data[buf.len++] = strdup(line);
        }

        if (buf.len >= buf.cap) {
                buf.cap = buf.cap == 0 ? 2 : buf.cap*2;
                buf.data = realloc(buf.data, buf.cap * sizeof(char *));
        }
        buf.data[buf.len++] = NULL;

        free(line);
        fclose(f);

        return buf.data;
}

char *
forge_io_resolve_absolute_path(const char *fp)
{
        char *result = NULL;

        if (!fp || !*fp) { return NULL; }

        if (fp[0] == '~') {
                struct passwd *pw = getpwuid(getuid());
                if (!pw) {
                        return NULL;
                }

                size_t home_len = strlen(pw->pw_dir);
                size_t fp_len = strlen(fp);
                result = malloc(home_len + fp_len);
                if (!result) { return NULL; }

                strcpy(result, pw->pw_dir);
                strcat(result, fp + 1); // Skip the ~
        } else {
                result = strdup(fp);
                if (!result) {
                        return NULL;
                }
        }

        char *absolute = realpath(result, NULL);
        free(result);

        if (!absolute) {
                return NULL;
        }

        return absolute;
}

int
forge_io_write_file(const char *fp,
                    const char *content)
{
        FILE *f = fopen(fp, "w");
        if (!f) { return 0; }
        (void)fprintf(f, "%s", content);
        fclose(f);
        return 1;
}

int
forge_io_is_dir(const char *path)
{
        struct stat path_stat;
        if (stat(path, &path_stat) != 0) {
                return 0;
        }
        return S_ISDIR(path_stat.st_mode);
}

int
forge_io_dir_contains_file(const char *dir,
                           const char *filename)
{
        if (!forge_io_filepath_exists(dir)) return 0;
        if (!forge_io_is_dir(dir))          return 0;

        char **files = ls(dir);
        if (!files) return 0;

        for (size_t i = 0; files[i]; ++i) {
                if (!strcmp(files[i], filename)) {
                        for (size_t j = i; files[j]; ++j) { free(files[j]); }
                        free(files);
                        return 1;
                }
                free(files[i]);
        }
        free(files);

        return 0;
}
