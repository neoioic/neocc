/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#undef malloc
#undef calloc
#undef free
#undef realloc
static long failure;
static size_t calls, live;
static int initialized;

static void report(void)
{
	fprintf(stderr, "test allocations: %zu live: %zu\n", calls, live);
}

static int fail_now(void)
{
	if (!initialized) {
		const char *value = getenv("OPENOICC_TEST_FAIL_ALLOC");
		failure = value ? strtol(value, NULL, 10) : 0;
		initialized = 1;
		atexit(report);
	}
	return ++calls == (size_t)failure;
}

void *test_malloc(size_t size)
{
	void *memory = fail_now() ? NULL : malloc(size);
	if (memory)
		live++;
	return memory;
}

void *test_calloc(size_t count, size_t size)
{
	void *memory = fail_now() ? NULL : calloc(count, size);
	if (memory)
		live++;
	return memory;
}

void *test_realloc(void *old, size_t size)
{
	void *memory;
	if (fail_now())
		return NULL;
	memory = realloc(old, size);
	if (memory && !old)
		live++;
	return memory;
}

void test_free(void *memory)
{
	if (memory)
		live--;
	free(memory);
}
