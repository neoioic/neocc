#include "neocc/temporaries.h"
#include "objects/temporaries.h"
#include "host/host.h"
#include "check.h"
#include <string.h>

#ifdef NEOCC_TEST_FAULT
void *__real_malloc(size_t);
void *__real_realloc(void *, size_t);
static int fail_node, fail_resize;

void *__wrap_malloc(size_t size)
{
	if (fail_node && size == sizeof(struct neocc_temporary)) {
		fail_node = 0;
		return NULL;
	}
	return __real_malloc(size);
}

void *__wrap_realloc(void *pointer, size_t size)
{
	if (fail_resize) {
		fail_resize = 0;
		return NULL;
	}
	return __real_realloc(pointer, size);
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

static int action(struct object *object, unsigned int code)
{
	int result = vo_interact0(object, code, &neocc_host_caller);

	clean_access(object);
	return result;
}

static int push(struct object *object, struct neocc_string **string,
	unsigned int capacity)
{
	unsigned char bytes[sizeof(*string) + 1];
	unsigned int quantity = 123;
	int result = vo_interact1(object, NEOCC_AC1_TEMPORARIES_PUSH,
		bytes + 1, capacity, &quantity, &neocc_host_caller);

	CHECK(quantity == (RP(result) ? sizeof(*string) : 0));
	if (RP(result))
		memcpy(string, bytes + 1, sizeof(*string));
	clean_access(object);
	return result;
}

static int pop(struct object *object, struct neocc_string *string,
	unsigned int quantity)
{
	unsigned char bytes[sizeof(string) + 1];
	int result;

	memcpy(bytes + 1, &string, sizeof(string));
	result = vo_interact2(object, NEOCC_AC2_TEMPORARIES_POP, bytes + 1,
		quantity, &neocc_host_caller);
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
	struct em_temporaries *members;
	struct neocc_string *outer, *strings[512], *foreign, *string;
	int (*saved_close)(struct object *, const struct vr *);
	char *data;

	CHECK(RP(create_object(NEOCC_MSN_TEMPORARIES, 0, 0, 1, &object)));
	CHECK(RP(create_object(NEOCC_MSN_TEMPORARIES, 0, 0, 1, &other)));
	members = object->members;
	CHECK(push(object, &outer, sizeof(outer)) == RN_INVALIDARG);
	CHECK(action(object, NEOCC_AC0_TEMPORARIES_RESET) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_open(other, &neocc_host_caller)));
	CHECK(push(object, &outer, sizeof(outer) - 1) == RN_INVALIDARG);
	CHECK(RP(push(other, &foreign, sizeof(foreign))));
	CHECK(RP(push(object, &outer, sizeof(outer))));
	CHECK(neocc_string_printf(outer, "%s:%d", "outer", 7) == 7);
	CHECK(!strcmp(outer->data, "outer:7") && outer->size == 7);
	CHECK(RP(action(object, NEOCC_AC0_TEMPORARIES_UNWIND)));
	CHECK(!strcmp(outer->data, "outer:7"));
	CHECK(action(object, NEOCC_AC0_TEMPORARIES_LEAVE) == RN_INVALIDARG);
	CHECK(RP(action(object, NEOCC_AC0_TEMPORARIES_ENTER)));
	CHECK(pop(object, &members->top->string, sizeof(string)) == RN_INVALIDARG);
	for (int i = 0; i < 512; i++) {
#ifdef NEOCC_TEST_FAULT
		struct neocc_temporary *top = members->top;

		fail_node = 1;
		CHECK(RN(push(object, &string, sizeof(string))) && !fail_node);
		CHECK(members->top == top);
		fail_node = 1;
		CHECK(RN(action(object, NEOCC_AC0_TEMPORARIES_ENTER)) && !fail_node);
		CHECK(members->top == top);
#endif
		CHECK(RP(push(object, &strings[i], sizeof(strings[i]))));
		CHECK(neocc_string_printf(strings[i], "%d", i) > 0);
	}
	CHECK(pop(object, foreign, sizeof(foreign)) == RN_INVALIDARG);
	CHECK(pop(object, strings[0], sizeof(string)) == RN_INVALIDARG);
	CHECK(pop(object, strings[511], sizeof(string) - 1) == RN_INVALIDARG);
	for (int i = 511; i >= 0; i--) {
		CHECK(atoi(strings[i]->data) == i);
		CHECK(RP(pop(object, strings[i], sizeof(string))));
	}
	CHECK(RP(push(object, &string, sizeof(string))));
	CHECK(neocc_string_printf(string, "%080d", 5) == 80);
	data = string->data;
#ifdef NEOCC_TEST_FAULT
	fail_resize = 1;
	CHECK(neocc_string_printf(string, "%08000d", 5) == -1);
	CHECK(!fail_resize && string->data == data && string->size == 80);
	CHECK(string->data[79] == '5');
#else
	CHECK(neocc_string_printf(string, "%08000d", 5) == 8000);
#endif
	CHECK(RP(action(object, NEOCC_AC0_TEMPORARIES_ENTER)));
	CHECK(RP(push(object, &strings[0], sizeof(string))));
	CHECK(RP(action(object, NEOCC_AC0_TEMPORARIES_UNWIND)));
	CHECK(members->top->boundary);
	CHECK(RP(action(object, NEOCC_AC0_TEMPORARIES_LEAVE)));
	CHECK(&members->top->string == string);
	CHECK(RP(action(object, NEOCC_AC0_TEMPORARIES_LEAVE)));
	CHECK(&members->top->string == outer);
	CHECK(!strcmp(outer->data, "outer:7"));
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(&members->top->string == outer);
	CHECK(RP(action(object, NEOCC_AC0_TEMPORARIES_RESET)));
	CHECK(!members->top);
	CHECK(RP(push(object, &string, sizeof(string))));
	CHECK(neocc_string_printf(string, "close") == 5);
	data = string->data;
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &neocc_host_caller) == RN_CLOSEPARTIAL);
	CHECK(!host_destroy_object(object) && members->top->string.data == data);
	root->close = saved_close;
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(!members->top);
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	CHECK(!members->top);
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	CHECK(RP(vo_close(other, &neocc_host_caller)));
	clean_access(object);
	clean_access(other);
	destroy_object(object);
	destroy_object(other);
	CHECK(!object && !other);
	puts("temporary ownership, boundaries and lifecycle passed");
	return 0;
}
