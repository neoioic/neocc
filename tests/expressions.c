#include "neocc/expressions.h"
#include "host/host.h"
#include "check.h"
#include <string.h>

static struct neocc_expressions *borrow(struct object *object)
{
	unsigned char output[sizeof(struct neocc_expressions *) + 1];
	struct neocc_expressions *state;
	unsigned int quantity = 123;

	CHECK(RP(vo_interact1(object, NEOCC_AC1_EXPRESSIONS_BORROW,
		output + 1, sizeof(state), &quantity, &neocc_host_caller)));
	CHECK(quantity == sizeof(state));
	memcpy(&state, output + 1, sizeof(state));
	return state;
}

static void clean_access(struct object *object)
{
	for (unsigned int i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
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
	struct neocc_expressions *state, *second;
	struct neocc_symbol symbol = { 0 };
	unsigned int quantity;
	int (*saved_close)(struct object *, const struct vr *);

	CHECK(RP(create_object(NEOCC_MSN_EXPRESSIONS, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_EXPRESSIONS, 0, 0, 1, &other)));
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	state = borrow(object);
	second = borrow(other);
	CHECK(state != second && state->top == state->values);
	CHECK((uintptr_t)state % _Alignof(struct neocc_expressions) == 0);
	CHECK((uintptr_t)&state->top->c % _Alignof(union neocc_value) == 0);
	/* Include both the empty-stack sentinel and the last usable entry. */
	for (int i = 0; i <= NEOCC_EXPRESSION_CAPACITY; i++) {
		state->values[i].c.ld = i + 0.25L;
		state->values[i].sym = &symbol;
	}
	state->top += NEOCC_EXPRESSION_CAPACITY;
	state->int_type.ref = state->char_type.ref = &symbol;
	state->char_pointer_type.ref = state->func_old_type.ref = &symbol;
	CHECK(second->top == second->values && !second->top->c.i);
	CHECK(!second->func_old_type.ref);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(borrow(object) == state &&
		state->top->c.ld == NEOCC_EXPRESSION_CAPACITY + 0.25L);
	quantity = 123;
	CHECK(vo_interact1(object, NEOCC_AC1_EXPRESSIONS_BORROW,
		(unsigned char *)&second, sizeof(second) - 1, &quantity,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(!quantity);
	CHECK(RP(vo_interact0(object, NEOCC_AC0_EXPRESSIONS_RESET,
		&neocc_host_caller)));
	CHECK(borrow(object) == state && state->top == state->values);
	for (int i = 0; i <= NEOCC_EXPRESSION_CAPACITY; i++)
		CHECK(!state->values[i].sym && !state->values[i].c.i);
	CHECK(!state->int_type.ref && !state->char_type.ref);
	CHECK(!state->char_pointer_type.ref && !state->func_old_type.ref);
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	state->top->c.ld = 1.25L;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(state->top->c.ld == 1.25L && !host_destroy_object(object));
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	quantity = 123;
	CHECK(vo_interact1(object, NEOCC_AC1_EXPRESSIONS_BORROW,
		(unsigned char *)&state, sizeof(state), &quantity,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(!quantity);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	state = borrow(object);
	CHECK(state->top == state->values && !state->top->c.i);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
	puts("expression storage isolation, alignment and lifecycle passed");
	return 0;
}
