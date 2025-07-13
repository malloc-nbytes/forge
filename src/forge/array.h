#ifndef FORGE_ARRAY_H_INCLUDED
#define FORGE_ARRAY_H_INCLUDED

#include <stdlib.h>

// MIT License

// Copyright (c) 2025 malloc-nbytes

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/* This file is used for making stack alloc'd dynamic
 * arrays where we dont need to use the Array<T> DS. */

/**
 * Creates a new dynamic array type globally.
 * Note: Use dyn_array_init() to initialize
 *       any instances of it.
 * Example:
 *   dyn_array_type(int, Int_Array);
 *
 *   void f(Int_Array *arr);
 *
 *   int main(void) {
 *       Int_Array arr;
 *       return 0;
 *   }
 */
#define DYN_ARRAY_TYPE(ty, name) \
    typedef struct {             \
        ty *data;                \
        size_t len, cap;         \
    } name

#define dyn_array_empty(arr_ty)                 \
        (arr_ty) {                              \
                .data = NULL,                   \
                .len = 0,                       \
                .cap = 0,                       \
        }

/**
 * Initializes a global array type. This is only
 * used if you use DYN_ARRAY_TYPE().
 * Example:
 *   dyn_array_type(int, Int_Array);
 *
 *   int main(void) {
 *       Int_Array arr;
 *       dyn_array_init_type(arr); // <- here
 *       return 0;
 *   }
 */
#define dyn_array_init_type(da)                 \
    do {                                        \
        (da).data = malloc(sizeof(*(da).data)); \
        (da).cap = 1;                           \
        (da).len = 0;                           \
    } while (0)

/**
 * Creates a new dynamic array on the stack.
 * Example:
 *   dyn_array(int, int_vector);
 */
#define dyn_array(ty, name)                                        \
    struct {                                                       \
        ty *data;                                                  \
        size_t len, cap;                                           \
    } (name) = { .data = (typeof(ty) *)malloc(sizeof(ty)), .len = 0, .cap = 1 };

/**
 * Append to a dynamic array.
 * Example:
 *   dyn_array(int, int_vector);
 *   for (int i = 0; i < 10; ++i)
 *     dyn_array_append(int_vector, i);
 */
#define dyn_array_append(da, value)                                     \
    do {                                                                \
        if ((da).len >= (da).cap) {                                     \
            (da).cap = (da).cap ? (da).cap * 2 : 2;                     \
            (da).data = (typeof(*((da).data)) *)                        \
                realloc((da).data,                                      \
                        (da).cap * sizeof(*((da).data)));               \
        }                                                               \
        (da).data[(da).len++] = (value);                                \
    } while (0)

/**
 * Free a dynamic array.
 * Example:
 *   dyn_array(int, int_vector);
 *   dyn_array_free(int_vector);
*/
#define dyn_array_free(da)       \
    do {                         \
        if ((da).data != NULL) { \
                free((da).data); \
        }                        \
        (da).len = (da).cap = 0; \
    } while (0)

/**
 * Get an element safely at an index.
 * Will panic if the element is out of bounds.
 * Example:
 *   dyn_array(int, int_vector);
 *   dyn_array_append(int_vector, i);
 *   printf("%d\n", dyn_array_at_s(int_vector));
 */
#define dyn_array_at_s(da, i)                                      \
    ((i) < (da).len ? (da).data[i] : (fprintf(stderr,              \
    "[dyn_array error]: index %zu is out of bounds (len = %zu)\n", \
    (size_t)(i), (size_t)(da).len), exit(1), (da).data[0]))

/**
 * Get an element at an index.
 * Example:
 *   dyn_array(int, int_vector);
 *   dyn_array_append(int_vector, i);
 *   printf("%d\n", dyn_array_at(int_vector));
 */
#define dyn_array_at(da, i) ((da).data[i])

/**
 * Clear a dynamic array.
 * Example:
 *   dyn_array(int, int_vector);
 *   dyn_array_append(int_vector, i);
 *   dyn_array_clear(int_vector);
 */
#define dyn_array_clear(da) (da).len = 0;

/**
 * Remove an element at index `idx`.
 * Example:
 *   dyn_array(int, int_vector);
 *   ...
 *   dyn_array_rm_at(int_vector, 0);
 *   dyn_array_rm_at(int_vector, 5);
 *   ...
 */
#define dyn_array_rm_at(da, idx) \
    do {                                                     \
        for (size_t __i_ = (idx); __i_ < (da).len-1; ++__i_) \
            (da).data[__i_] = (da).data[__i_+1];             \
        (da).len--;                                          \
    } while (0)

#define dyn_array_explode(da) (da).data, (da).len, (da).cap

#define dyn_array_explode_mem(da) &(da).data, &(da).len, &(da).cap

// Common types for arrays.

DYN_ARRAY_TYPE(int,      int_array);
DYN_ARRAY_TYPE(char,     char_array);
DYN_ARRAY_TYPE(char *,   str_array);
DYN_ARRAY_TYPE(size_t,   size_t_array);
DYN_ARRAY_TYPE(float,    float_array);
DYN_ARRAY_TYPE(double,   double_array);
DYN_ARRAY_TYPE(long,     long_array);
DYN_ARRAY_TYPE(unsigned, unsigned_array);
DYN_ARRAY_TYPE(void *,   void_ptr_array);

#endif // FORGE_ARRAY_H_INCLUDED
