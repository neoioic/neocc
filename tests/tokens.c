#include "neocc/tokens.h"
#include "host/host.h"
#include "objects/tokens.h"
#include "check.h"
#include <string.h>
#include <stdio.h>

#ifdef NEOCC_TEST_TOKEN_FAULTS
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
static size_t live;
static int calls, fail_at;

void *__wrap_malloc(size_t size)
{
	void *pointer = ++calls == fail_at ? NULL : __real_malloc(size);
	if (pointer)
		live++;
	return pointer;
}

void *__wrap_calloc(size_t count, size_t size)
{
	void *pointer = ++calls == fail_at ? NULL : __real_calloc(count, size);
	if (pointer)
		live++;
	return pointer;
}

void *__wrap_realloc(void *old, size_t size)
{
	void *pointer = ++calls == fail_at ? NULL : __real_realloc(old, size);
	if (pointer && !old)
		live++;
	return pointer;
}

void __wrap_free(void *pointer)
{
	if (pointer) {
		CHECK(live);
		live--;
	}
	__real_free(pointer);
}
#endif

static void clean_access(struct object *object)
{
	unsigned int i, j;

	for (i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;
		for (j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static int intern(struct object *object, const char *name,
	struct neocc_token_result *output)
{
	struct neocc_token_intern request = { name, strlen(name) };
	unsigned char input[sizeof(request) + 1];
	unsigned char buffer[sizeof(*output) + 1];
	unsigned int quantity = 123;
	int result;

	memcpy(input + 1, &request, sizeof(request));
	result = vo_interact3(object, NEOCC_AC3_TOKENS_INTERN,
		input + 1, sizeof(request), buffer + 1, sizeof(*output),
		&quantity, &neocc_host_caller);
	if (RP(result)) {
		CHECK(quantity == sizeof(*output));
		memcpy(output, buffer + 1, sizeof(*output));
	} else {
		CHECK(!quantity);
	}
	clean_access(object);
	return result;
}

static int resize(struct object *object, void *pointer, size_t size, void **out)
{
	struct neocc_token_resize request = { pointer, size };
	unsigned int quantity;
	int result = vo_interact3(object, NEOCC_AC3_TOKENS_RESIZE,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)out, sizeof(*out), &quantity,
		&neocc_host_caller);

	CHECK(RP(result) ? quantity == sizeof(*out) : !quantity);
	clean_access(object);
	return result;
}

int main(void)
{
	struct object *a, *b;
	struct neocc_token_result result;
	struct neocc_token *first;
	void *pointer, *grown, *unused;
	char name[64];
	size_t i;
#ifdef NEOCC_TEST_TOKEN_FAULTS
	/* MinGW startup may allocate before main through wrapped imports. */
	size_t initial_live = live;
#endif

	CHECK(RP(create_object(NEOCC_MSN_TOKENS, 0, 0, 1, &a)));
	CHECK(RP(create_object(NEOCC_MSN_TOKENS, 0, 0, 1, &b)));
	CHECK(RN(intern(a, "closed", &result)));
	CHECK(RP(vo_open(a, &neocc_host_caller)));
	CHECK(RP(vo_open(b, &neocc_host_caller)));
#ifdef NEOCC_TEST_TOKEN_FAULTS
	/* Fail table growth, then the symbol arena, without publishing an ID. */
	for (i = 1; i <= 2; i++) {
		fail_at = calls + (int)i;
		CHECK(RN(intern(a, "failed", &result)));
		CHECK(!((struct em_tokens *)a->members)->count);
		fail_at = 0;
		CHECK(RP(vo_interact0(a, NEOCC_AC0_TOKENS_RESET,
			&neocc_host_caller)));
	}
#endif
	CHECK(RP(intern(a, "first", &result)));
	first = result.token;
	CHECK(first->tok == NEOCC_TOKEN_FIRST);
	CHECK(RP(intern(a, "first", &result)) && result.token == first);
	CHECK(result.next == NEOCC_TOKEN_FIRST + 1);
	CHECK(RP(intern(b, "first", &result)) && result.token != first);
	for (i = 0; i < 2000; i++) {
		snprintf(name, sizeof(name), "identifier_%zu", i);
#ifdef NEOCC_TEST_TOKEN_FAULTS
		if (i == 511) {
			fail_at = calls + 1;
			CHECK(RN(intern(a, name, &result)));
			fail_at = 0;
			CHECK(!strcmp(first->str, "first"));
		}
#endif
		CHECK(RP(intern(a, name, &result)));
		CHECK(result.token->tok == NEOCC_TOKEN_FIRST + 1 + (int)i);
	}
	CHECK(!strcmp(first->str, "first"));
	CHECK(RP(resize(a, NULL, 64, &pointer)));
	CHECK((uintptr_t)pointer % _Alignof(max_align_t) == 0);
	memset(pointer, 0x5a, 64);
	CHECK(RN(resize(b, pointer, 128, &unused)));
	CHECK(RN(resize(a, pointer, SIZE_MAX, &unused)));
#ifdef NEOCC_TEST_TOKEN_FAULTS
	fail_at = calls + 1;
	CHECK(RN(resize(a, pointer, 600000, &unused)));
	fail_at = 0;
#endif
	for (i = 0; i < 64; i++)
		CHECK(((unsigned char *)pointer)[i] == 0x5a);
	CHECK(RP(resize(a, pointer, 600000, &grown)));
	for (i = 0; i < 64; i++)
		CHECK(((unsigned char *)grown)[i] == 0x5a);
	CHECK(RP(resize(a, grown, 16, &pointer)));
	CHECK(RP(resize(a, pointer, 0, &unused)));
	CHECK(!unused);
	/* Shared open keeps storage until the final close. */
	CHECK(RP(vo_open(a, &neocc_host_caller)));
	CHECK(RP(vo_close(a, &neocc_host_caller)));
	CHECK(!strcmp(first->str, "first"));
	CHECK(RP(vo_close(a, &neocc_host_caller)));
	CHECK(!((struct em_tokens *)a->members)->table);
	CHECK(RP(vo_open(a, &neocc_host_caller)));
	CHECK(RP(intern(a, "reopened", &result)));
	CHECK(result.token->tok == NEOCC_TOKEN_FIRST);
	CHECK(RP(vo_interact0(a, NEOCC_AC0_TOKENS_RESET,
		&neocc_host_caller)));
	CHECK(RP(intern(a, "reset", &result)));
	CHECK(result.token->tok == NEOCC_TOKEN_FIRST);
	CHECK(RP(vo_close(a, &neocc_host_caller)));
	CHECK(RP(vo_close(b, &neocc_host_caller)));
	destroy_object(a);
	destroy_object(b);
	CHECK(!a && !b);
#ifdef NEOCC_TEST_TOKEN_FAULTS
	CHECK(live == initial_live);
#endif
	puts("token interning, isolation, stream preservation and cleanup passed");
	return 0;
}
