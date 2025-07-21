#include <stdlib.h>

#include "forge/utils.h"

int
forge_utils_rand_in_range(int st, int end)
{
        if (st > end) {
                int temp = st;
                st = end;
                end = temp;
        }
        return st + (rand() % (end - st + 1));
}
