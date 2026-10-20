#include "objects/functions.h"
#include "host/host.h"
#include "check.h"
#include <limits.h>
#include <string.h>

static void clean_access(struct object *object)
{
	for (unsigned int i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static int borrow(struct object *object, struct neocc_function **state,
	unsigned int capacity)
{
	unsigned char output[sizeof(*state) + 1];
	unsigned int quantity = 123;
	int code = vo_interact1(object, NEOCC_AC1_FUNCTIONS_BORROW,
		output + 1, capacity, &quantity, &neocc_host_caller);

	CHECK(quantity == (RP(code) ? sizeof(*state) : 0));
	if (RP(code))
		memcpy(state, output + 1, sizeof(*state));
	clean_access(object);
	return code;
}

static int begin(struct object *object, struct neocc_function *initial,
	unsigned int quantity)
{
	unsigned char input[sizeof(*initial) + 1];
	int code;

	memcpy(input + 1, initial, sizeof(*initial));
	code = vo_interact2(object, NEOCC_AC2_FUNCTIONS_BEGIN,
		input + 1, quantity, &neocc_host_caller);
	clean_access(object);
	return code;
}

static int reserve(struct object *object, int size, int alignment,
	unsigned int used, unsigned int quantity, unsigned int capacity,
	struct neocc_function_temp_result *result)
{
	struct neocc_function_temp_request request = { size, alignment, used };
	unsigned char input[sizeof(request) + 1];
	unsigned char output[sizeof(*result) + 1];
	unsigned int actual = 123;
	int code;

	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact3(object, NEOCC_AC3_FUNCTIONS_TEMP,
		input + 1, quantity, output + 1, capacity, &actual,
		&neocc_host_caller);
	CHECK(actual == (RP(code) ? sizeof(*result) : 0));
	if (RP(code))
		memcpy(result, output + 1, sizeof(*result));
	clean_access(object);
	return code;
}

static struct neocc_function_temp_result temp(struct object *object,
	int size, int alignment, unsigned int used)
{
	struct neocc_function_temp_result result;

	CHECK(RP(reserve(object, size, alignment, used,
		sizeof(struct neocc_function_temp_request), sizeof(result),
		&result)));
	return result;
}

static void clean_state(struct object *object)
{
	struct em_functions *members = object->members;
	struct neocc_function *state = &members->state;

	CHECK(!state->type.ref && !state->type.t);
	CHECK(state->name && !*state->name && state->start == -1);
	CHECK(!state->variadic && !state->old_style);
	CHECK(!state->stack_offset && !state->struct_return && !state->return_jumps);
	CHECK(!members->temp_count);
	for (int i = 0; i < NEOCC_FUNCTION_TEMP_CAPACITY; i++)
		CHECK(!members->temps[i].location && !members->temps[i].size &&
		      !members->temps[i].alignment);
	clean_access(object);
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
	struct neocc_function *state, *second, *borrowed;
	struct neocc_symbol symbol = { 0 };
	struct neocc_function initial = {
		.type = { .t = 17, .ref = &symbol },
		.name = "variadic_function",
		.start = 32,
		.variadic = 1,
		.old_style = 1,
		.struct_return = 16,
		.return_jumps = 99,
	};
	struct neocc_function_temp_result result;
	struct em_functions *members;
	int (*saved_close)(struct object *, const struct vr *);
	unsigned int used = 0;

	CHECK(RP(create_object(NEOCC_MSN_FUNCTIONS, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_FUNCTIONS, 0, 0, 1, &other)));
	CHECK(borrow(object, &state, sizeof(state)) == RN_INVALIDARG);
	CHECK(begin(object, &initial, sizeof(initial)) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	CHECK(borrow(object, &state, sizeof(state) - 1) == RN_INVALIDARG);
	CHECK(RP(borrow(object, &state, sizeof(state))));
	CHECK(RP(borrow(other, &second, sizeof(second))));
	CHECK(state != second);
	clean_state(object);
	clean_state(other);
	CHECK(begin(object, &initial, sizeof(initial) - 1) == RN_INVALIDARG);
	CHECK(RP(begin(object, &initial, sizeof(initial))));
	CHECK(state->type.ref == &symbol && state->type.t == 17);
	CHECK(state->name == initial.name && state->start == 32);
	CHECK(state->variadic && state->old_style && state->struct_return == 16);
	CHECK(state->return_jumps == 99);
	members = object->members;
	for (int i = 0; i < NEOCC_FUNCTION_TEMP_CAPACITY; i++) {
		result = temp(object, 8, 8, used);
		CHECK(result.slot == i && result.offset == -8 * (i + 1));
		used |= 1U << i;
	}
	result = temp(object, 8, 8, used);
	CHECK(result.slot == -1 && result.offset == -72);
	CHECK(members->temp_count == NEOCC_FUNCTION_TEMP_CAPACITY);
	/* First fit reuses a slot without rolling back intervening locals. */
	state->stack_offset = -128;
	result = temp(object, 4, 4, used & ~(1U << 3));
	CHECK(result.slot == 3 && result.offset == -32);
	CHECK(state->stack_offset == -128);
	result = temp(object, 16, 16, 0);
	CHECK(result.slot == -1 && result.offset == -144);
	CHECK(begin(object, &initial, sizeof(initial) - 2) == RN_INVALIDARG);
	initial.name = NULL;
	CHECK(begin(object, &initial, sizeof(initial)) == RN_INVALIDARG);
	initial.name = "next_function";
	initial.start = -1;
	CHECK(begin(object, &initial, sizeof(initial)) == RN_INVALIDARG);
	CHECK(state->stack_offset == -144 && members->temp_count == 8);
	initial.start = 64;
	CHECK(RP(begin(object, &initial, sizeof(initial))));
	CHECK(!members->temp_count && !state->stack_offset);
	result = temp(object, 4, 16, 0);
	CHECK(result.slot == 0 && result.offset == -16);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(state->stack_offset == -16 && members->temp_count == 1);
	for (int i = 0; i < 5; i++) {
		int size = i == 0 ? -1 : 32;
		int align = i == 1 ? 0 : i == 2 ? 3 : 16;
		unsigned int quantity = sizeof(struct neocc_function_temp_request);
		unsigned int capacity = sizeof(result);

		if (i == 3)
			quantity--;
		if (i == 4)
			capacity--;
		CHECK(reserve(object, size, align, 0, quantity, capacity,
			&result) == RN_INVALIDARG);
		CHECK(state->stack_offset == -16 && members->temp_count == 1);
	}
	state->stack_offset = INT_MIN;
	CHECK(reserve(object, 1, 1, 1,
		sizeof(struct neocc_function_temp_request), sizeof(result),
		&result) == RN_INVALIDARG);
	CHECK(state->stack_offset == INT_MIN && members->temp_count == 1);
	clean_state(other);
	CHECK(RP(vo_interact0(object, NEOCC_AC0_FUNCTIONS_RESET,
		&neocc_host_caller)));
	CHECK(RP(borrow(object, &borrowed, sizeof(borrowed))));
	CHECK(borrowed == state);
	clean_state(object);
	result = temp(object, 0, 1, 0);
	CHECK(result.slot == 0 && !result.offset);
	CHECK(RP(begin(object, &initial, sizeof(initial))));
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(state->type.ref == &symbol && state->start == 64);
	CHECK(!host_destroy_object(object));
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	clean_state(object);
	CHECK(borrow(object, &borrowed, sizeof(borrowed)) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(borrow(object, &borrowed, sizeof(borrowed))));
	clean_state(object);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
	puts("function workspace, temporary stack slots and lifecycle passed");
	return 0;
}
