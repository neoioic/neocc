#include "neocc/symbols.h"
#include "objects/symbols.h"
#include "host/host.h"
#include "check.h"
#include <string.h>
#include <stdio.h>

#ifdef NEOCC_TEST_SYMBOL_FAULTS
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
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

static int allocate(struct object *object, struct neocc_symbol **symbol,
	unsigned int capacity)
{
	unsigned char output[sizeof(*symbol) + 1];
	unsigned int quantity = 123;
	int result = vo_interact1(object, NEOCC_AC1_SYMBOLS_ALLOCATE,
		output + 1, capacity, &quantity, &neocc_host_caller);

	CHECK(quantity == (RP(result) ? sizeof(*symbol) : 0));
	if (RP(result))
		memcpy(symbol, output + 1, sizeof(*symbol));
	clean_access(object);
	return result;
}

static int release(struct object *object, struct neocc_symbol *symbol,
	unsigned int quantity)
{
	unsigned char input[sizeof(symbol) + 1];
	int result;

	memcpy(input + 1, &symbol, sizeof(symbol));
	result = vo_interact2(object, NEOCC_AC2_SYMBOLS_RELEASE,
		input + 1, quantity, &neocc_host_caller);
	clean_access(object);
	return result;
}

static int partial_close(struct object *self, const struct vr *caller_context)
{
	(void)self;
	(void)caller_context;
	return RN_CLOSEPARTIAL;
}

int main(void)
{
	struct object *a, *b, *root;
	struct em_symbols *members;
	struct neocc_symbol *symbols[600], *other, *reused;
	int (*saved_close)(struct object *, const struct vr *);
#ifdef NEOCC_TEST_SYMBOL_FAULTS
	size_t initial_live = live;

	/* Constructor and both queues must unwind without consuming OIDs. */
	for (int i = 1; i <= 3; i++) {
		fail_at = calls + i;
		CHECK(RN(create_object(NEOCC_MSN_SYMBOLS, 8, 8, 1, &a)));
		CHECK(!a && live == initial_live);
		fail_at = 0;
	}
#endif
	CHECK(RP(create_object(NEOCC_MSN_SYMBOLS, 0, 0, 1, &a)));
	CHECK(RP(create_object(NEOCC_MSN_SYMBOLS, 0, 0, 1, &b)));
	CHECK(RN(allocate(a, &reused, sizeof(reused))));
	CHECK(RP(vo_open(a, &neocc_host_caller)));
	CHECK(RP(vo_open(b, &neocc_host_caller)));
	members = a->members;
	CHECK(allocate(a, &reused, sizeof(reused) - 1) == RN_INVALIDARG);
	CHECK(release(a, NULL, sizeof(reused)) == RN_INVALIDARG);
	CHECK(release(a, NULL, sizeof(reused) - 1) == RN_INVALIDARG);
	CHECK(!members->blocks && !members->free_first);
	for (int i = 0; i < 600; i++) {
#ifdef NEOCC_TEST_SYMBOL_FAULTS
		if (!members->free_first) {
			struct neocc_symbol_block *blocks = members->blocks;

			fail_at = calls + 1;
			CHECK(RN(allocate(a, &reused, sizeof(reused))));
			CHECK(members->blocks == blocks && !members->free_first);
			fail_at = 0;
		}
#endif
		CHECK(RP(allocate(a, &symbols[i], sizeof(symbols[i]))));
		CHECK((uintptr_t)symbols[i] % _Alignof(struct neocc_symbol) == 0);
		memset(symbols[i], 0, sizeof(*symbols[i]));
		symbols[i]->v = i + 17;
		for (int j = 0; j < i; j++)
			CHECK(symbols[i] != symbols[j] && symbols[j]->v == j + 17);
	}
	CHECK(RP(allocate(b, &other, sizeof(other))));
	other->v = 73;
	for (int i = 0; i < 600; i++) {
		CHECK(other != symbols[i]);
		CHECK(RP(release(a, symbols[i], sizeof(symbols[i]))));
	}
	CHECK(RP(allocate(a, &reused, sizeof(reused))));
	CHECK(reused == symbols[599]);
	CHECK(RP(vo_open(a, &neocc_host_caller)));
	CHECK(RP(vo_close(a, &neocc_host_caller)));
	CHECK(reused->v == 616);
	CHECK(RP(vo_interact0(a, NEOCC_AC0_SYMBOLS_RESET, &neocc_host_caller)));
	CHECK(!members->blocks && !members->free_first && other->v == 73);
	CHECK(RP(allocate(a, &reused, sizeof(reused))));
	reused->v = 91;
	root = get_base_node(a, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(a, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(members->blocks && reused->v == 91);
	CHECK(!host_destroy_object(a));
	root->close = saved_close;
	CHECK(RP(vo_close(a, &neocc_host_caller)));
	CHECK(!members->blocks && !members->free_first);
	CHECK(RN(allocate(a, &reused, sizeof(reused))));
	CHECK(RP(vo_open(a, &neocc_host_caller)));
	CHECK(RP(allocate(a, &reused, sizeof(reused))));
	CHECK(RP(vo_close(a, &neocc_host_caller)));
	CHECK(RP(vo_close(b, &neocc_host_caller)));
	clean_access(a);
	clean_access(b);
	destroy_object(a);
	destroy_object(b);
	CHECK(!a && !b);
#ifdef NEOCC_TEST_SYMBOL_FAULTS
	CHECK(live == initial_live);
#endif
	puts("semantic symbol pools, reuse, isolation and cleanup passed");
	return 0;
}
