/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
int test_fail_at, test_calls, test_live;
void *test_malloc(size_t size)
{
	void *memory;
	if (++test_calls == test_fail_at)
		return NULL;
	memory = malloc(size);
	if (memory)
		test_live++;
	return memory;
}
void *test_calloc(size_t count, size_t size)
{
	void *memory;
	if (++test_calls == test_fail_at)
		return NULL;
	memory = calloc(count, size);
	if (memory)
		test_live++;
	return memory;
}
void test_free(void *memory)
{
	if (memory)
		test_live--;
	free(memory);
}
