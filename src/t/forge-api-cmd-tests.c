#include <stdlib.h>
#include <unistd.h>

#include "testapi.h"
#include "forge/cmd.h"

void
test_forge_api_cmd_cwd(void)
{
        char *dir = cwd();
        tassert_true(dir != NULL);

        char buf[512] = {0};
        getcwd(buf, sizeof(buf));
        tassert_streq(dir, buf);
}

void
test_forge_api_cmd_cd(void)
{
        char *home = env("HOME");
        tassert_true(home != NULL);
        tassert_true(cd(home));

        char buf[512] = {0};
        getcwd(buf, sizeof(buf));
        char *dir = cwd();
        tassert_true(dir != NULL);
        tassert_streq(dir, buf);
}

void
test_forge_api_cmd_cd_silent(void)
{
        char *home = env("HOME");
        tassert_true(home != NULL);
        tassert_true(cd_silent(home));

        char buf[512] = {0};
        getcwd(buf, sizeof(buf));
        char *dir = cwd();
        tassert_true(dir != NULL);
        tassert_streq(dir, buf);
}

void
test_forge_api_cmd_cmd(void)
{
        tassert_true(cmd("echo 'Testing cmd()'"));
}

void
test_forge_api_cmd_as(void)
{
        tskip();
}

void
test_forge_api_cmd_cmdout(void)
{
        char *out = cmdout("echo 'Testing cmdout()'");
        tassert_true(out != NULL);
        tassert_streq(out, "Testing cmdout()");
}

void
test_forge_api_cmd_git_clone(void)
{
        tskip();
}

void
test_forge_api_cmd_mkdirp(void)
{
        tskip();
}

void
test_forge_api_cmd_env(void)
{
        char *home = env("HOME");
        tassert_true(home != NULL);

        tassert_streq(home, getenv("HOME"));
}

void
test_forge_api_cmd_get_prev_user(void)
{
        tskip();
}

void
test_forge_api_cmd_change_file_owner(void)
{
        tskip();
}

void
test_forge_api_cmd_make(void)
{
        tskip();
}

void
test_forge_api_cmd_configure(void)
{
        tskip();
}

void
test_forge_api_cmd_ls(void)
{
        char **files = ls(".");
        tassert_true(files != NULL);
        tassert_true(files[0] != NULL);

        for (int i = 0; files[i]; ++i) {
                free(files[i]);
        }
        free(files);
}

void
test_forge_api_cmd_is_git_dir(void)
{
        tassert_false(is_git_dir("."));
}
