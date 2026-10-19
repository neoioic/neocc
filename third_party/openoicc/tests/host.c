/* SPDX-License-Identifier: MIT */
#include "host/host.h"
#include "objects/project.h"
#include "neoioic.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
	struct object *a, *b;
	struct vr caller = { OID_OPENOICC, OID_OPENOICC };
	uint64_t first = 0;
	unsigned int capacity, quantity;
	union { max_align_t alignment; unsigned char bytes[16384]; } external;
	for (int i = 0; i < 10; i++) {
		CHECK(RP(create_object(MSN_PROJECT, 0, 0, 1, &a)));
		if (!i)
			first = a->oid;
		CHECK(a->oid == first);
		CHECK(RP(create_object(MSN_PROJECT, 0, 0, 1, &b)));
		CHECK(a->oid != b->oid && a->nodes[1].oid != b->oid);
		CHECK(RP(vo_open(a, &caller)));
		CHECK(!host_destroy_object(a));
		CHECK(RP(vo_interact0(a, SET_UNSHARED, &caller)));
		CHECK(RN(vo_open(a, &caller)));
		CHECK(RP(vo_interact0(a, SET_SHARED, &caller)));
		CHECK(RP(vo_interact1(a, GET_IQCTY, (unsigned char *)&capacity,
				      sizeof(capacity), &quantity, &caller)));
		CHECK(capacity == 0 && quantity == sizeof(capacity));
		CHECK(RN(vo_interact2(a, AC2_PROJECT_RUN, (unsigned char *)&capacity,
				      sizeof(capacity), &caller)));
		CHECK(RP(vo_close(a, &caller)));
		CHECK(host_destroy_object(a) && host_destroy_object(b));
		free(a);
		free(b);
	}
	memset(external.bytes, 0xa5, sizeof(external.bytes));
	CHECK(RP(host_create_object(MSN_PROJECT, 0, 0, 1, NULL, external.bytes)));
	a = (struct object *)external.bytes;
	CHECK(a->oid == first && !((struct em_project *)a->members)->nodes);
	CHECK(RP(vo_open(a, &caller)) && RP(vo_close(a, &caller)));
	CHECK(host_destroy_object(a));
	CHECK(RN(host_create_object(0, 0, 0, 1, &a, NULL)));
	CHECK(RN(host_create_object(MSN_PROJECT, 0, 0, 1, NULL, NULL)));
	puts("host checks passed");
	return 0;
}
