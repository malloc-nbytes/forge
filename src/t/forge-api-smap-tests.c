#include "testapi.h"
#include "forge/smap.h"

static char *
generate_sequence(void)
{
        static char str[11] = "aaaaaaaaaa";
        static int pos = 0;
        char *result = malloc(11);

        if (!result) {
                return NULL;
        }

        strcpy(result, str);

        str[pos]++;

        if (str[pos] > 'z') {
                str[pos] = 'a';
                pos++;
                if (pos >= 10) {
                        memset(str, 'a', 10);
                        pos = 0;
                }
        }

        return result;
}

void
test_forge_api_smap_create(void)
{
        forge_smap m = forge_smap_create();
        tassert_true(m.len == 0);
        tassert_true(m.cap == FORGE_SMAP_DEFAULT_TBL_CAPACITY);
        tassert_true(m.sz == 0);
        forge_smap_destroy(&m);
}

void
test_forge_api_smap_insert_contains_get(void)
{
        forge_smap m = forge_smap_create();

        int nums[26];
        int n = sizeof(nums)/sizeof(*nums);

        for (int i = 0; i < n; ++i) {
                nums[i] = i;
        }

        char **strs = (char **)malloc(sizeof(char *) * n);
        for (int i = 0; i < n; ++i) {
                strs[i] = generate_sequence();
        }

        for (int i = 0; i < n; ++i) {
                forge_smap_insert(&m, strs[i], (void *)&nums[i]);
                tassert_true(forge_smap_contains(&m, strs[i]));
                int *value = (int *)forge_smap_get(&m, strs[i]);
                tassert_true(value != NULL);
                tassert_inteq(*value, nums[i]);
        }

        tassert_false(forge_smap_contains(&m, "does not exist"));

        forge_smap_destroy(&m);

        for (int i = 0; i < n; ++i) {
                free(strs[i]);
        }
        free(strs);
}

void
test_forge_api_smap_iter(void)
{
        forge_smap m = forge_smap_create();

        int a = 1, b = 2, c = 3;

        forge_smap_insert(&m, "a", (void *)&a);
        forge_smap_insert(&m, "b", (void *)&b);
        forge_smap_insert(&m, "c", (void *)&c);

        char **keys = forge_smap_iter(&m);
        tassert_true(keys != NULL);

        for (int i = 0; keys[i]; ++i) {
                tassert_true(keys[i] != NULL);
                tassert_true(!strcmp(keys[i], "a") || !strcmp(keys[i], "b") || !strcmp(keys[i], "c"));
        }

        free(keys);

        forge_smap_destroy(&m);
}

void
test_forge_api_smap_size(void)
{
        forge_smap m = forge_smap_create();

        int a = 1, b = 2, c = 3;

        forge_smap_insert(&m, "a", (void *)&a);
        forge_smap_insert(&m, "b", (void *)&b);
        forge_smap_insert(&m, "c", (void *)&c);

        tassert_inteq((int)forge_smap_size(&m), 3);

        forge_smap_destroy(&m);
}
