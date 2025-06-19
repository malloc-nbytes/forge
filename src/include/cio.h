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

#ifndef CIO_H
#define CIO_H

#ifdef CIO_IMPL

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>

int cio_file_exists(const char *fp) {
        FILE *f = fopen(fp, "r");
        if (f) {
                fclose(f);
                return 1;
        }
        return 0;
}

void cio_create_file(const char *fp, int force_overwrite) {
        if (!force_overwrite && cio_file_exists(fp)) { return; }
        FILE *f = fopen(fp, "w");
        fclose(f);
}

char *cio_file_to_cstr(const char *fp, size_t *ret_len) {
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
                if (!strcmp(line, "\n")) { continue; }
                if (line[read - 1] == '\n') {
                        line[read - 1] = '\0';
                }
                for (size_t i = 0; line[i]; ++i) {
                        if (buf.len >= buf.cap) {
                                buf.cap = buf.cap == 0 ? 2 : buf.cap*2;
                                buf.data = realloc(buf.data, buf.cap);
                        }
                        buf.data[buf.len++] = line[i];
                }
        }

        free(line);
        fclose(f);

        *ret_len = buf.len;
        return buf.data;
}

char **cio_file_to_lines(const char *fp, size_t *ret_len) {
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
                if (!strcmp(line, "\n")) { continue; }
                if (line[read - 1] == '\n') {
                        line[read - 1] = '\0';
                }
                if (buf.len >= buf.cap) {
                        buf.cap = buf.cap == 0 ? 2 : buf.cap*2;
                        buf.data = realloc(buf.data, buf.cap * sizeof(char *));
                }
                buf.data[buf.len++] = strdup(line);
        }

        free(line);
        fclose(f);

        *ret_len = buf.len;
        return buf.data;
}

char *resolve_absolute_path(const char *fp) {
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

int cio_write_file(const char *fp, const char *content) {
        FILE *f = fopen(fp, "w");
        if (!f) { return 0; }
        (void)fprintf(f, "%s", content);
        fclose(f);
        return 1;
}

#endif // CIO_IMPL

#endif // CIO_H
