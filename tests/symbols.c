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

static int push_stack(struct object *object, unsigned int stack, int v,
	struct neocc_symbol **symbol)
{
	struct neocc_symbol_push request = { stack, v, 0, 0 };
	struct neocc_symbol_stack_result result;
	unsigned char input[sizeof(request) + 1];
	unsigned char output[sizeof(result) + 1];
	unsigned int quantity = 123;
	int code;

	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact3(object, NEOCC_AC3_SYMBOLS_PUSH, input + 1,
		sizeof(request), output + 1, sizeof(result), &quantity,
		&neocc_host_caller);
	CHECK(quantity == (RP(code) ? sizeof(result) : 0));
	if (RP(code)) {
		memcpy(&result, output + 1, sizeof(result));
		*symbol = result.top;
	}
	clean_access(object);
	return code;
}

static int copy_stack(struct object *object, unsigned int stack,
	struct neocc_symbol *source, struct neocc_symbol **symbol)
{
	struct neocc_symbol_copy request = { stack, source };
	struct neocc_symbol_stack_result result;
	unsigned char input[sizeof(request) + 1];
	unsigned char output[sizeof(result) + 1];
	unsigned int quantity = 123;
	int code;

	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact3(object, NEOCC_AC3_SYMBOLS_COPY, input + 1,
		sizeof(request), output + 1, sizeof(result), &quantity,
		&neocc_host_caller);
	CHECK(quantity == (RP(code) ? sizeof(result) : 0));
	if (RP(code)) {
		memcpy(&result, output + 1, sizeof(result));
		*symbol = result.top;
	}
	clean_access(object);
	return code;
}

static int top_stack(struct object *object, unsigned int stack,
	struct neocc_symbol **symbol)
{
	struct neocc_symbol_stack_query request = { stack };
	struct neocc_symbol_stack_result result;
	unsigned char input[sizeof(request) + 1];
	unsigned char output[sizeof(result) + 1];
	unsigned int quantity = 123;
	int code;

	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact3(object, NEOCC_AC3_SYMBOLS_TOP, input + 1,
		sizeof(request), output + 1, sizeof(result), &quantity,
		&neocc_host_caller);
	CHECK(quantity == (RP(code) ? sizeof(result) : 0));
	if (RP(code)) {
		memcpy(&result, output + 1, sizeof(result));
		*symbol = result.top;
	}
	clean_access(object);
	return code;
}

static int drop_stack(struct object *object, unsigned int stack,
	struct neocc_symbol *symbol)
{
	struct neocc_symbol_drop request = { stack, symbol };
	unsigned char input[sizeof(request) + 1];
	int code;

	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact2(object, NEOCC_AC2_SYMBOLS_DROP, input + 1,
		sizeof(request), &neocc_host_caller);
	clean_access(object);
	return code;
}

static int move_stack(struct object *object, unsigned int source,
	unsigned int destination, struct neocc_symbol *symbol, int copy,
	struct neocc_symbol **replacement, unsigned int *quantity)
{
	struct neocc_symbol_move request = {
		source, destination, symbol, copy
	};
	struct neocc_symbol_move_result result;
	unsigned char input[sizeof(request) + 1];
	unsigned char output[sizeof(result) + 1];
	int code;

	*quantity = 123;
	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact3(object, NEOCC_AC3_SYMBOLS_MOVE, input + 1,
		sizeof(request), output + 1, sizeof(result), quantity,
		&neocc_host_caller);
	CHECK(*quantity == (RP(code) ? sizeof(result) : 0));
	if (RP(code)) {
		memcpy(&result, output + 1, sizeof(result));
		*replacement = result.replacement;
	}
	clean_access(object);
	return code;
}

static int scope_get(struct object *object)
{
	unsigned char output[sizeof(int) + 1];
	unsigned int quantity = 123;
	int scope = -1;
	int code = vo_interact1(object, NEOCC_AC1_SYMBOLS_SCOPE, output + 1,
		sizeof(int), &quantity, &neocc_host_caller);

	CHECK(code == R_P && quantity == sizeof(int));
	memcpy(&scope, output + 1, sizeof(scope));
	clean_access(object);
	return scope;
}

