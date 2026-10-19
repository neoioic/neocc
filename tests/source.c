#include "check.h"
#include "neocc/source.h"
#include "host/host.h"
#include "objects/source.h"
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <io.h>

static const struct vr caller = { 7, 9 };

static int partial_close(struct object *self, const struct vr *caller_context)
{
	(void)self;
	(void)caller_context;
	return RN_CLOSEPARTIAL;
}

static void check_access(struct object *object)
{
	unsigned int i, j;

	for (i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;
		for (j = 0; j < node->caller_count; j++)
			CHECK(!node->callers[j].cr && !node->callers[j].mr);
	}
}

static int push(struct object *object, struct neocc_source_request request,
	struct neocc_source_result *result)
{
	unsigned char input[sizeof(request) + 1];
	unsigned char output[sizeof(*result) + 1];
	unsigned int count = 123;
	int code;

	memcpy(input + 1, &request, sizeof(request));
	code = vo_interact3(object, NEOCC_AC3_SOURCE_PUSH, input + 1,
		sizeof(request), output + 1, sizeof(*result), &count, &caller);
	CHECK(count == (RP(code) ? sizeof(*result) : 0));
	if (RP(code))
		memcpy(result, output + 1, sizeof(*result));
	check_access(object);
	return code;
}

static int pop(struct object *object, struct neocc_source_result *result)
{
	unsigned char output[sizeof(*result) + 1];
	unsigned int count = 123;
	int code = vo_interact1(object, NEOCC_AC1_SOURCE_POP, output + 1,
		sizeof(*result), &count, &caller);

	CHECK(count == (RP(code) ? sizeof(*result) : 0));
	if (RP(code))
		memcpy(result, output + 1, sizeof(*result));
	check_access(object);
	return code;
}

