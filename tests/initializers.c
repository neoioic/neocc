#include "neocc/initializers.h"
#include "host/host.h"
#include "check.h"
#include <string.h>

static void clean_access(struct object *object)
{
	for (unsigned int i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static int borrow(struct object *object, struct neocc_initializers **state,
	unsigned int capacity)
{
	unsigned char bytes[sizeof(*state) + 1];
	unsigned int quantity = 123;
	int result = vo_interact1(object, NEOCC_AC1_INITIALIZERS_BORROW,
		bytes + 1, capacity, &quantity, &neocc_host_caller);

	CHECK(quantity == (RP(result) ? sizeof(*state) : 0));
	if (RP(result))
		memcpy(state, bytes + 1, sizeof(*state));
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
	struct neocc_initializers *state, *second, *again;
	int (*saved_close)(struct object *, const struct vr *);
	char *data;

	CHECK(RP(create_object(NEOCC_MSN_INITIALIZERS, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_INITIALIZERS, 0, 0, 1, &other)));
	CHECK(borrow(object, &state, sizeof(state)) == RN_INVALIDARG);
	CHECK(vo_interact0(object, NEOCC_AC0_INITIALIZERS_RESET,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	CHECK(borrow(object, &state, sizeof(state) - 1) == RN_INVALIDARG);
	CHECK(RP(borrow(object, &state, sizeof(state))));
	CHECK(RP(borrow(other, &second, sizeof(second))));
	CHECK(state != second && !state->string.data && !second->string.data);
	CHECK(!state->global_expression && !second->global_expression);
	CHECK(neocc_string_reserve(&state->string, 8));
	memcpy(state->string.data, "abc\0def", 8);
	state->string.size = 8;
	state->global_expression = 1;
	CHECK(neocc_string_reserve(&state->string, 8192));
	CHECK(!memcmp(state->string.data, "abc\0def", 8));
	data = state->string.data;
	CHECK(!neocc_string_reserve(&state->string, -1));
	CHECK(state->string.data == data && state->string.size == 8);
	CHECK(!second->global_expression && !second->string.data);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(state->global_expression && state->string.data == data);
	CHECK(RP(vo_interact0(object, NEOCC_AC0_INITIALIZERS_RESET,
		&neocc_host_caller)));
	CHECK(RP(borrow(object, &again, sizeof(again))) && again == state);
	CHECK(!state->global_expression && !state->string.data);
	CHECK(!state->string.size && !state->string.size_allocated);
	CHECK(neocc_string_reserve(&state->string, 32));
	state->global_expression = 1;
	data = state->string.data;
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(!host_destroy_object(object));
	/* Only retry close after a partial close; no business operations. */
	CHECK(state->string.data == data && state->global_expression);
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(!state->string.data && !state->global_expression);
	CHECK(borrow(object, &again, sizeof(again)) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(borrow(object, &again, sizeof(again))));
	CHECK(!again->string.data && !again->global_expression);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
	puts("initializer workspace and lifecycle passed");
	return 0;
}
