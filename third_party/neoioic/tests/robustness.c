#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "check.h"
/* Internal seams for malformed command arguments and OID exhaustion. */
#include "../src/neoioic.c"
#include "../examples/multiple_inheritance/host.c"

static int fail_alloc;
static int track_allocations;
static void *allocations[128];
static unsigned int live_allocations;
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);

void *__wrap_calloc(size_t count, size_t size)
{
	void *pointer;

	/* Static libc startup may call calloc before the test suite begins. */
	if (!track_allocations)
		return __real_calloc(count, size);
	if (fail_alloc && --fail_alloc == 0)
		return NULL;
	pointer = __real_calloc(count, size);
	if (pointer != NULL) {
		CHECK(live_allocations < sizeof(allocations) / sizeof(*allocations));
		allocations[live_allocations++] = pointer;
	}
	return pointer;
}

void __wrap_free(void *pointer)
{
	for (unsigned int i = 0; i < live_allocations; i++) {
		if (allocations[i] != pointer)
			continue;
		allocations[i] = allocations[--live_allocations];
		break;
	}
	__real_free(pointer);
}

static const struct vr context = { 1, 1 };

static void test_queue(void)
{
	unsigned char storage[17], expected[17], input[30], output[30];
	unsigned int seed = 1;
	for (unsigned int cap = 0; cap <= sizeof(storage); cap++) {
		struct queue q = { .data = storage, .capacity = cap };
		unsigned int count = 0;
		unsigned int expected_lost = 0;
		for (unsigned int step = 0; step < 10000; step++) {
			seed = seed * 1664525U + 1013904223U;
			unsigned int size = 1 + seed % sizeof(input);
			unsigned int op = seed >> 28;
			if (op < 6) {
				unsigned int n = cap - count;
				if (n > size)
					n = size;
				memset(input, (unsigned char)step, size);
				int result = enqueue(&q, input, size);
				CHECK(result == (n < size ? RN_LOST : R_P));
				if (n < size)
					expected_lost = size - n;
				memcpy(expected + count, input, n);
				count += n;
			} else if (op < 12) {
				unsigned int n = count < size ? count : size, got;
				unsigned int lost = expected_lost;
				memset(output, 0xa5, sizeof(output));
				int result = dequeue(&q, output, size, &got);
				CHECK(result == (n ? (lost ? RN_LOST : R_P) : RO_VAINLY));
				CHECK(got == n && memcmp(output, expected, n) == 0);
				for (unsigned int i = n; i < sizeof(output); i++)
					CHECK(output[i] == 0xa5);
				if (n)
					expected_lost = 0;
				memmove(expected, expected + n, count - n);
				count -= n;
			} else if (op == 12 && count < cap) {
				ENQ_PLACE_BYTE(&q, (unsigned char)step);
				expected[count++] = (unsigned char)step;
			} else if (op == 13 && count) {
				unsigned char byte;
				DEQ_BYTE(&q, &byte);
				CHECK(byte == expected[0]);
				memmove(expected, expected + 1, --count);
			} else if (op == 14 && cap && count == cap) {
				ENQ_PUSH_BYTE(&q, (unsigned char)step);
				memmove(expected, expected + 1, count - 1);
				expected[count - 1] = (unsigned char)step;
			}
			CHECK(q.quantity == count);
			CHECK(q.lost == expected_lost);
			CHECK(enqueue(&q, input, 0) == RO_VAINLY);
			if (!count)
				CHECK(q.front == NULL && q.rear == NULL);
			else {
				CHECK(q.front >= storage && q.front < storage + cap);
				CHECK(q.rear >= storage && q.rear < storage + cap);
				for (unsigned int i = 0; i < count; i++)
					CHECK(storage[(q.front - storage + i) % cap] == expected[i]);
			}
		}
	}
}

