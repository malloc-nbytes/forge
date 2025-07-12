#ifndef TESTAPI_INCLUDED
#define TESTAPI_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forge/colors.h"

typedef int test_result;

#define PASSED 1
#define FAILED 0

#define _TO_STR(x) #x
#define _PRE_EXPR __FILE__, __func__, __LINE__
#define MAX_PRINT_LEN 256

extern int _passed;
extern int _failed;

#define tassert_true(expr) \
        do { \
                if ((expr)) { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_true(expr)), BOLD GREEN "ok" RESET); \
                        _passed++; \
                } else { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_true(expr)), BOLD RED "FAILED" RESET); \
                        _failed--; \
                        return; \
                } \
        } while (0)

#define tassert_false(expr) \
        do { \
                if (!(expr)) { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_false(expr)), BOLD GREEN "ok" RESET); \
                        _passed++; \
                } else { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_false(expr)), BOLD RED "FAILED" RESET); \
                        _failed--; \
                        return; \
                } \
        } while (0)

#define tassert_inteq(s1, s2) \
        do { \
                if ((s1) == (s2)) { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_eq(s1, s2)), BOLD GREEN "ok" RESET); \
                        _passed++; \
                } else { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_eq(s1, s2)), BOLD RED "FAILED" RESET); \
                        printf("  LEFT=%d\n", s1); \
                        printf("  RIGHT=%d\n", s2); \
                        _failed--; \
                        return; \
                } \
        } while (0)


#define tassert_streq(s1, s2) \
        do { \
                if (!strcmp(s1, s2)) { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_streq(s1, s2)), BOLD GREEN "ok" RESET); \
                        _passed++; \
                } else { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tassert_streq(s1, s2)), BOLD RED "FAILED" RESET); \
                        printf("  LEFT=%s\n", s1); \
                        printf("  RIGHT=%s\n", s2); \
                        _failed--; \
                        return; \
                } \
        } while (0)

#define tskip() printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tskip), YELLOW "skipping" RESET); \

// Forge API
/// cmd
void test_forge_api_cmd_cwd(void);
void test_forge_api_cmd_cd(void);
void test_forge_api_cmd_cd_silent(void);
void test_forge_api_cmd_cmd(void);
void test_forge_api_cmd_as(void);
void test_forge_api_cmd_cmdout(void);
void test_forge_api_cmd_git_clone(void);
void test_forge_api_cmd_mkdirp(void);
void test_forge_api_cmd_env(void);
void test_forge_api_cmd_get_prev_user(void);
void test_forge_api_cmd_change_file_owner(void);
void test_forge_api_cmd_make(void);
void test_forge_api_cmd_configure(void);
void test_forge_api_cmd_ls(void);
void test_forge_api_cmd_is_git_dir(void);

/// smap
void test_forge_api_smap_create(void);
void test_forge_api_smap_insert_contains_get(void);
void test_forge_api_smap_iter(void);
void test_forge_api_smap_size(void);

/// pkg
void test_forge_api_pkg_forge_pkg_git_update(void);
void test_forge_api_pkg_forge_pkg_git_pull(void);
void test_forge_api_pkg_forge_pkg_update_manual_check(void);
void test_forge_api_pkg_forge_pkg_get_changes_redownload(void);

#endif // TESTAPI_INCLUDED
