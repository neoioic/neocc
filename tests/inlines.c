#include "neocc/tokens.h"
#include "neocc/symbols.h"
#include "host/host.h"
#include "objects/tokens.h"
#include "check.h"
#include <string.h>

#ifdef NEOCC_TEST_FAULT
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
static int fail_next;
static size_t live;

void *__wrap_malloc(size_t size)
{
	void *pointer = fail_next ? NULL : __real_malloc(size);

	fail_next = 0;
	if (pointer)
		live++;
	return pointer;
}

void *__wrap_calloc(size_t count, size_t size)
{
	void *pointer = __real_calloc(count, size);

	if (pointer)
		live++;
	return pointer;
}

void *__wrap_realloc(void *old, size_t size)
{
	void *pointer = __real_realloc(old, size);

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

static int save(struct object *object, struct neocc_inline_save *request,
	struct neocc_inline **entry, unsigned int length, unsigned int capacity)
{
	unsigned char input[sizeof(*request) + 1], output[sizeof(*entry) + 1];
	unsigned int quantity = 123;
	int result;

	memcpy(input + 1, request, sizeof(*request));
	result = vo_interact3(object, NEOCC_AC3_TOKENS_SAVE_INLINE,
		input + 1, length, output + 1, capacity, &quantity,
		&neocc_host_caller);
	CHECK(quantity == (RP(result) ? sizeof(*entry) : 0));
	if (RP(result))
		memcpy(entry, output + 1, sizeof(*entry));
	clean_access(object);
	return result;
}

static struct neocc_inline *first(struct object *object)
{
	struct neocc_inline *entry;
	unsigned int quantity;

	CHECK(RP(vo_interact1(object, NEOCC_AC1_TOKENS_FIRST_INLINE,
		(unsigned char *)&entry, sizeof(entry), &quantity,
		&neocc_host_caller)));
	CHECK(quantity == sizeof(entry));
	clean_access(object);
	return entry;
}

static int resize(struct object *object, void **pointer, size_t size)
{
	struct neocc_token_resize request = { *pointer, size };
	void *output;
	unsigned int quantity = 123;
	int result = vo_interact3(object, NEOCC_AC3_TOKENS_RESIZE,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)&output, sizeof(output), &quantity,
		&neocc_host_caller);

	if (RP(result)) {
		CHECK(quantity == sizeof(output));
		*pointer = output;
	} else {
		CHECK(!quantity);
	}
	clean_access(object);
	return result;
}