static void test_root(void)
{
	struct em_neoioic members = { 0 };
	struct vr slot = { 0 };
	struct object root = { .members = &members,
		       .callers = &slot, .caller_count = 1
	};
	unsigned char bytes[sizeof(unsigned int) + 2];
	unsigned int value = 8, got;
	tog_neoioic(&root);
	CHECK(vo_close(&root, &context) == RO_NOPEN);
	root.ref_count = UINT_MAX;
	CHECK(vo_open(&root, &context) == RN_FULL && root.ref_count == UINT_MAX);
	root.ref_count = 0;
	CHECK(vo_open(&root, &context) == R_P && root.ref_count == 1);
	CHECK(vo_close(&root, &context) == R_P && root.ref_count == 0);
	for (unsigned int side = 0; side < 2; side++) {
		unsigned int set = side ? SET_OQCTY : SET_IQCTY;
		unsigned int get = side ? GET_OQCTY : GET_IQCTY;
		struct queue *q = side ? &members.oq : &members.iq;
		memcpy(bytes + 1, &value, sizeof(value));
		CHECK(root.interact2(&root, set, bytes + 1, sizeof(value), &context) == R_P);
		CHECK(q->capacity == 8 && q->front == NULL && q->rear == NULL);
		CHECK(enqueue(q, bytes, 2) == R_P);
		struct queue saved = *q;
		unsigned char saved_data[8];
		memcpy(saved_data, q->data, sizeof(saved_data));
		fail_alloc = 1;
		CHECK(root.interact2(&root, set, bytes + 1, sizeof(value), &context) == R_N);
		CHECK(memcmp(q, &saved, sizeof(saved)) == 0);
		CHECK(memcmp(q->data, saved_data, sizeof(saved_data)) == 0);
		CHECK(root.interact2(&root, set, NULL, sizeof(value),
				     &context) == RN_INVALIDARG);
		CHECK(root.interact2(&root, set, bytes, sizeof(value) + 1,
				     &context) == RN_INVALIDARG);
		CHECK(memcmp(q, &saved, sizeof(saved)) == 0);
		CHECK(root.interact2(&root, set, bytes + 1, 1, &context) == RN_INVALIDARG);
		CHECK(memcmp(q, &saved, sizeof(saved)) == 0);
		memset(bytes, 0xcc, sizeof(bytes));
		got = 99;
		CHECK(root.interact1(&root, get, bytes + 1, 1, &got,
				     &context) == RN_INVALIDARG);
		CHECK(got == 0);
		CHECK(root.interact1(&root, get, NULL, sizeof(value), &got,
				     &context) == RN_INVALIDARG);
		CHECK(got == 0);
		CHECK(root.interact1(&root, get, bytes, sizeof(value), NULL,
				     &context) == RN_INVALIDARG);
		for (size_t i = 0; i < sizeof(bytes); i++)
			CHECK(bytes[i] == 0xcc);
		CHECK(root.interact1(&root, get, bytes + 1, sizeof(value), &got,
				     &context) == R_P);
		memcpy(&value, bytes + 1, sizeof(value));
		CHECK(value == 8 && got == sizeof(value));
		/* The capacity argument may reside in the queue being replaced. */
		memcpy(q->data, &value, sizeof(value));
		CHECK(root.interact2(&root, set, q->data, sizeof(value), &context) == R_P);
		value = 0;
		memcpy(bytes + 1, &value, sizeof(value));
		CHECK(root.interact2(&root, set, bytes + 1, sizeof(value), &context) == R_P);
		CHECK(q->data == NULL && q->capacity == 0 && q->quantity == 0);
		value = 8;
	}
	/* Reset clears data/loss state without replacing queue storage or modes. */
	for (unsigned int side = 0; side < 2; side++) {
		unsigned int set = side ? SET_OQCTY : SET_IQCTY;
		struct queue *q = side ? &members.oq : &members.iq;

		CHECK(vo_interact2(&root, set, (unsigned char *)&value,
				   sizeof(value), &context) == R_P);
		CHECK(enqueue(q, bytes, 2) == R_P);
		q->lost = 3;
	}
	CHECK(vo_interact0(&root, SET_UNBLOCKI, &context) == R_P);
	CHECK(vo_interact0(&root, SET_UNBLOCKO, &context) == R_P);
	CHECK(members.mds == (MOD_BLOCKI | MOD_BLOCKO));
	CHECK(vo_interact0(&root, CMD_RESET, &context) == R_P);
	CHECK(members.mds == (MOD_BLOCKI | MOD_BLOCKO));
	CHECK(vo_interact0(&root, SET_BLOCKI, &context) == R_P);
	CHECK(vo_interact0(&root, SET_BLOCKO, &context) == R_P);
	CHECK(members.mds == 0);
	for (unsigned int side = 0; side < 2; side++) {
		struct queue *q = side ? &members.oq : &members.iq;

		CHECK(q->data != NULL && q->capacity == value);
		CHECK(q->quantity == 0 && q->lost == 0);
		CHECK(q->front == NULL && q->rear == NULL);
		free(q->data);
	}
}

