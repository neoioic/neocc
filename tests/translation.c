#include "neocc/translation.h"
#include "host/host.h"
#include "check.h"
#include <string.h>

/* Linker storage is deliberately independent of the workspace owner. */
struct Section { int marker; };

static void clean_access(struct object *object)
{
	for (unsigned int i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;

		for (unsigned int j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static int borrow(struct object *object, struct neocc_translation **state,
	unsigned int capacity)
{
	unsigned char bytes[sizeof(*state) + 1];
	unsigned int quantity = 123;
	int result = vo_interact1(object, NEOCC_AC1_TRANSLATION_BORROW,
		bytes + 1, capacity, &quantity, &neocc_host_caller);

	CHECK(quantity == (RP(result) ? sizeof(*state) : 0));
	if (RP(result))
		memcpy(state, bytes + 1, sizeof(*state));
	clean_access(object);
	return result;
}

static void empty(const struct neocc_translation *state)
{
	CHECK(!state->anonymous_symbol && !state->instruction);
	CHECK(!state->suppression && !state->debug_modes && !state->section);
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
	struct neocc_translation *state, *second, *again;
	struct Section section = { 73 };
	int (*saved_close)(struct object *, const struct vr *);

	CHECK(RP(create_object(NEOCC_MSN_TRANSLATION, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_TRANSLATION, 0, 0, 1, &other)));
	CHECK(borrow(object, &state, sizeof(state)) == RN_INVALIDARG);
	CHECK(vo_interact0(object, NEOCC_AC0_TRANSLATION_RESET,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	CHECK(borrow(object, &state, sizeof(state) - 1) == RN_INVALIDARG);
	CHECK(RP(borrow(object, &state, sizeof(state))));
	CHECK(RP(borrow(other, &second, sizeof(second))));
	CHECK(state != second);
	empty(state);
	empty(second);
	*state = (struct neocc_translation){ 17, 8192, 0x20000001, 3, &section };
	empty(second);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(state->anonymous_symbol == 17 && state->instruction == 8192);
	CHECK(state->suppression == 0x20000001 && state->debug_modes == 3);
	CHECK(state->section == &section);
	CHECK(RP(vo_interact0(object, NEOCC_AC0_TRANSLATION_RESET,
		&neocc_host_caller)));
	CHECK(RP(borrow(object, &again, sizeof(again))) && again == state);
	empty(state);
	CHECK(section.marker == 73);
	state->instruction = 512;
	state->section = &section;
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(!host_destroy_object(object));
	CHECK(state->instruction == 512 && state->section == &section);
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	empty(state);
	CHECK(section.marker == 73);
	CHECK(borrow(object, &again, sizeof(again)) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(borrow(object, &again, sizeof(again))));
	empty(again);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
	puts("translation workspace, section borrowing and lifecycle passed");
	return 0;
}