static void reset(struct object *object, unsigned int action)
{
	CHECK(RP(vo_interact0(object, action, &neocc_host_caller)));
	CHECK(!first(object));
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
#ifdef NEOCC_TEST_FAULT
	size_t initial_live = live;
#endif
	struct object *object = NULL, *other = NULL, *pp = NULL, *root;
	struct neocc_symbol symbol = { 0 };
	char filename[4097];
	struct neocc_inline_save request = { &symbol, filename };
	struct neocc_inline *entries[512], *entry, *tail = NULL;
	struct em_tokens *members;
	struct neocc_preprocessor *state;
	struct neocc_macro_pop pop;
	int (*saved_close)(struct object *, const struct vr *);
	unsigned int quantity;
	void *unrelated = NULL;

	memset(filename, 'a', sizeof(filename) - 1);
	filename[sizeof(filename) - 1] = 0;
	CHECK(RP(create_object(NEOCC_MSN_TOKENS, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_TOKENS, 0, 0, 1, &other)));
	CHECK(RP(create_object(NEOCC_MSN_PREPROCESSOR, 0, 0, 1, &pp)));
	CHECK(save(object, &request, &entry, sizeof(request), sizeof(entry)) ==
	      RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	CHECK(RP(vo_open(pp, &neocc_host_caller)));
	CHECK(RP(vo_interact1(pp, NEOCC_AC1_PREPROCESSOR_BORROW,
		(unsigned char *)&state, sizeof(state), &quantity,
		&neocc_host_caller)));
	members = object->members;
	CHECK(save(object, &request, &entry, sizeof(request) - 1,
		sizeof(entry)) == RN_INVALIDARG);
	CHECK(save(object, &request, &entry, sizeof(request),
		sizeof(entry) - 1) == RN_INVALIDARG);
	request.symbol = NULL;
	CHECK(save(object, &request, &entry, sizeof(request), sizeof(entry)) ==
	      RN_INVALIDARG);
	request.symbol = &symbol;
	CHECK(!first(object));
	CHECK(RP(resize(object, &unrelated, 32)));
	memset(unrelated, 73, 32);
	for (int i = 0; i < 512; i++) {
#ifdef NEOCC_TEST_FAULT
		fail_next = 1;
		CHECK(RN(save(object, &request, &entry,
			sizeof(request), sizeof(entry))));
		CHECK(!fail_next && members->inline_tail == tail);
#endif
		CHECK(RP(save(object, &request, &entries[i],
			sizeof(request), sizeof(entry))));
		entry = entries[i];
		CHECK(!entry->next && !entry->body.str && !entry->body.len);
		CHECK(entry->body.last_line_num == -1 && entry->symbol == &symbol);
		CHECK(!strcmp(entry->filename, filename));
		if (tail)
			CHECK(tail->next == entry);
		tail = entry;
		CHECK(first(object) == entries[0] && !first(other));
		void *data = NULL;
		CHECK(RP(resize(object, &data, 3 * sizeof(int))));
		entry->body.str = data;
		entry->body.allocated_len = entry->body.len = 3;
		entry->body.str[0] = i;
		entry->body.str[1] = -1;
		entry->body.str[2] = 0;
#ifdef NEOCC_TEST_FAULT
		/* Force a fresh arena block; failed growth preserves the body. */
		fail_next = 1;
		CHECK(RN(resize(object, &data, 1024 * 1024)));
		CHECK(!fail_next && data == entry->body.str);
		CHECK(entry->body.str[0] == i);
#endif
	}
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(first(object) == entries[0]);
	for (int i = 0; i < 512; i++) {
		entry = entries[i];
		struct neocc_macro_push push = { &entry->body, 0, 79 };

		CHECK(RP(vo_interact2(pp, NEOCC_AC2_PREPROCESSOR_PUSH,
			(unsigned char *)&push, sizeof(push), &neocc_host_caller)));
		CHECK(state->macro_stack == &entry->body);
		entry->symbol = NULL;
	}
	/* Unwind nested borrowed bodies before their owner releases them. */
	for (int i = 511; i >= 0; i--) {
		CHECK(state->macro_stack == &entries[i]->body);
		CHECK(RP(vo_interact1(pp, NEOCC_AC1_PREPROCESSOR_POP,
			(unsigned char *)&pop, sizeof(pop), &quantity,
			&neocc_host_caller)));
		CHECK(!pop.release && pop.saved_line == 79);
	}
	CHECK(!state->macro_stack && !state->macro_ptr);
	clean_access(pp);
	reset(object, NEOCC_AC0_TOKENS_RESET_INLINES);
	CHECK(!members->inline_tail && *(unsigned char *)unrelated == 73);
	CHECK(RP(resize(object, &unrelated, 0)));
	CHECK(RP(save(object, &request, &entry, sizeof(request), sizeof(entry))));
	reset(object, NEOCC_AC0_TOKENS_RESET);
	CHECK(RP(save(object, &request, &entry, sizeof(request), sizeof(entry))));
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(!host_destroy_object(object) && members->inline_head == entry);
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(!members->inline_head && !members->inline_tail);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(!first(object));
	CHECK(RP(save(object, &request, &entry, sizeof(request), sizeof(entry))));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	CHECK(RP(vo_close(pp, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	clean_access(pp);
	destroy_object(object);
	destroy_object(other);
	destroy_object(pp);
	CHECK(!object && !other && !pp);
#ifdef NEOCC_TEST_FAULT
	CHECK(live == initial_live);
#endif
	puts("deferred inline records, token ownership and cleanup passed");
	return 0;
}
