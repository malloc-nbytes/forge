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

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "forge/cmd.h"

int
cd(const char *fp)
{
        printf("cd(%s)\n", fp);
        if (chdir(fp) != 0) {
                fprintf(stderr, "[forge] err: failed to change directory to `%s`: %s\n",
                        fp, strerror(errno));
                return 0;
        }
        return 1;
}

int
cd_silent(const char *fp)
{
        if (chdir(fp) != 0) {
                return 0;
        }
        return 1;
}

int
cmd(const char *cmd)
{
        printf("%s\n", cmd);

        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
                fprintf(stderr, "Failed to execute command '%s': %s\n", cmd, strerror(errno));
                return 0;
        }

        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                printf("%s", buffer);
        }

        // Get exit status
        int status = pclose(fp);
        if (status == -1) {
                fprintf(stderr, "Failed to close pipe for command '%s': %s\n", cmd, strerror(errno));
                return 0;
        }

        return WEXITSTATUS(status) == 0;
}

char *
cmdout(const char *cmd)
{
        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
                fprintf(stderr, "Failed to execute command '%s': %s\n", cmd, strerror(errno));
                return NULL;
        }

        // Dynamic buffer to store output
        char *buffer = NULL;
        size_t buffer_size = 0;
        size_t total_size = 0;
        size_t chunk_size = 1024;

        // Read output in chunks
        while (1) {
                buffer = realloc(buffer, buffer_size + chunk_size);
                if (buffer == NULL) {
                        fprintf(stderr, "Memory allocation failed: %s\n", strerror(errno));
                        pclose(fp);
                        return NULL;
                }
                buffer_size += chunk_size;

                size_t read_size = fread(buffer + total_size, 1, chunk_size - 1, fp);
                total_size += read_size;
                buffer[total_size] = '\0';

                if (read_size < chunk_size - 1) {
                        break; // EOF or error
                }
        }

        int status = pclose(fp);
        if (status == -1 || WEXITSTATUS(status) != 0) {
                free(buffer);
                return NULL;
        }

        // Check if output is only whitespace
        int is_whitespace = 1;
        for (size_t i = 0; i < total_size; i++) {
                if (!isspace((unsigned char)buffer[i])) {
                        is_whitespace = 0;
                        break;
                }
        }

        if (is_whitespace) {
                free(buffer);
                return NULL;
        }

        // Trim trailing whitespace
        while (total_size > 0 && isspace((unsigned char)buffer[total_size - 1])) {
                buffer[--total_size] = '\0';
        }

        return buffer;
}

char *
git_clone(char *author,
          char *name)
{
        char buf[256] = {0};
        sprintf(buf, "git clone https://www.github.com/%s/%s.git/", author, name);
        if (!cmd(buf)) {
                return NULL;
        }
        return name;
}

char *
mkdirp(char *fp)
{
        char buf[256] = {0};
        sprintf(buf, "mkdir -p %s", fp);
        if (!cmd(buf)) {
                return NULL;
        }
        return fp;
}

char *
env(const char *var)
{
        return getenv(var);
}