static int nested_interact0(struct object *self, unsigned int action,
			    const struct vr *caller_context)
{
	struct vr delegated = *caller_context;

	CHECK(self->callers[0].cr == caller_context->cr);
	CHECK(vo_interact0(self, action, caller_context) == R_N);
	if (action == AC0_) {
		delegated.mr++;
		CHECK(vo_interact0(self, action + 1, &delegated) == R_P);
		CHECK(self->callers[0].mr == caller_context->mr);
		CHECK(self->callers[1].cr == 0);
	}
	return R_P;
}

static void test_access_context(void)
{
	struct vr slots[2] = { 0 };
	struct object object = { .callers = slots, .caller_count = 2,
		       .interact0 = nested_interact0
	};

	CHECK(vo_interact0(&object, AC0_, &context) == R_P);
	CHECK(slots[0].cr == 0 && slots[0].mr == 0);
	CHECK(slots[1].cr == 0 && slots[1].mr == 0);
}

static int close_failure;
static int fail_model = 2;
static int close_calls[3];
static int leaf_close(struct object *self, const struct vr *caller_context)
{
	(void)caller_context;
	close_calls[self->model_id]++;
	if (self->model_id == fail_model && close_failure)
		return R_N;
	self->ref_count--;
	return R_P;
}
static int parent_close(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_CLOSE_;
	return R_P;
}
static int leaf_open(struct object *self, const struct vr *caller_context)
{
	(void)caller_context;
	if (self->model_id == 2)
		return R_N;
	self->ref_count++;
	return R_P;
}
static int parent_open(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_OPEN_;
	return R_P;
}
static void test_open_cleanup(void)
{
	struct vr slots[6] = { 0 };
	struct em_neoioic members = { 0 };
	struct object root = { .model_id = MSN_NEOIOIC, .members = &members };
	struct object a = { .model_id = 1, .open = leaf_open, .close = leaf_close,
		       .callers = slots + 2, .caller_count = 2
	};
	struct object b = { .model_id = 2, .open = leaf_open, .close = leaf_close,
		       .callers = slots + 4, .caller_count = 2
	};
	struct object *bases[] = { &root, &a, &b };
	struct object p = { .oid = 7, .open = parent_open, .close = parent_close,
		       .callers = slots, .caller_count = 2, .direct_bases = bases + 1,
		       .direct_base_count = 2, .bases = bases, .base_count = 3
	};
	close_failure = 1;
	fail_model = 1;
	CHECK(vo_open(&p, &context) == RN_CLOSEPARTIAL);
	CHECK(p.ref_count == 1 && a.ref_count == 1 && b.ref_count == 0);
	close_failure = 0;
	fail_model = 2;
	CHECK(vo_close(&p, &context) == R_P);
	CHECK(p.ref_count == 0 && a.ref_count == 0 && b.ref_count == 0);
	CHECK(vo_open(&p, &context) == R_N);
	CHECK(p.ref_count == 0 && a.ref_count == 0 && b.ref_count == 0);
}
static void test_close(void)
{
	struct vr slots[3] = { 0 };
	struct object a = { .model_id = 1, .ref_count = 1, .close = leaf_close,
		       .callers = slots + 1, .caller_count = 1
	};
	struct object b = { .model_id = 2, .ref_count = 1, .close = leaf_close,
		       .callers = slots + 2, .caller_count = 1
	};
	struct object *bases[] = { &a, &b };
	struct object p = { .ref_count = 1, .close = parent_close,
		       .callers = slots, .caller_count = 1, .direct_bases = bases,
		       .direct_base_count = 2, .bases = bases, .base_count = 2
	};
	close_failure = 1;
	fail_model = 1;
	CHECK(vo_close(&p, &context) == R_N);
	CHECK(p.ref_count == 1 && a.ref_count == 1 && b.ref_count == 1);
	memset(close_calls, 0, sizeof(close_calls));
	fail_model = 2;
	CHECK(vo_close(&p, &context) == RN_CLOSEPARTIAL);
	CHECK(p.ref_count == 1 && a.ref_count == 0 && b.ref_count == 1);
	CHECK(vo_close(&p, &context) == RN_CLOSEPARTIAL);
	close_failure = 0;
	CHECK(vo_close(&p, &context) == R_P);
	CHECK(p.ref_count == 0 && a.ref_count == 0 && b.ref_count == 0);
	CHECK(close_calls[1] == 1 && close_calls[2] == 3);
	CHECK(vo_close(&p, &context) == RO_NOPEN);
	/* Access-slot failure after an earlier successful close. */
	p.ref_count = a.ref_count = b.ref_count = 1;
	slots[2] = (struct vr){ 9, 9 };
	CHECK(vo_close(&p, &context) == RN_CLOSEPARTIAL);
	slots[2] = (struct vr){ 0 };
	CHECK(vo_close(&p, &context) == R_P);
	/* Propagate partial close through another inheritance level. */
	struct vr top_slot = { 0 };
	struct object *all[] = { &p, &a, &b };
	struct object top = { .ref_count = 1, .close = parent_close,
		       .callers = &top_slot, .caller_count = 1, .direct_bases = all,
		       .direct_base_count = 1, .bases = all, .base_count = 3
	};
	p.ref_count = a.ref_count = b.ref_count = 1;
	close_failure = 1;
	CHECK(vo_close(&top, &context) == RN_CLOSEPARTIAL);
	CHECK(top.ref_count == 1 && p.ref_count == 1);
	close_failure = 0;
	CHECK(vo_close(&top, &context) == R_P);
	CHECK(top.ref_count == 0 && p.ref_count == 0);
}

