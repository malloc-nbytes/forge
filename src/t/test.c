#include "testapi.h"

int _passed = 0;
int _failed = 0;

int
main(void)
{
        // API
        /// CMD
        test_forge_api_cmd_cwd();
        test_forge_api_cmd_cd();
        test_forge_api_cmd_cd_silent();
        test_forge_api_cmd_cmd();
        test_forge_api_cmd_as();
        test_forge_api_cmd_cmdout();
        test_forge_api_cmd_git_clone();
        test_forge_api_cmd_mkdirp();
        test_forge_api_cmd_env();

        return 0;
}
