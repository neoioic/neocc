#include "objects/switches.h"
#include "host/host.h"
#include "check.h"
#include <string.h>
#include <stdio.h>

#ifdef NEOCC_TEST_SWITCH_FAULTS
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void __real_free(void *);
void *__real_realloc(void *, size_t);
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

static void *query_ok(struct object *object, unsigned int action)
{
	void *pointer = NULL;

	CHECK(RP(query(object, action, &pointer, sizeof(pointer))));
	return pointer;
}

static int command(struct object *object, unsigned int action, void *input,
	unsigned int quantity)
{
	unsigned char bytes[sizeof(void *) + 1];
	int result;

	memcpy(bytes + 1, input, quantity);
	result = vo_interact2(object, action, bytes + 1, quantity,
		&neocc_host_caller);
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
	struct neocc_switches *state, *second;
	struct neocc_switch *frames[258], *sw, *foreign;
	struct neocc_case *record;
	void *pointer;
	int index;
	int (*saved_close)(struct object *, const struct vr *);
#ifdef NEOCC_TEST_SWITCH_FAULTS
	size_t initial_live = live;

	for (int n = 1; n <= 3; n++) {
		fail_at = calls + n;
		CHECK(RN(create_object(NEOCC_MSN_SWITCHES, 8, 8, 1, &object)));
		CHECK(!object && live == initial_live);
		fail_at = 0;
	}
#endif
	CHECK(RP(create_object(NEOCC_MSN_SWITCHES, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_SWITCHES, 0, 0, 1, &other)));
	CHECK(RN(query(object, NEOCC_AC1_SWITCHES_BORROW, &pointer, sizeof(pointer))));
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	state = query_ok(object, NEOCC_AC1_SWITCHES_BORROW);
	second = query_ok(other, NEOCC_AC1_SWITCHES_BORROW);
	CHECK(state != second && !state->current && !second->current);
	CHECK(RN(query(object, NEOCC_AC1_SWITCHES_ENTER, &pointer, sizeof(pointer) - 1)));
	CHECK(!state->current);
	CHECK(RN(query(object, NEOCC_AC1_SWITCHES_CASE, &pointer, sizeof(pointer))));
	foreign = query_ok(other, NEOCC_AC1_SWITCHES_ENTER);
	for (int n = 0; n < 258; n++) {
#ifdef NEOCC_TEST_SWITCH_FAULTS
		fail_at = calls + 1;
		CHECK(RN(query(object, NEOCC_AC1_SWITCHES_ENTER, &pointer, sizeof(pointer))));
		CHECK(state->current == (n ? frames[n - 1] : NULL));
		fail_at = 0;
#endif
		sw = frames[n] = query_ok(object, NEOCC_AC1_SWITCHES_ENTER);
		CHECK(state->current == sw && sw->prev == (n ? frames[n - 1] : NULL));
		CHECK((uintptr_t)sw % _Alignof(struct neocc_switch) == 0);
		CHECK(!sw->n && !sw->p && !sw->bsym && !sw->sv.type.ref);
		for (int i = 0; i < 17; i++) {
#ifdef NEOCC_TEST_SWITCH_FAULTS
			int allocations = sw->n == sw->capacity ? 2 : 1;
			for (int j = 1; j <= allocations; j++) {
				struct neocc_case **old = sw->p;
				int capacity = sw->capacity;
				size_t before = live;

				fail_at = calls + j;
				CHECK(RN(query(object, NEOCC_AC1_SWITCHES_CASE, &pointer, sizeof(pointer))));
				CHECK(sw->n == i && sw->p == old && sw->capacity == capacity);
				CHECK(live == before);
				fail_at = 0;
			}
#endif
			record = query_ok(object, NEOCC_AC1_SWITCHES_CASE);
			CHECK(!record->v1 && !record->v2 && !record->ind && !record->line);
			record->v1 = i;
		}
		CHECK(RN(command(object, NEOCC_AC2_SWITCHES_EXIT, &foreign, sizeof(foreign))));
		CHECK(state->current == sw);
		index = sw->n;
		CHECK(RN(command(object, NEOCC_AC2_SWITCHES_REMOVE, &index, sizeof(index))));
		index = 3;
		CHECK(RN(command(object, NEOCC_AC2_SWITCHES_REMOVE, &index, sizeof(index) - 1)));
		CHECK(RP(command(object, NEOCC_AC2_SWITCHES_REMOVE, &index, sizeof(index))));
		CHECK(sw->n == 16 && sw->p[3]->v1 == 4);
		for (int i = 0; i < sw->n; i++)
			CHECK(sw->p[i]->v1 == (i < 3 ? i : i + 1));
	}
	CHECK(second->current == foreign && !foreign->n);
	for (int n = 257; n >= 0; n--) {
		if (n)
			CHECK(RN(command(object, NEOCC_AC2_SWITCHES_EXIT, &frames[n - 1], sizeof(sw))));
		CHECK(RP(command(object, NEOCC_AC2_SWITCHES_EXIT, &frames[n], sizeof(sw))));
	}
	CHECK(!state->current);
	sw = query_ok(object, NEOCC_AC1_SWITCHES_ENTER);
	query_ok(object, NEOCC_AC1_SWITCHES_CASE);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(state->current == sw);
	CHECK(RP(vo_interact0(object, NEOCC_AC0_SWITCHES_RESET, &neocc_host_caller)));
	CHECK(!state->current && state == query_ok(object, NEOCC_AC1_SWITCHES_BORROW));
	/* Cleanup must not follow parser stack addresses after longjmp. */
	sw = query_ok(object, NEOCC_AC1_SWITCHES_ENTER);
	sw->bsym = (int *)(uintptr_t)1;
	sw->scope = (struct neocc_scope *)(uintptr_t)1;
	query_ok(object, NEOCC_AC1_SWITCHES_CASE);
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(state->current == sw);
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RN(query(object, NEOCC_AC1_SWITCHES_BORROW, &pointer, sizeof(pointer))));
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	state = query_ok(object, NEOCC_AC1_SWITCHES_BORROW);
	CHECK(!state->current);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	CHECK(host_destroy_object(object));
	CHECK(host_destroy_object(other));
	free(object);
	free(other);
#ifdef NEOCC_TEST_SWITCH_FAULTS
	CHECK(live == initial_live);
#endif
	puts("switch ownership, growth, isolation and cleanup passed");
	return 0;
}
