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

#define INDENT printf("    ");

static void
help_help(void)
{
        printf("help(-%c, --%s):\n", FLAG_1HY_HELP, FLAG_2HY_HELP);
        INDENT printf("View the help information or provide a command/option\n");
        INDENT printf("to view more information on it, or do '*' for all.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge -h\n");
        INDENT INDENT printf("forge --help\n");
        INDENT INDENT printf("forge --help=-h\n");
        INDENT INDENT printf("forge --help=--help\n");
        INDENT INDENT printf("forge --help=install\n");
}

static void
help_rebuild(void)
{
        printf("help(-%c, --%s):\n", FLAG_1HY_REBUILD, FLAG_2HY_REBUILD);
        INDENT printf("Rebuild the C modules in each repository.\n");
        INDENT printf("Doing this operation is required when adding\n");
        INDENT printf("and dropping repositories.\n");
        INDENT printf("What this option does is compile all C modules\n");
        INDENT printf("into .so files for forge to link to during runtime.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge -r\n");
        INDENT INDENT printf("forge --rebuild\n");
        INDENT INDENT printf("forge -rs\n");
        INDENT INDENT printf("forge -r edit author@mypkg\n");
}

static void
help_sync(void)
{
        printf("help(-%c, --%s):\n", FLAG_1HY_SYNC, FLAG_2HY_SYNC);
        INDENT printf("Sync all repositories with remote.\n");
        INDENT printf("This option will go through each repository that\n");
        INDENT printf("is registered in forge and will perform `git pull` and\n");
        INDENT printf("rebuild if needed.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge -s\n");
        INDENT INDENT printf("forge --sync\n");
        INDENT INDENT printf("forge -rs\n");
}