static void test_construction(void)
{
	struct object *object = NULL;
	CHECK(create_object(MSN_AMPHICAR, 0, 0, 65536, &object) == RN_INVALIDARG);
	CHECK(create_object(MSN_AMPHICAR, 0, 0, INT_MAX, &object) == RN_INVALIDARG);
	CHECK(create_object(MSN_AMPHICAR, 0, 0, USHRT_MAX / 4 + 1,
			    &object) == RN_INVALIDARG);
	CHECK(create_object(MSN_AMPHICAR, 0, 0, USHRT_MAX / 4, &object) == R_P);
	CHECK(get_base_node(object, MSN_NEOIOIC)->caller_count == (USHRT_MAX / 4) * 4);
	destroy_object(object);
	CHECK(object == NULL);
	/* Sufficient, naturally aligned caller-owned storage, filled with garbage. */
	unsigned char *external = malloc(65536);
	CHECK(external != NULL);
	memset(external, 0xa5, 65536);
	CHECK(host_create_object(MSN_AMPHICAR, 8, 8, 1, NULL, external) == R_P);
	object = (struct object *)external;
	for (unsigned short i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;
		CHECK(node->ref_count == 0);
		for (unsigned short j = 0; j < node->caller_count; j++)
			CHECK(node->callers[j].cr == 0 && node->callers[j].mr == 0);
	}
	CHECK(get_iq(object)->front == NULL && get_oq(object)->rear == NULL);
	CHECK(vo_open(object, &context) == R_P);
	CHECK(vo_close(object, &context) == RP_RCZERO);
	CHECK(host_destroy_object(object) == TRUE);
	/* Force the host's OID-wrap cleanup, including caller-owned memory. */
	gs_tad.oid = (OID_ & ~MSKSO) | MSKSO;
	CHECK(host_create_object(MSN_AMPHICAR, 8, 8, 1, NULL, external) == RN_LACKOID);
	CHECK(get_iq(object)->data == NULL && get_oq(object)->data == NULL);
	CHECK(create_object(MSN_AMPHICAR, 8, 8, 1, &object) == RN_LACKOID);
	CHECK(object == NULL && gs_tad.neo == 0);
	gs_tad.oid = OID_;
	free(external);
}

static void check_idle(struct object *object, unsigned int references)
{
	for (unsigned short i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;

		CHECK(node->nodes == object);
		CHECK(node->ref_count == references);
		CHECK((uintptr_t)node->callers % _Alignof(struct vr) == 0);
		CHECK((uintptr_t)node->members % _Alignof(struct object) == 0);
		for (unsigned short j = 0; j < node->caller_count; j++)
			CHECK(node->callers[j].cr == 0 && node->callers[j].mr == 0);
	}
}

static void test_allocation_failures(void)
{
	const unsigned short models[] = { MSN_CAR, MSN_SHIP, MSN_AMPHICAR };
	unsigned char *external = malloc(65536);

	CHECK(external != NULL);
	for (unsigned int i = 0; i < sizeof(models) / sizeof(*models); i++) {
		for (unsigned int mode = 0; mode < 2; mode++) {
			/* Dynamic storage, input queue, output queue; external
			 * storage only needs the two queue allocations. */
			for (int failure = 1; failure <= (mode ? 2 : 3); failure++) {
				struct object *object = NULL;
				uint64_t saved_oid = gs_tad.oid;

				memset(external, 0xa5, 65536);
				fail_alloc = failure;
				CHECK(host_create_object(models[i], 8, 8, 1,
							 mode ? NULL : &object,
							 mode ? external : NULL) == R_N);
				CHECK(fail_alloc == 0);
				CHECK(object == NULL && gs_tad.neo == 0);
				CHECK(gs_tad.oid == saved_oid);
				CHECK(live_allocations == 0);
				CHECK(create_object(models[i], 8, 8, 1, &object) == R_P);
				CHECK(object->oid == saved_oid);
				CHECK(vo_open(object, &context) == R_P);
				check_idle(object, 1);
				CHECK(vo_close(object, &context) == RP_RCZERO);
				destroy_object(object);
				CHECK(object == NULL && live_allocations == 0);
			}
		}
	}
	free(external);
}

