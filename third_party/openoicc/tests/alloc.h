/* SPDX-License-Identifier: MIT */
#ifndef OPENOICC_TEST_ALLOC_H
#define OPENOICC_TEST_ALLOC_H
#include <stdlib.h>
void *test_malloc(size_t size);
void *test_calloc(size_t count, size_t size);
void test_free(void *memory);
extern int test_fail_at, test_calls, test_live;
#define malloc test_malloc
#define calloc test_calloc
#define free test_free
#endif
