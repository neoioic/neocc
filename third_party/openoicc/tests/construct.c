/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include "alloc.h"
#include "openoicc/construct.h"

extern void tog_neoioic(struct object *);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void)
{
	const unsigned short base[] = { 1 };
	const unsigned short invalid[] = { 2 };
	const unsigned short self[] = { 0 };
	const struct openoicc_layout valid[] = {
		{ 2, tog_neoioic, 0, 1, 1, base, 1, base, 1 },
		{ MSN_NEOIOIC, tog_neoioic, NEOIOIC_ALIGNED_SIZE(struct em_neoioic),
		  _Alignof(struct em_neoioic), 2, NULL, 0, NULL, 0 }
	};
	struct openoicc_layout layout[2];
	struct object *object;
	uint64_t oid;
	union { max_align_t align; unsigned char bytes[4096]; } external;
	/* Every rejected layout must fail before allocation or external writes. */
	for (int variant = 0; variant < 22; variant++) {
		memcpy(layout, valid, sizeof(layout));
		switch (variant) {
		case 0: layout[0].model_id = 0; break;
		case 1: layout[0].model_id = MSN_NEOIOIC; break;
		case 2: layout[0].bind = NULL; break;
		case 3: layout[0].seats = 0; break;
		case 4: layout[0].seats = USHRT_MAX + (size_t)1; break;
		case 5: layout[0].direct = NULL; break;
		case 6: layout[0].bases = NULL; break;
		case 7: layout[0].direct_count = USHRT_MAX + (size_t)1; break;
		case 8: layout[0].base_count = USHRT_MAX + (size_t)1; break;
		case 9: layout[0].member_alignment = 0; break;
		case 10: layout[0].member_alignment = 3; break;
		case 11: layout[0].member_alignment = _Alignof(struct object) * 2; break;
		case 12: layout[0].direct = invalid; break;
		case 13: layout[0].bases = invalid; break;
		case 14: layout[0].direct = self; break;
		case 15: layout[0].bases = self; break;
		case 16: layout[1].member_size = sizeof(struct em_neoioic) - 1; break;
		case 17: layout[1].direct = self; layout[1].direct_count = 1; break;
		case 18: layout[1].bases = self; layout[1].base_count = 1; break;
		case 19: layout[0].member_size = SIZE_MAX; break;
		case 20: layout[0].member_size = SIZE_MAX / 2;
			layout[1].member_size = SIZE_MAX / 2; break;
		case 21: layout[1].model_id = 2; break;
		}
		for (int dynamic = 0; dynamic < 2; dynamic++) {
			oid = 100;
			test_calls = 0;
			object = (struct object *)(uintptr_t)1;
			memset(external.bytes, 0xa5, sizeof(external.bytes));
			CHECK(RN(openoicc_construct(layout, 2, &oid, 0, 0, 1,
				dynamic ? &object : NULL, dynamic ? NULL : external.bytes)));
			CHECK(oid == 100 && test_calls == 0 && !test_live);
			if (dynamic)
				CHECK(!object);
			for (size_t i = 0; i < sizeof(external.bytes); i++)
				CHECK(external.bytes[i] == 0xa5);
		}
	}
	oid = 100;
	CHECK(RN(openoicc_construct(NULL, 2, &oid, 0, 0, 1, &object, NULL)));
	CHECK(RN(openoicc_construct(valid, 0, &oid, 0, 0, 1, &object, NULL)));
	CHECK(RN(openoicc_construct(valid, USHRT_MAX + (size_t)1, &oid, 0, 0, 1, &object, NULL)));
	for (int capacity = -1; capacity <= 0; capacity++)
		CHECK(RN(openoicc_construct(valid, 2, &oid, 0, 0, capacity, &object, NULL)));
	CHECK(RN(openoicc_construct(valid, 2, &oid, 0, 0, INT_MAX, &object, NULL)));
	oid = 0;
	CHECK(RN(openoicc_construct(valid, 2, &oid, 0, 0, 1, &object, NULL)));
	/* Last valid contiguous block, then one past it. */
	oid = UINT16_MAX - 2;
	CHECK(RP(openoicc_construct(valid, 2, &oid, 0, 0, 1, &object, NULL)));
	CHECK(oid == UINT16_MAX && object->oid == UINT16_MAX - 2);
	free(object);
	CHECK(openoicc_construct(valid, 2, &oid, 0, 0, 1, &object, NULL) == RN_LACKOID);
	CHECK(!object && !test_live);
	/* Alignment padding and multiplication exact limits without allocation. */
	for (size_t alignment = 1; alignment <= _Alignof(max_align_t); alignment *= 2) {
		for (size_t start = 0; start < 2 * alignment; start++) {
			size_t size = start, offset;
			CHECK(openoicc_reserve(&size, 3, 7, alignment, &offset));
			CHECK(offset >= start && offset - start < alignment);
			CHECK(offset % alignment == 0 && size == offset + 21);
		}
	}
	size_t size = 0, offset;
	CHECK(openoicc_reserve(&size, 1, SIZE_MAX, 1, &offset));
	CHECK(size == SIZE_MAX && offset == 0);
	CHECK(!openoicc_reserve(&size, 1, 1, 1, &offset));
	CHECK(!openoicc_reserve(&size, 0, 0, 0, &offset));
	puts("constructor boundary checks passed");
	return 0;
}