static void test_host_reuse(void)
{
	struct object *objects[13] = { 0 };
	struct object *replacement = NULL;
	uint64_t first_oid;
	unsigned int live;

	/* Exercise both initial recycle-table allocation and table growth. */
	for (unsigned int i = 0; i < 13; i++)
		CHECK(create_object(MSN_CAR, 8, 8, 1, &objects[i]) == R_P);
	first_oid = objects[0]->oid;
	live = live_allocations;
	fail_alloc = 1;
	destroy_object(objects[0]);
	CHECK(objects[0] != NULL && gs_tad.neo == 13);
	CHECK(live_allocations == live);
	CHECK(get_iq(objects[0])->capacity == 8);
	CHECK(vo_open(objects[0], &context) == R_P);
	CHECK(vo_close(objects[0], &context) == RP_RCZERO);
	for (unsigned int i = 0; i < 10; i++) {
		destroy_object(objects[i]);
		CHECK(objects[i] == NULL);
	}
	CHECK(gs_tad.lad == 10 && gs_tad.nwe == 10);
	live = live_allocations;
	fail_alloc = 1;
	destroy_object(objects[10]);
	CHECK(objects[10] != NULL && gs_tad.neo == 3);
	CHECK(live_allocations == live && gs_tad.nwe == 10);
	destroy_object(objects[10]);
	CHECK(objects[10] == NULL && gs_tad.lad == 20);
	CHECK(create_object(MSN_CAR, 0, 0, 1, &replacement) == R_P);
	CHECK(replacement->oid == first_oid);
	destroy_object(replacement);
	for (unsigned int i = 11; i < 13; i++) {
		destroy_object(objects[i]);
		CHECK(objects[i] == NULL);
	}
	CHECK(gs_tad.neo == 0 && gs_tad.pad == NULL && gs_tad.oid == OID_);
	CHECK(live_allocations == 0);
}

static void test_diamond(void)
{
	struct object *object = NULL;
	struct object *car, *ship, *vehicle, *root;
	struct em_neoioic *members;

	CHECK(create_object(MSN_AMPHICAR, 8, 8, 1, &object) == R_P);
	car = get_base_node(object, MSN_CAR);
	ship = get_base_node(object, MSN_SHIP);
	vehicle = get_base_node(object, MSN_VEHICLE);
	root = get_base_node(object, MSN_NEOIOIC);
	members = root->members;
	CHECK(object->nodes == object && object->node_count == 5);
	CHECK(get_base_node(car, MSN_VEHICLE) == vehicle);
	CHECK(get_base_node(ship, MSN_VEHICLE) == vehicle);
	CHECK(get_base_node(car, MSN_NEOIOIC) == root);
	CHECK(get_base_node(ship, MSN_NEOIOIC) == root);
	CHECK(get_base_node(object, MSN_AMPHICAR) == NULL);
	CHECK(get_base_members(object, USHRT_MAX) == NULL);
	CHECK(get_iq(object) == &members->iq && get_oq(object) == &members->oq);
	check_idle(object, 0);
	for (unsigned int i = 1; i <= 3; i++) {
		CHECK(vo_open(object, &context) == R_P);
		check_idle(object, i);
	}
	CHECK(vo_interact0(object, SET_UNSHARED, &context) == R_P);
	CHECK(vo_open(object, &context) == RN_UNSHARED);
	check_idle(object, 3);
	CHECK(vo_interact0(object, SET_SHARED, &context) == R_P);
	for (unsigned int i = 3; i > 0; i--) {
		CHECK(RP(vo_close(object, &context)));
		check_idle(object, i - 1);
	}
	CHECK(vo_close(object, &context) == RO_NOPEN);
	check_idle(object, 0);
	destroy_object(object);
	CHECK(object == NULL);
}

int main(void)
{
	track_allocations = 1;
	test_queue();
	test_root();
	test_close();
	test_open_cleanup();
	test_construction();
	test_allocation_failures();
	test_host_reuse();
	test_diamond();
	test_access_context();
	CHECK(live_allocations == 0);
	track_allocations = 0;
	puts("regression checks passed");
	return 0;
}
