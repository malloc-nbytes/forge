#ifndef FORGE_TEST_H_INCLUDED
#define FORGE_TEST_H_INCLUDED

#include <stdio.h>
#include <string.h>

#define _TO_STR(x) #x
#define _PRE_EXPR __FILE__, __func__, __LINE__

#define forge_test_assert_true(expr, blk) \
        do { \
                if (!(expr)) { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(forge_test_assert_true(expr)), BOLD RED "FAILED" RESET); \
                        blk; \
                } \
        } while (0)

#define forge_test_assert_false(expr, blk) \
        do { \
                if (expr) { \
                       printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(forge_test_assert_false(expr)), BOLD RED "FAILED" RESET); \
                        blk; \
                 } \
        } while (0)

#define forge_test_assert_numeq(s1, s2, blk) \
        do { \
                if ((s1) != (s2)) { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(forge_test_assert_numeq(s1, s2)), BOLD RED "FAILED" RESET); \
                        printf("  LEFT=%d\n", s1); \
                        printf("  RIGHT=%d\n", s2); \
                        blk; \
                } \
        } while (0)

#define forge_test_assert_streq(s1, s2, blk) \
        do { \
                if (strcmp(s1, s2) != 0) { \
                        printf("%s:%s:%d -> %s ... %s\n", _PRE_EXPR, _TO_STR(forge_test_assert_streq(s1, s2)), BOLD RED "FAILED" RESET); \
                        printf("  LEFT=%s\n", s1); \
                        printf("  RIGHT=%s\n", s2); \
                        blk; \
                } \
        } while (0)

#endif // FORGE_TEST_H_INCLUDED