static int scope_add(struct object *object, int change)
{
	struct neocc_symbol_scope request = { change };
	unsigned char input[sizeof(request) + 1];
	int code;

	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact2(object, NEOCC_AC2_SYMBOLS_SCOPE, input + 1,
		sizeof(request), &neocc_host_caller);
	clean_access(object);
	return code;
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
	struct neocc_symbol *g1, *g2, *l1, *lc, *d1, *gl, *ll;
	struct neocc_symbol *moved, *replacement;
	unsigned int quantity;
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
	CHECK(!members->scope);
	for (int i = 0; i < NEOCC_SYMBOL_STACK_COUNT; i++) {
		struct neocc_symbol *top;

		CHECK(RP(top_stack(a, i, &top)) && !top);
		CHECK(RP(top_stack(b, i, &top)) && !top);
	}
#ifdef NEOCC_TEST_SYMBOL_FAULTS
	fail_at = calls + 1;
	CHECK(RN(push_stack(a, NEOCC_SYMBOL_STACK_GLOBAL, 1, &g1)));
	CHECK(!members->blocks && !members->stacks[NEOCC_SYMBOL_STACK_GLOBAL]);
	fail_at = 0;
#endif
	CHECK(scope_get(a) == 0);
	CHECK(RP(scope_add(a, 1)) && scope_get(a) == 1);
	CHECK(RP(scope_add(a, 2)) && scope_get(a) == 3);
	CHECK(scope_add(a, -4) == RN_INVALIDARG && scope_get(a) == 3);
	CHECK(RP(scope_add(a, -3)) && !scope_get(a));

	CHECK(RP(push_stack(a, NEOCC_SYMBOL_STACK_GLOBAL, 101, &g1)));
	CHECK(RP(push_stack(a, NEOCC_SYMBOL_STACK_GLOBAL, 102, &g2)));
	CHECK(RP(push_stack(a, NEOCC_SYMBOL_STACK_LOCAL, 201, &l1)));
	CHECK(RP(push_stack(a, NEOCC_SYMBOL_STACK_DEFINE, 301, &d1)));
	CHECK(RP(push_stack(a, NEOCC_SYMBOL_STACK_GLOBAL_LABEL, 401, &gl)));
	CHECK(RP(push_stack(a, NEOCC_SYMBOL_STACK_LOCAL_LABEL, 501, &ll)));
	CHECK(g1->v == 101 && g2->prev == g1 && !g1->prev);
	CHECK(d1->v == 301 && gl->v == 401 && ll->v == 501);
	CHECK(RP(top_stack(a, NEOCC_SYMBOL_STACK_GLOBAL, &moved)) &&
		moved == g2);
	CHECK(RP(top_stack(a, NEOCC_SYMBOL_STACK_DEFINE, &moved)) &&
		moved == d1);
	CHECK(RP(top_stack(b, NEOCC_SYMBOL_STACK_GLOBAL, &moved)) && !moved);

	CHECK(RP(copy_stack(a, NEOCC_SYMBOL_STACK_LOCAL, g1, &lc)));
	CHECK(lc != g1 && lc->v == g1->v);
	CHECK(RP(move_stack(a, NEOCC_SYMBOL_STACK_GLOBAL,
		NEOCC_SYMBOL_STACK_LOCAL, g1, 0, &replacement, &quantity)));
	CHECK(quantity == sizeof(struct neocc_symbol_move_result) &&
		!replacement);
	CHECK(RP(top_stack(a, NEOCC_SYMBOL_STACK_LOCAL, &moved)) &&
		moved == g1);
	CHECK(RP(top_stack(a, NEOCC_SYMBOL_STACK_GLOBAL, &moved)) &&
		moved == g2);
	CHECK(RP(move_stack(a, NEOCC_SYMBOL_STACK_LOCAL,
		NEOCC_SYMBOL_STACK_GLOBAL, l1, 1, &replacement, &quantity)));
	CHECK(replacement && replacement != l1 && replacement->v == l1->v);
	CHECK(lc->prev == replacement && !replacement->prev);
	CHECK(RP(top_stack(a, NEOCC_SYMBOL_STACK_GLOBAL, &moved)) &&
		moved == l1);
	CHECK(move_stack(a, NEOCC_SYMBOL_STACK_LOCAL,
		NEOCC_SYMBOL_STACK_GLOBAL, other, 0, &replacement, &quantity)
		== RO_VAINLY && !quantity);
	CHECK(move_stack(a, NEOCC_SYMBOL_STACK_LOCAL,
		NEOCC_SYMBOL_STACK_LOCAL, l1, 0, &replacement, &quantity)
		== RN_INVALIDARG);
	CHECK(RP(drop_stack(a, NEOCC_SYMBOL_STACK_LOCAL, g1)));
	CHECK(RP(top_stack(a, NEOCC_SYMBOL_STACK_LOCAL, &moved)) &&
		moved == lc);
	CHECK(drop_stack(a, NEOCC_SYMBOL_STACK_LOCAL, g1) == RN_INVALIDARG);
	CHECK(RP(vo_interact0(a, NEOCC_AC0_SYMBOLS_RESET,
		&neocc_host_caller)));
	CHECK(!members->blocks && !members->free_first && !members->scope);
	for (int i = 0; i < NEOCC_SYMBOL_STACK_COUNT; i++)
		CHECK(!members->stacks[i]);

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
	CHECK(RP(push_stack(a, NEOCC_SYMBOL_STACK_LOCAL, 701, &l1)));
	CHECK(RP(scope_add(a, 1)));
	root = get_base_node(a, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(a, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(members->blocks && reused->v == 91 && l1->v == 701);
	CHECK(members->scope == 1 &&
		members->stacks[NEOCC_SYMBOL_STACK_LOCAL] == l1);
	CHECK(!host_destroy_object(a));
	root->close = saved_close;
	CHECK(RP(vo_close(a, &neocc_host_caller)));
	CHECK(!members->blocks && !members->free_first && !members->scope);
	for (int i = 0; i < NEOCC_SYMBOL_STACK_COUNT; i++)
		CHECK(!members->stacks[i]);
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
	puts("semantic symbol pools, stacks, scope, reuse and cleanup passed");
	return 0;
}
