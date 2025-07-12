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
                } \
        } while (0)

#define tskip() printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(tskip), YELLOW "skipping" RESET); \

void test_forge_api_cmd_cwd(void);
void test_forge_api_cmd_cd(void);
void test_forge_api_cmd_cd_silent(void);
void test_forge_api_cmd_cmd(void);
void test_forge_api_cmd_as(void);
void test_forge_api_cmd_cmdout(void);
void test_forge_api_cmd_git_clone(void);
void test_forge_api_cmd_mkdirp(void);
void test_forge_api_cmd_env(void);

#endif // TESTAPI_INCLUDED
