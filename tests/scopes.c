#include "objects/scopes.h"
#include "host/host.h"
#include "check.h"
#include <string.h>
#include <stdio.h>

#ifdef NEOCC_TEST_SCOPE_FAULTS
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
	for (unsigned int i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static int query(struct object *object, unsigned int action, void *out,
	unsigned int capacity)
{
	unsigned char output[sizeof(void *) + 1];
	unsigned int quantity = 123;
	int result = vo_interact1(object, action, output + 1, capacity,
		&quantity, &neocc_host_caller);

	CHECK(quantity == (RP(result) ? sizeof(void *) : 0));
	if (RP(result))
		memcpy(out, output + 1, sizeof(void *));
	clean_access(object);
	return result;
}

static struct neocc_scope *enter(struct object *object)
{
	struct neocc_scope *scope;

	CHECK(RP(query(object, NEOCC_AC1_SCOPES_ENTER, &scope, sizeof(scope))));
	return scope;
}

static int leave(struct object *object, struct neocc_scope *scope)
{
	unsigned char input[sizeof(scope) + 1];
	int result;

	memcpy(input + 1, &scope, sizeof(scope));
	result = vo_interact2(object, NEOCC_AC2_SCOPES_EXIT,
		input + 1, sizeof(scope), &neocc_host_caller);
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
	struct object *object = NULL, *other = NULL, *root;
	struct neocc_scopes *state, *second;
	struct neocc_scope *scopes[258], *scope;
	struct neocc_symbol symbol = { 0 };
	struct em_scopes *members;
	int jump = 0;
	int (*saved_close)(struct object *, const struct vr *);

#ifdef NEOCC_TEST_SCOPE_FAULTS
	size_t initial_live = live;

	for (int n = 1; n <= 3; n++) {
		fail_at = calls + n;
		CHECK(RN(create_object(NEOCC_MSN_SCOPES, 8, 8, 1, &object)));
		CHECK(!object && live == initial_live);
		fail_at = 0;
	}
#endif
	CHECK(RP(create_object(NEOCC_MSN_SCOPES, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_SCOPES, 0, 0, 1, &other)));
	CHECK(query(object, NEOCC_AC1_SCOPES_BORROW, &state,
		sizeof(state)) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	CHECK(RP(query(object, NEOCC_AC1_SCOPES_BORROW, &state, sizeof(state))));
	CHECK(RP(query(other, NEOCC_AC1_SCOPES_BORROW, &second, sizeof(second))));
	CHECK(state != second && !state->current && !second->root);
	members = object->members;
	CHECK(query(object, NEOCC_AC1_SCOPES_ENTER, &scope,
		sizeof(scope) - 1) == RN_INVALIDARG);
	CHECK(!state->current && !members->active && !members->available);
#ifdef NEOCC_TEST_SCOPE_FAULTS
	fail_at = calls + 1;
	CHECK(RN(query(object, NEOCC_AC1_SCOPES_ENTER, &scope, sizeof(scope))));
	CHECK(!state->current && !members->active && !members->available);
	fail_at = 0;
#endif
	scopes[0] = enter(object);
	CHECK(scopes[0] == state->root && !scopes[0]->prev);
	scopes[0]->bsym = &jump;
	scopes[0]->cl.s = &symbol;
	scopes[0]->cl.n = 9;
	scopes[0]->vla.num = 3;
	scopes[0]->vla.loc = 17;
	for (int n = 1; n < 258; n++) {
#ifdef NEOCC_TEST_SCOPE_FAULTS
		fail_at = calls + 1;
		CHECK(RN(query(object, NEOCC_AC1_SCOPES_ENTER,
			&scope, sizeof(scope))));
		CHECK(state->current == scopes[n - 1] && state->root == scopes[0]);
		fail_at = 0;
#endif
		scopes[n] = enter(object);
		CHECK(scopes[n]->prev == scopes[n - 1]);
		CHECK(scopes[n]->cl.s == &symbol && scopes[n]->cl.n == 9);
		CHECK(scopes[n]->bsym == &jump && !scopes[n]->vla.num);
		CHECK(scopes[n]->vla.loc == 17);
	}
	CHECK(!second->current && !second->all_cleanups);
	CHECK(leave(other, state->current) == RN_INVALIDARG);
	CHECK(leave(object, scopes[0]) == RN_INVALIDARG);
	state->loop = state->current;
	CHECK(leave(object, state->current) == RN_INVALIDARG);
	state->loop = NULL;
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(state->current == scopes[257]);
	CHECK(vo_interact2(object, NEOCC_AC2_SCOPES_EXIT,
		(unsigned char *)&scope, sizeof(scope) - 1,
		&neocc_host_caller) == RN_INVALIDARG);
	for (int n = 257; n >= 0; n--)
		CHECK(RP(leave(object, scopes[n])));
	CHECK(!state->current && !state->root && !members->active);
#ifdef NEOCC_TEST_SCOPE_FAULTS
	fail_at = calls + 1;
#endif
	/* All frames must be reusable without another allocation. */
	for (int n = 0; n < 258; n++)
		CHECK(enter(object) == scopes[n]);
#ifdef NEOCC_TEST_SCOPE_FAULTS
	fail_at = 0;
#endif
	CHECK(RP(leave(object, state->current)));
	state->loop = scopes[0];
	state->all_cleanups = state->pending_gotos = &symbol;
	CHECK(RP(vo_interact0(object, NEOCC_AC0_SCOPES_RESET,
		&neocc_host_caller)));
	CHECK(!state->current && !state->root && !state->loop);
	CHECK(!state->all_cleanups && !state->pending_gotos);
	CHECK(!members->active && !members->available);
	CHECK(RP(query(object, NEOCC_AC1_SCOPES_BORROW, &second, sizeof(second))));
	CHECK(second == state);
	scope = enter(object);
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(state->current == scope && members->active);
	CHECK(!host_destroy_object(object));
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(!members->active && !members->available);
	CHECK(query(object, NEOCC_AC1_SCOPES_ENTER, &scope,
		sizeof(scope)) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(query(object, NEOCC_AC1_SCOPES_BORROW, &state, sizeof(state))));
	CHECK(!state->current && !state->root);
	enter(object);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
#ifdef NEOCC_TEST_SCOPE_FAULTS
	CHECK(live == initial_live);
#endif
	puts("scope frame nesting, reuse, isolation and cleanup passed");
	return 0;
}
