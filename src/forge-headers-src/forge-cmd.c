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
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>

#include "forge/cmd.h"
#include "forge/conf.h"

char *
cwd(void)
{
        size_t size = 1024;
        char *buf = NULL;

        while (1) {
                char *tmp = (char *)realloc(buf, size);
                if (!tmp) {
                        free(buf);
                        return NULL;
                }
                buf = tmp;

                if (getcwd(buf, size) != NULL) {
                        return buf;
                }

                // If buf was too small, double the size and try again
                if (errno == ERANGE) {
                        size *= 2;
                        continue;
                }

                // Other errors
                free(buf);
                return NULL;
        }

        return NULL; // unreachable
}

int
cd(const char *fp)
{
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
        printf("\033[93m" "\033[2m");

        printf("%s\n", cmd);

        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
                printf("\033[0m");
                fprintf(stderr, "Failed to execute command '%s': %s\n", cmd, strerror(errno));
                return 0;
        }

        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                printf("\033[0m");
                printf("%s", buffer);
        }

        // Get exit status
        int status = pclose(fp);
        if (status == -1) {
                printf("\033[0m");
                fprintf(stderr, "Failed to close pipe for command '%s': %s\n", cmd, strerror(errno));
                return 0;
        }

        printf("\033[0m");
        return WEXITSTATUS(status) == 0;
}

int
cmd_as(const char *cmd,
       const char *username) {
        // Construct the command with sudo -u to run as the specified user
        char *sudo_cmd = malloc(strlen(cmd) + strlen(username) + 20);
        if (!sudo_cmd) {
                fprintf(stderr, "Memory allocation failed: %s\n", strerror(errno));
                return 0;
        }
        snprintf(sudo_cmd, strlen(cmd) + strlen(username) + 20, "sudo -u %s %s", username, cmd);

        printf("%s\n", sudo_cmd);

        // Open pipe to execute the command
        FILE *fp = popen(sudo_cmd, "r");
        if (fp == NULL) {
                fprintf(stderr, "Failed to execute command '%s': %s\n", sudo_cmd, strerror(errno));
                free(sudo_cmd);
                return 0;
        }

        // Read and print command output
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                printf("%s", buffer);
        }

        // Get exit status
        int status = pclose(fp);
        if (status == -1) {
                fprintf(stderr, "Failed to close pipe for command '%s': %s\n", sudo_cmd, strerror(errno));
                free(sudo_cmd);
                return 0;
        }

        free(sudo_cmd);
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

char *
get_prev_user(void)
{
        return getenv("SUDO_USER");
}

int
change_file_owner(const char *path,
                  const char *user)
{
        struct passwd *pwd = getpwnam(user);
        struct group *grp = getgrnam(user);
        if (chown(path, pwd->pw_uid, grp->gr_gid) == -1) {
                perror("chown");
                return 0;
        }
        return 1;
}

int
make(const char *type)
{
        char buf[256] = {0};
        if (type) {
                sprintf(buf, "make %s -j%s", type, FORGE_PREFERRED_MAKEFILE_JFLAGS);
        } else {
                sprintf(buf, "make -j%s", FORGE_PREFERRED_MAKEFILE_JFLAGS);
        }
        return cmd(buf);
}

int
configure(const char *fp,
          const char *flags)
{
        char buf[256] = {0};
        if (flags) {
                sprintf(buf, "%sconfigure --prefix=%s --libdir=%s %s", fp,
                        FORGE_PREFERRED_INSTALL_PREFIX,
                        FORGE_PREFERRED_LIB_PREFIX, flags);
        } else {
                sprintf(buf, "%sconfigure --prefix=%s --libdir=%s", fp,
                        FORGE_PREFERRED_INSTALL_PREFIX,
                        FORGE_PREFERRED_LIB_PREFIX);
        }
        return cmd(buf);
}

char **
ls(const char *dir)
{
        DIR *dp = opendir(dir);
        if (!dp) {
                return NULL;
        }

        // Count files
        struct dirent *entry;
        int count = 0;
        while ((entry = readdir(dp))) {
                count++;
        }
        rewinddir(dp);

        char **files = malloc((count + 1) * sizeof(char *));
        if (!files) {
                closedir(dp);
                return NULL;
        }

        int i = 0;
        while ((entry = readdir(dp))) {
                files[i] = strdup(entry->d_name);
                if (!files[i]) {
                        for (int j = 0; j < i; j++) {
                                free(files[j]);
                        }
                        free(files);
                        closedir(dp);
                        return NULL;
                }
                i++;
        }
        files[count] = NULL;

        closedir(dp);
        return files;
}

int
is_git_dir(const char *path)
{
        // Store the current working directory
        char current_dir[1024];
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
                return 0;
        }

        if (!cd(path)) return 0;

        // Check for .git directory
        DIR* dir = opendir(".git");
        int is_git = (dir != NULL);
        if (dir) {
                closedir(dir);
        }

        // Restore original working directory
        if (!cd(current_dir)) return 0;

        return is_git;
}

int
is_sudo(void)
{
        return !geteuid();
}
