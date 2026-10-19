#include "check.h"
#include "neocc/source.h"
#include "host/host.h"
#include "objects/source.h"
#include <string.h>
#include <io.h>
#include <fcntl.h>

void *__real_calloc(size_t, size_t);
void *__real_malloc(size_t);
void __real_free(void *);
static int fail_at, calls;
static int tracking;
static void *allocations[128];
static size_t live;

static void *track(void *pointer)
{
	if (pointer) {
		CHECK(live < sizeof(allocations) / sizeof(*allocations));
		allocations[live++] = pointer;
	}
	return pointer;
}

void *__wrap_calloc(size_t count, size_t size)
{
	if (!tracking)
		return __real_calloc(count, size);
	return ++calls == fail_at ? NULL : track(__real_calloc(count, size));
}

void *__wrap_malloc(size_t size)
{
	if (!tracking)
		return __real_malloc(size);
	return ++calls == fail_at ? NULL : track(__real_malloc(size));
}

void __wrap_free(void *pointer)
{
	size_t i;
	for (i = 0; i < live; i++) {
		if (allocations[i] != pointer)
			continue;
		allocations[i] = allocations[--live];
		break;
	}
	__real_free(pointer);
}

int main(void)
{
	struct object *object;
	struct neocc_source_result output;
	struct neocc_source_request request = { "fault.c", 0, -1, NULL, 0 };
	struct em_source *members;
	uint64_t oid = 0;
	unsigned int quantity;
	size_t before;
	int i, fd;

	tracking = 1;
	for (i = 1; i <= 3; i++) {
		calls = 0;
		fail_at = i;
		object = (struct object *)1;
		CHECK(RN(create_object(NEOCC_MSN_SOURCE, 17, 19, 1, &object)));
		CHECK(!object && live == 0);
		fail_at = 0;
		CHECK(RP(create_object(NEOCC_MSN_SOURCE, 0, 0, 1, &object)));
		if (oid)
			CHECK(object->oid == oid);
		oid = object->oid;
		destroy_object(object);
		CHECK(!live);
	}
	CHECK(RP(create_object(NEOCC_MSN_SOURCE, 0, 0, 1, &object)));
	CHECK(RP(vo_open(object, &neocc_host_caller)));
	members = object->members;
	before = live;
	fd = open("tests/source_fault.c", O_RDONLY | O_BINARY);
	CHECK(fd > 0);
	request.fd = fd;
	fail_at = calls + 1;
	CHECK(RN(vo_interact3(object, NEOCC_AC3_SOURCE_PUSH,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)&output, sizeof(output), &quantity,
		&neocc_host_caller)));
	CHECK(!quantity && !members->top && live == before);
	CHECK(_get_osfhandle(fd) != -1);
	close(fd);
	request.fd = -1;
	fail_at = 0;
	CHECK(RP(vo_interact3(object, NEOCC_AC3_SOURCE_PUSH,
		(unsigned char *)&request, sizeof(request),
		(unsigned char *)&output, sizeof(output), &quantity,
		&neocc_host_caller)));
	before = live;
	fail_at = calls + 1;
	CHECK(RN(vo_interact0(object, NEOCC_AC0_SOURCE_SAVE_NAME,
		&neocc_host_caller)));
	CHECK(live == before && members->top->true_filename ==
		members->top->filename);
	fail_at = 0;
	CHECK(RP(vo_interact0(object, NEOCC_AC0_SOURCE_SAVE_NAME,
		&neocc_host_caller)));
	CHECK(RP(vo_close(object, &neocc_host_caller)));
	destroy_object(object);
	CHECK(!object && !live);
	puts("source constructor, buffer and filename allocation failures passed");
	return 0;
}