int main(void)
{
	struct object *object = NULL, *other = NULL;
	struct object *root;
	int (*saved_close)(struct object *, const struct vr *);
	struct neocc_source_request request = { "a\\b.c", 0, -1, NULL, 42 };
	struct neocc_source_result result;
	struct neocc_source_buffer *first;
	unsigned int count;
	char name[1025];
	uint64_t oid;
	int fd, i;

	CHECK(create_object(0, 0, 0, 1, &object) == RN_INVALIDARG);
	CHECK(!object);
	CHECK(create_object(NEOCC_MSN_SOURCE, 0, 0, 0, &object) == RN_INVALIDARG);
	CHECK(create_object(NEOCC_MSN_SOURCE, 0, 0, INT_MAX, &object) == RN_INVALIDARG);
	CHECK(!object);
	CHECK(RP(create_object(NEOCC_MSN_SOURCE, 0, 0, 1, &object)));
	oid = object->oid;
	CHECK(push(object, request, &result) == RN_INVALIDARG);
	CHECK(pop(object, &result) == RN_INVALIDARG);
	CHECK(RP(vo_open(object, &caller)));
	CHECK(!host_destroy_object(object));
	CHECK(pop(object, &result) == RO_VAINLY);
	CHECK(RP(push(object, request, &result)));
	first = result.current;
	CHECK(!strcmp(first->filename, "a/b.c"));
	CHECK(first->buf_ptr == first->buffer && first->buf_end == first->buffer);
	CHECK(*first->buf_end == NEOCC_SOURCE_EOB && first->line_num == 1);
	CHECK(RP(vo_interact0(object, NEOCC_AC0_SOURCE_SAVE_NAME, &caller)));
	strcpy(first->filename, "logical.c");
	CHECK(!strcmp(first->true_filename, "a/b.c"));
	CHECK(RP(vo_interact0(object, NEOCC_AC0_SOURCE_SAVE_NAME, &caller)));
	CHECK(!strcmp(first->true_filename, "a/b.c"));
	request.length = 37;
	request.tok_flags = 73;
	CHECK(RP(push(object, request, &result)));
	CHECK(result.current->buf_end == result.current->buffer + 37);
	CHECK(*result.current->buf_end == NEOCC_SOURCE_EOB);
	CHECK(result.current->prev == first);
	CHECK(RP(pop(object, &result)));
	CHECK(result.current == first && result.tok_flags == 73);
	CHECK(RP(pop(object, &result)));
	CHECK(!result.current && result.tok_flags == 42);

	request.length = SIZE_MAX;
	CHECK(push(object, request, &result) == RN_INVALIDARG);
	request.length = (size_t)INT_MAX + 1;
	CHECK(push(object, request, &result) == RN_INVALIDARG);
	request.length = 0;
	request.fd = -2;
	CHECK(push(object, request, &result) == RN_INVALIDARG);
	request.fd = -1;
	request.name = NULL;
	CHECK(push(object, request, &result) == RN_INVALIDARG);
	memset(name, 'x', sizeof(name));
	name[1024] = 0;
	request.name = name;
	CHECK(push(object, request, &result) == RN_INVALIDARG);
	name[1023] = 0;
	CHECK(RP(push(object, request, &result)));
	CHECK(RP(pop(object, &result)));
	request.name = "tests/source.c";
	count = 12;
	CHECK(vo_interact3(object, NEOCC_AC3_SOURCE_PUSH,
		(unsigned char *)&request, sizeof(request) - 1,
		(unsigned char *)&result, sizeof(result), &count, &caller)
		== RN_INVALIDARG);
	CHECK(!count);
	CHECK(vo_interact3(object, NEOCC_AC3_SOURCE_PUSH,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)&result, sizeof(result) - 1, &count,
		&caller) == RN_INVALIDARG);
	CHECK(!count);
	CHECK(pop(object, &result) == RO_VAINLY);

	fd = open(request.name, O_RDONLY | O_BINARY);
	CHECK(fd > 0);
	request.fd = fd;
	CHECK(RP(push(object, request, &result)));
	result.current->line_num = 19;
	CHECK(RP(pop(object, &result)));
	CHECK(result.lines == 18);
	errno = 0;
	CHECK(_get_osfhandle(fd) == -1 && errno == EBADF);
	/* Stdin is borrowed even if a source is popped or closed. */
	request.fd = 0;
	CHECK(RP(push(object, request, &result)));
	CHECK(RP(pop(object, &result)));
	CHECK(_get_osfhandle(0) != -1);

	request.fd = -1;
	CHECK(RP(create_object(NEOCC_MSN_SOURCE, 0, 0, 1, &other)));
	CHECK(other->oid != object->oid);
	CHECK(RP(vo_open(other, &caller)));
	for (i = 0; i < 257; i++)
		CHECK(RP(push(object, request, &result)));
	CHECK(pop(other, &result) == RO_VAINLY);
	CHECK(RP(vo_open(object, &caller)));
	CHECK(RP(vo_close(object, &caller)));
	CHECK(object->ref_count == 1);
	CHECK(RP(pop(object, &result)));
	root = get_base_node(object, MSN_NEOIOIC);
	saved_close = root->close;
	root->close = partial_close;
	CHECK(vo_close(object, &caller) == RN_CLOSEPARTIAL);
	CHECK(((struct em_source *)object->members)->top);
	CHECK(!host_destroy_object(object));
	root->close = saved_close;
	CHECK(RP(vo_close(object, &caller)));
	CHECK(!((struct em_source *)object->members)->top);
	CHECK(!object->ref_count);
	CHECK(vo_close(object, &caller) == RO_NOPEN);
	check_access(object);
	CHECK(host_destroy_object(object));
	free(object);
	CHECK(RP(vo_close(other, &caller)));
	destroy_object(other);
	CHECK(!other);
	for (i = 0; i < 1000; i++) {
		CHECK(RP(create_object(NEOCC_MSN_SOURCE, 0, 0, 1, &object)));
		CHECK(object->oid == oid);
		destroy_object(object);
		CHECK(!object);
	}
	puts("source ownership, boundaries, access cleanup and isolation passed");
	return 0;
}
