#include "neocc/preprocessor.h"
#include "neocc/source.h"
#include "host/host.h"
#include "check.h"
#include <string.h>

static struct neocc_preprocessor *borrow(struct object *object)
{
	unsigned char output[sizeof(struct neocc_preprocessor *) + 1];
	struct neocc_preprocessor *state;
	unsigned int quantity = 123;

	CHECK(RP(vo_interact1(object, NEOCC_AC1_PREPROCESSOR_BORROW,
		output + 1, sizeof(state), &quantity, &neocc_host_caller)));
	CHECK(quantity == sizeof(state));
	memcpy(&state, output + 1, sizeof(state));
	return state;
}

static int push(struct object *object, struct neocc_token_string *stream,
	int alloc, int line)
{
	struct neocc_macro_push request = { stream, alloc, line };
	unsigned char input[sizeof(request) + 1];

	memcpy(input + 1, &request, sizeof(request));
	return vo_interact2(object, NEOCC_AC2_PREPROCESSOR_PUSH,
		input + 1, sizeof(request), &neocc_host_caller);
}

static int pop(struct object *object, struct neocc_macro_pop *result,
	unsigned int capacity)
{
	unsigned char output[sizeof(*result) + 1];
	unsigned int quantity = 123;
	int code = vo_interact1(object, NEOCC_AC1_PREPROCESSOR_POP,
		output + 1, capacity, &quantity, &neocc_host_caller);

	CHECK(quantity == (RP(code) ? sizeof(*result) : 0));
	if (RP(code))
		memcpy(result, output + 1, sizeof(*result));
	return code;
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
	struct neocc_preprocessor *state, *second;
	struct neocc_token_string streams[257] = { 0 };
	struct neocc_source_buffer source = { 0 };
	struct neocc_macro_pop result;
	int data[] = { 1, 2, 0 };
	int (*saved_close)(struct object *, const struct vr *);
	unsigned int quantity;
	char *old;

	CHECK(RP(create_object(NEOCC_MSN_PREPROCESSOR, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_PREPROCESSOR, 0, 0, 1, &other)));
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	state = borrow(object);
	second = borrow(other);
	CHECK(state != second);
	CHECK((uintptr_t)state % _Alignof(struct neocc_preprocessor) == 0);
	CHECK((uintptr_t)&state->value % _Alignof(union neocc_value) == 0);
	CHECK(!state->token && !state->value.i && !state->source);
	state->token = 123;
	state->value.ld = 0x1.000000000000001p0L;
	state->source = &source;
	state->debug_indent = 7;
	CHECK(!second->token && !second->value.i && !second->source);
	CHECK(!second->debug_indent);
	CHECK(!state->character[0]);
	CHECK(state->character[' ' + 1] == NEOCC_CHAR_SPACE);
	CHECK(!state->character['\n' + 1]);
	CHECK(state->character['_' + 1] == NEOCC_CHAR_ID);
	CHECK(state->character[256] == NEOCC_CHAR_ID);
	CHECK(state->character['9' + 1] == NEOCC_CHAR_NUMBER);
	CHECK(state->expansion.last_line_num == -1);
	CHECK(state->unget.last_line_num == -1);
	state->counter = 73;
	state->character['$' + 1] = NEOCC_CHAR_ID;
	CHECK(!second->counter && !second->character['$' + 1]);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(state == borrow(object) && state->counter == 73);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(state->counter == 73);
	CHECK(state->token == 123 && state->source == &source);
	CHECK(state->value.ld == 0x1.000000000000001p0L);
	CHECK(state->debug_indent == 7);

	CHECK(pop(object, &result, sizeof(result)) == RO_VAINLY);
	CHECK(push(object, NULL, 0, 1) == RN_INVALIDARG);
	CHECK(push(object, streams, 3, 1) == RN_INVALIDARG);
	quantity = 123;
	CHECK(vo_interact1(object, NEOCC_AC1_PREPROCESSOR_BORROW,
		(unsigned char *)&second, sizeof(second) - 1, &quantity,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(!quantity);
	CHECK(vo_interact2(object, NEOCC_AC2_PREPROCESSOR_PUSH,
		(unsigned char *)streams, sizeof(struct neocc_macro_push) - 1,
		&neocc_host_caller) == RN_INVALIDARG);
	for (int i = 0; i < 257; i++) {
		streams[i].str = data;
		streams[i].len = 2;
		CHECK(RP(push(object, &streams[i], i % 3, i + 11)));
		state->macro_ptr = data + 1;
	}
	CHECK(pop(other, &result, sizeof(result)) == RO_VAINLY);
	CHECK(pop(object, &result, sizeof(result) - 1) == RN_INVALIDARG);
	CHECK(state->macro_stack == &streams[256]);
	for (int i = 256; i >= 0; i--) {
		CHECK(RP(pop(object, &result, sizeof(result))));
		CHECK(result.saved_line == i + 11);
		CHECK(result.release == (i % 3 ? &streams[i] : NULL));
		if (!(i % 3))
			CHECK(!streams[i].len && !streams[i].need_spc);
		if (i % 3 == 2)
			CHECK(!streams[i].str);
		CHECK(state->macro_ptr == (i ? data + 1 : NULL));
	}
	CHECK(!state->macro_stack);
	CHECK(neocc_string_reserve(&state->parsed, 17));
	memcpy(state->parsed.data, "retained", 9);
	state->parsed.size = 9;
	old = state->parsed.data;
	CHECK(!neocc_string_reserve(&state->parsed, -1));
	CHECK(state->parsed.data == old && state->parsed.size == 9);
	CHECK(!strcmp(state->parsed.data, "retained"));
	CHECK(neocc_string_reserve(&state->spelling, 3000));
	CHECK(neocc_string_reserve(&state->predefs, 100));
	CHECK(RP(vo_interact0(object, NEOCC_AC0_PREPROCESSOR_RESET,
		&neocc_host_caller)));
	CHECK(!state->parsed.data && !state->spelling.data && !state->predefs.data);
	CHECK(!state->counter && !state->character['$' + 1]);
	CHECK(borrow(object) == state);
	CHECK(!state->token && !state->value.i && !state->source);
	CHECK(!state->debug_indent);

	CHECK(neocc_string_reserve(&state->predefs, 10));
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(state->predefs.data);
	CHECK(!host_destroy_object(object));
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	/* Final close ends the workspace borrow, including its aligned storage. */
	quantity = 123;
	CHECK(vo_interact1(object, NEOCC_AC1_PREPROCESSOR_BORROW,
		(unsigned char *)&state, sizeof(state), &quantity,
		&neocc_host_caller) == RN_INVALIDARG);
	CHECK(!quantity);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	state = borrow(object);
	CHECK(!state->counter && !state->source && !state->token);
	CHECK(!state->value.i && !state->debug_indent);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
	puts("preprocessor nesting, isolation, boundaries and cleanup passed");
	return 0;
}
