#include "testapi.h"
#include "forge/colors.h"

int _passed = 0;
int _failed = 0;

int
main(void)
{
        // API
        /// cmd
        test_forge_api_cmd_cwd();
        test_forge_api_cmd_cd();
        test_forge_api_cmd_cd_silent();
        test_forge_api_cmd_cmd();
        test_forge_api_cmd_as();
        test_forge_api_cmd_cmdout();
        test_forge_api_cmd_git_clone();
        test_forge_api_cmd_mkdirp();
        test_forge_api_cmd_env();
        test_forge_api_cmd_get_prev_user();
        test_forge_api_cmd_change_file_owner();
        test_forge_api_cmd_make();
        test_forge_api_cmd_configure();
        test_forge_api_cmd_ls();
        test_forge_api_cmd_is_git_dir();

        /// smap
        test_forge_api_smap_create();
        test_forge_api_smap_insert_contains_get();
        test_forge_api_smap_iter();
        test_forge_api_smap_size();

        /// pkg
        test_forge_api_pkg_forge_pkg_git_update();
        test_forge_api_pkg_forge_pkg_git_pull();
        test_forge_api_pkg_forge_pkg_update_manual_check();
        test_forge_api_pkg_forge_pkg_get_changes_redownload();

        printf(YELLOW BOLD "*** Summary\n" RESET);
        printf(GREEN BOLD "  Passed: %d\n" RESET, _passed);
        printf(RED BOLD "  Failed: %d\n" RESET, _failed);

        return 0;
}