static void
help_drop_broken_packages(void)
{
        printf("help(--%s):\n", FLAG_2HY_DROP_BROKEN_PKGS);
        INDENT printf("Have forge attempt to drop any packages\n");
        INDENT printf("that are broken (NULL functions, bad names, etc).\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("This method is not guaranteed to work in every case.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge --drop-broken-pkgs\n");
}

static void
help_list(void)
{
        printf("help(%s):\n", FLAG_2HY_LIST);
        INDENT printf("List all available packages that can be installed.\n");
        INDENT printf("This shows the package meta-data\n");
        INDENT printf("(name, version, installed, and description)\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("If you do not want to `grep` for a name,\n");
        INDENT INDENT printf("use the `search` command instead\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge list\n");
}

static void
help_search(void)
{
        printf("help(%s <names...>):\n", FLAG_2HY_SEARCH);
        INDENT printf("List packages by a name using regex.\n");
        INDENT printf("This command will search through all packages\n");
        INDENT printf("and will only display the ones where the name\n");
        INDENT printf("gets a regex match with the names provided.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge search malloc-nbytes\n");
        INDENT INDENT printf("forge search malloc-nbytes GNU\n");
}

static void
help_install(void)
{
        printf("help(%s <pkg...>):\n", FLAG_2HY_INSTALL);
        INDENT printf("This command will install packages matching the names\n");
        INDENT printf("of the packages provided.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge install malloc-nbytes@ampire\n");
        INDENT INDENT printf("forge install malloc-nbytes@earl naskst@gf\n");
}

static void
help_uninstall(void)
{
        printf("help(%s <pkg...>):\n", FLAG_2HY_UNINSTALL);
        INDENT printf("This command will uninstall packages based\n");
        INDENT printf("off of the package names provided.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge uninstall malloc-nbytes@ampire\n");
        INDENT INDENT printf("forge uninstall malloc-nbytes@earl GNU@gdb\n");
}

static void
help_update(void)
{
        printf("help(%s [pkg...]):\n", FLAG_2HY_UPDATE);
        INDENT printf("This command will update packages based off of\n");
        INDENT printf("the package names provided:\n");
        INDENT INDENT printf("1. provide names to update those specific packages\n");
        INDENT INDENT printf("2. leave empty to update all installed packages.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge update malloc-nbytes@earl\n");
        INDENT INDENT printf("forge update malloc-nbytes@ampire Github@github-cli\n");
        INDENT INDENT printf("forge update\n");
}

static void
help_add_repo(void)
{
        printf("help(%s <git-link>):\n", FLAG_2HY_ADD_REPO);
        INDENT printf("Add a new repository for forge to use.\n");
        INDENT printf("A repository is expected to contain C files\n");
        INDENT printf("with no entry point and they must fulfill everything\n");
        INDENT printf("that a module needs.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("The repository *must* be a Github link.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge add-repo https://github.com/malloc-nbytes/forge-modules.git\n");
}

static void
help_drop_repo(void)
{
        printf("help(%s <name>):\n", FLAG_2HY_DROP_REPO);
        INDENT printf("Drop a repository that forge is using.\n");
        INDENT printf("This will completely remove all packages that\n");
        INDENT printf("the repository provides.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("use the `list-repos` command to see all available repositories.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge drop-repo my-test-repo\n");
}

static void
help_list_repos(void)
{
        printf("help(%s):\n", FLAG_2HY_LIST_REPOS);
        INDENT printf("List all repositories that are currently\n");
        INDENT printf("registered in forge.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge list-repos\n");
}

static void
help_deps(void)
{
        printf("help(%s <pkg>):\n", FLAG_2HY_DEPS);
        INDENT printf("List all dependencies of a package.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge deps malloc-nbytes@particles\n");
}

static void
help_new(void)
{
        printf("help(%s <name...>):\n", FLAG_2HY_NEW);
        INDENT printf("Create new packages based off of the names provided.\n");
        INDENT printf("Using this will open a template file for you to edit\n");
        INDENT printf("as much as you want. The editor is based off of whatever\n");
        INDENT printf("the macro for FORGE_EDITOR is (see command `editconf`).\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("The names that you give it must match the\n");
        INDENT INDENT printf("format of `author@pkgname`.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("When creating a new package this way, it will not\n");
        INDENT INDENT printf("be stored in any of the repositories that are registered.\n");
        INDENT INDENT printf("Instead, it will be put into a directory for user-created packages (user_modules).\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge new me@mypkg1\n");
        INDENT INDENT printf("forge new me@mypkg2 me@mypkg3\n");
}

static void
help_edit(void)
{
        printf("help(%s <pkg...>):\n", FLAG_2HY_EDIT);
        INDENT printf("Edit the packages that are provided.\n");
        INDENT printf("The editor that it opens in is whatever the\n");
        INDENT printf("macro FORGE_EDITOR is set to (see command `editconf`).\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge edit author@pkg1\n");
        INDENT INDENT printf("forge edit author@pkg2 author@pkg3\n");
}

static void
help_dump(void)
{
        printf("help(%s <pkg...>):\n", FLAG_2HY_DUMP);
        INDENT printf("Show the contents of the C module for the packages provided.\n");
        INDENT printf("It will print it similarly to `less`.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge dump malloc-nbytes@AnimX\n");
        INDENT INDENT printf("forge dump malloc-nbytes@earl GNU@gdb\n");
}

static void
help_drop(void)
{
        printf("help(%s <pkg...>):\n", FLAG_2HY_DROP);
        INDENT printf("Drop packages based off of the packages provided.\n");
        INDENT printf("This will make it so these packages cannot be rebuilt,\n");
        INDENT printf("and the entries in the database will be deleted.\n");
        INDENT printf("Upon deletion, a backup is created.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("You can restore a recently dropped package\n");
        INDENT INDENT printf("by using the `restore` command.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge drop author@pkgname1\n");
        INDENT INDENT printf("forge drop author@pkgname2 author@pkgname3\n");
}

static void
help_files(void)
{
        printf("help(%s <pkg>):\n", FLAG_2HY_FILES);
        INDENT printf("View all files that <pkg> installed.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge files malloc-nbytes@earl\n");
}

static void
help_api(void)
{
        printf("help(%s <name...>):\n", FLAG_2HY_API);
        INDENT printf("View API files in a program similar to `less`.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("Use the command `api-list` so see available APIs.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge api cmd\n");
        INDENT INDENT printf("forge api str smap\n");
}

static void
help_restore(void)
{
        printf("help(%s <name>):\n", FLAG_2HY_RESTORE);
        INDENT printf("Restore a recently dropped package.\n");
        INDENT printf("All dropped packages have a time signature associated\n");
        INDENT printf("with it, and calling this command will restore the most\n");
        INDENT printf("recently dropped one.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge restore me@my-dropped-pkg\n");
}

static void
help_copying(void)
{
        printf("help(%s):\n", FLAG_2HY_COPYING);
        INDENT printf("View the copying information (aka legal jargon blah blah blah).\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge copying\n");
}

static void
help_depgraph(void)
{
        printf("help(%s):\n", FLAG_2HY_DEPGRAPH);
        INDENT printf("View the dependency graph of all packages.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge depgraph\n");
}

static void
help_apilist(void)
{
        printf("help(%s):\n", FLAG_2HY_APILIST);
        INDENT printf("View all APIs that forge provides for you to\n");
        INDENT printf("use in your packages (or personal projects).\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("If you are using the API in other projects,\n");
        INDENT INDENT printf("you can link with forge with `-lforge`.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge api-list\n");
}

static void
help_editconf(void)
{
        printf("help(%s):\n", FLAG_2HY_EDITCONF);
        INDENT printf("Edit the configuration file for forge.\n");
        INDENT printf("It will open in the editor that the\n");
        INDENT printf("macro FORGE_EDITOR is set to.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("Because this is a C header file, forge needs to be\n");
        INDENT INDENT printf("recompiled for it to apply (see command `updateforge`).\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge editconf\n");
}

static void
help_updateforge(void)
{
        printf("help(%s):\n", FLAG_2HY_UPDATEFORGE);
        INDENT printf("This command will download the source code,\n");
        INDENT printf("save your configuration file (see command `editconf`),\n");
        INDENT printf("and recompile forge.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("An internet connection is required.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge updateforge\n");
}

static void
help_repo_compile_template(void)
{
        printf("help(%s):\n", FLAG_2HY_REPO_COMPILE_TEMPLATE);
        INDENT printf("Generate a script for compiling forge C modules\n");
        INDENT printf("for repository testing.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("This script will remove all .so files\n");
        INDENT INDENT printf("in the current directory, so use this wisely.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge repo-compile-template\n");
}

static void
help_create_repo(void)
{
        printf("help(%s <repo_name> <git_url>):\n", FLAG_2HY_CREATE_REPO);
        INDENT printf("Create a new Github repository by repo_name at git_url.\n");
        INDENT printf("This is good for if you have created a bunch of custom packages\n");
        INDENT printf("and want to version control them.\n\n");

        INDENT printf("Note:\n");
        INDENT INDENT printf("It is *HIGHLY* recommended to have your environment being\n");
        INDENT INDENT printf("used in this step, and `sudo` does not provide this by just\n");
        INDENT INDENT printf("invoking it. Therefore, it is recommended to run `sudo -E` to\n");
        INDENT INDENT printf("inherit the environment for your Github credentials.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("sudo -E forge add-repo my-repo https://www.github.com/username/my-repo.git\n");
}

static void
help_lib(void)
{
        printf("help(%s):\n", FLAG_2HY_LIB);
        INDENT printf("Print the library flag for linking forge with other programs.\n\n");

        INDENT printf("Example:\n");
        INDENT INDENT printf("forge lib\n");
}

void
help(const char *flag)
{
        void (*hs[])(void) = {
                help_help,
                help_rebuild,
                help_sync,
                help_drop_broken_packages,
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
                help_repo_compile_template,
                help_create_repo,
                help_lib,
        };

        size_t n = strlen(flag);

        // options
        if (n == 2 && flag[0] == '-' && flag[1] == FLAG_1HY_HELP) {
                hs[0]();
        } else if (n > 3 && flag[0] == '-' && flag[1] == '-' && !strcmp(flag+2, FLAG_2HY_HELP)) {
                hs[0]();
        } else if (n == 2 && flag[0] == '-' && flag[1] == FLAG_1HY_REBUILD) {
                hs[1]();
        } else if (n > 3 && flag[0] == '-' && flag[1] == '-' && !strcmp(flag+2, FLAG_2HY_REBUILD)) {
                hs[1]();
        } else if (n == 2 && flag[0] == '-' && flag[1] == FLAG_1HY_SYNC) {
                hs[2]();
        } else if (n > 3 && flag[0] == '-' && flag[1] == '-' && !strcmp(flag+2, FLAG_2HY_SYNC)) {
                hs[2]();
        } else if (n > 3 && flag[0] == '-' && flag[1] == '-' && !strcmp(flag+2, FLAG_2HY_DROP_BROKEN_PKGS)) {
                hs[3]();
        }

        // commands
        else if (!strcmp(flag, FLAG_2HY_LIST)) {
                hs[4]();
        }
        else if (!strcmp(flag, FLAG_2HY_SEARCH)) {
                hs[5]();
        }
        else if (!strcmp(flag, FLAG_2HY_INSTALL)) {
                hs[6]();
        }
        else if (!strcmp(flag, FLAG_2HY_UNINSTALL)) {
                hs[7]();
        }
        else if (!strcmp(flag, FLAG_2HY_UPDATE)) {
                hs[8]();
        }
        else if (!strcmp(flag, FLAG_2HY_ADD_REPO)) {
                hs[9]();
        }
        else if (!strcmp(flag, FLAG_2HY_DROP_REPO)) {
                hs[10]();
        }
        else if (!strcmp(flag, FLAG_2HY_LIST_REPOS)) {
                hs[11]();
        }
        else if (!strcmp(flag, FLAG_2HY_DEPS)) {
                hs[12]();
        }
        else if (!strcmp(flag, FLAG_2HY_NEW)) {
                hs[13]();
        }
        else if (!strcmp(flag, FLAG_2HY_EDIT)) {
                hs[14]();
        }
        else if (!strcmp(flag, FLAG_2HY_DUMP)) {
                hs[15]();
        }
        else if (!strcmp(flag, FLAG_2HY_DROP)) {
                hs[16]();
        }
        else if (!strcmp(flag, FLAG_2HY_FILES)) {
                hs[17]();
        }
        else if (!strcmp(flag, FLAG_2HY_API)) {
                hs[18]();
        }
        else if (!strcmp(flag, FLAG_2HY_RESTORE)) {
                hs[19]();
        }
        else if (!strcmp(flag, FLAG_2HY_COPYING)) {
                hs[20]();
        }
        else if (!strcmp(flag, FLAG_2HY_DEPGRAPH)) {
                hs[21]();
        }
        else if (!strcmp(flag, FLAG_2HY_APILIST)) {
                hs[22]();
        }
        else if (!strcmp(flag, FLAG_2HY_EDITCONF)) {
                hs[23]();
        }
        else if (!strcmp(flag, FLAG_2HY_UPDATEFORGE)) {
                hs[24]();
        } else if (!strcmp(flag, FLAG_2HY_REPO_COMPILE_TEMPLATE)) {
                hs[25]();
        } else if (!strcmp(flag, FLAG_2HY_CREATE_REPO)) {
                hs[26]();
        } else if (!strcmp(flag, FLAG_2HY_LIB)) {
                hs[27]();
        }

        else if (!strcmp(flag, "*")) {
                for (size_t i = 0; i < sizeof(hs)/sizeof(*hs); ++i) {
                        if (i != 0) {
                                putchar('\n');
                        }
                        hs[i]();
                }
        } else {
                err_wargs("unknown flag `%s`", flag);
        }

        exit(0);
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
        printf("| flags: " COMPILER_FLAGS "\n");
        printf("| prefix: " PREFIX "\n");
        printf("| libdir: " LIBDIR "\n\n");

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
        printf(YELLOW BOLD "        --%s              "                         RESET " force the action if it can\n", FLAG_2HY_FORCE);
        printf(YELLOW BOLD "        --%s  R"                         RESET " remove all broken packages\n", FLAG_2HY_DROP_BROKEN_PKGS);
        printf("Commands:\n");
        printf(GREEN BOLD "    %s          " RESET                                "            list available packages\n", FLAG_2HY_LIST);
        printf(GREEN BOLD "    %s <pkg...> "                         RESET "          search for packages\n", FLAG_2HY_SEARCH);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "       R "     RESET  "install packages\n", FLAG_2HY_INSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "     R "       RESET  "uninstall packages\n", FLAG_2HY_UNINSTALL);
        printf(GREEN BOLD "    %s <pkg...> " RESET YELLOW BOLD "        R "    RESET  "update packages or leave empty to update all\n", FLAG_2HY_UPDATE);
        printf(GREEN BOLD "    %s <git-link> " RESET YELLOW BOLD "    R "    RESET  "add a github repository to forge\n", FLAG_2HY_ADD_REPO);
        printf(GREEN BOLD "    %s <name>     " RESET YELLOW BOLD "   R "    RESET  "drop a repository from forge\n", FLAG_2HY_DROP_REPO);
        printf(GREEN BOLD "    %s <n> <l>    " RESET YELLOW BOLD " R "    RESET  "create a new repository from your custom packages with the name `n` and git link `l`\n", FLAG_2HY_CREATE_REPO);
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
        printf(GREEN BOLD "    %s" RESET                                          "                  view API headers\n", FLAG_2HY_APILIST);
        printf(GREEN BOLD "    %s             " RESET YELLOW BOLD "   R "  RESET  "edit the forge configuration header\n", FLAG_2HY_EDITCONF);
        printf(GREEN BOLD "    %s             " RESET YELLOW BOLD "R "  RESET  "update forge (used after %s)\n", FLAG_2HY_UPDATEFORGE, FLAG_2HY_EDITCONF);
        printf(GREEN BOLD "    %s" RESET                                          "     generate a repo testing script\n", FLAG_2HY_REPO_COMPILE_TEMPLATE);
        printf(GREEN BOLD "    %s" RESET                                          "                       view forge library linking flag\n", FLAG_2HY_LIB);
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
