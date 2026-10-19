/* SPDX-License-Identifier: MIT */
#include "ho.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
	fprintf(stderr, "line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

int main(void)
{
	struct object *a = NULL, *b = NULL, *extra = NULL;
	union { max_align_t alignment; unsigned char bytes[8192]; } storage;
	struct object *external = (struct object *)storage.bytes;
	struct vr caller = { OID_HO, OID_HO };
	uint64_t released;
	memset(storage.bytes, 0xa5, sizeof(storage.bytes));
	CHECK(RP(host_create_object(MSN_CAR, 8, 8, 1, &a, NULL)));
	CHECK(RP(host_create_object(MSN_CAR, 0, 0, 1, &b, NULL)));
	CHECK(RP(host_create_object(MSN_CAR, 8, 8, 1, NULL, storage.bytes)));
	CHECK(external->ref_count == 0 && external->node_count == 2);
	CHECK(host_create_object(MSN_CAR, 0, 0, 1, &extra, NULL) == RN_LACKOID);
	CHECK(!extra);
	CHECK(RP(vo_open(a, &caller)));
	CHECK(!host_destroy_object(a));
	CHECK(RP(vo_interact0(a, ASK_SHARED, &caller)));
	CHECK(RP(vo_close(a, &caller)));
	released = a->oid;
	CHECK(host_destroy_object(a));
	free(a);
	CHECK(RP(host_create_object(MSN_CAR, 0, 0, 1, &a, NULL)));
	CHECK(a->oid == released);
	CHECK(host_destroy_object(a));
	free(a);
	CHECK(host_destroy_object(b));
	free(b);
	CHECK(host_destroy_object(external));
	struct em_neoioic *root = get_base_members(external, MSN_NEOIOIC);
	CHECK(root && !root->iq.data && !root->oq.data);
	CHECK(RP(host_create_object(MSN_CAR, 0, 0, 1, NULL, storage.bytes)));
	CHECK(host_destroy_object(external));
	return 0;
}
