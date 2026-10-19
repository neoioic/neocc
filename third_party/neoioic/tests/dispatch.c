#include "object.h"
#include "check.h"

static int leaf_result;
static unsigned int calls;
static const struct vr context = { 1, 1 };

static int leaf_input(struct object *self, unsigned char *input_stream,
		      unsigned int quantity, const struct vr *caller_context)
{
	calls++;
	return leaf_result;
}

static int relay_input(struct object *self, unsigned char *input_stream,
		       unsigned int quantity, const struct vr *caller_context)
{
	_SBO_OBJECT_INPUT;
}

static int leaf_output(struct object *self, unsigned char *output_stream,
		       unsigned int capacity, unsigned int *output_quantity,
		       const struct vr *caller_context)
{
	calls++;
	return leaf_result;
}

static int relay_output(struct object *self, unsigned char *output_stream,
			unsigned int capacity, unsigned int *output_quantity,
			const struct vr *caller_context)
{
	_SBO_OBJECT_OUTPUT;
}

static int leaf_io_put(struct object *self, unsigned char *input_stream,
		       unsigned int quantity, unsigned char *output_stream, unsigned int capacity,
		       unsigned int *output_quantity, const struct vr *caller_context)
{
	calls++;
	return leaf_result;
}

static int relay_io_put(struct object *self, unsigned char *input_stream,
			unsigned int quantity, unsigned char *output_stream, unsigned int capacity,
			unsigned int *output_quantity, const struct vr *caller_context)
{
	_SBO_OBJECT_IOPUT;
}

static int leaf_interact0(struct object *self, unsigned int action,
			  const struct vr *caller_context)
{
	calls++;
	return leaf_result;
}

static int relay_interact0(struct object *self, unsigned int action,
			   const struct vr *caller_context)
{
	_SBO_OBJECT_INTERACT0;
}

static int leaf_interact1(struct object *self, unsigned int action,
			  unsigned char *output_stream, unsigned int capacity,
			  unsigned int *output_quantity, const struct vr *caller_context)
{
	calls++;
	return leaf_result;
}

static int relay_interact1(struct object *self, unsigned int action,
			   unsigned char *output_stream, unsigned int capacity,
			   unsigned int *output_quantity, const struct vr *caller_context)
{
	_SBO_OBJECT_INTERACT1;
}

static int leaf_interact2(struct object *self, unsigned int action,
			  unsigned char *input_stream, unsigned int quantity,
			  const struct vr *caller_context)
{
	calls++;
	return leaf_result;
}

static int relay_interact2(struct object *self, unsigned int action,
			   unsigned char *input_stream, unsigned int quantity,
			   const struct vr *caller_context)
{
	_SBO_OBJECT_INTERACT2;
}

static int leaf_interact3(struct object *self, unsigned int action,
			  unsigned char *input_stream, unsigned int quantity,
			  unsigned char *output_stream, unsigned int capacity,
			  unsigned int *output_quantity, const struct vr *caller_context)
{
	calls++;
	return leaf_result;
}

static int relay_interact3(struct object *self, unsigned int action,
			   unsigned char *input_stream, unsigned int quantity,
			   unsigned char *output_stream, unsigned int capacity,
			   unsigned int *output_quantity, const struct vr *caller_context)
{
	_SBO_OBJECT_INTERACT3;
}

static int invoke(struct object *object, unsigned int interface)
{
	unsigned char bytes[8] = { 0 };
	unsigned int got = 99;
	int result;

	switch (interface) {
	case 0:
		result = vo_input(object, bytes, 1, &context);
		return result;
	case 1:
		result = vo_output(object, bytes, sizeof(bytes), &got, &context);
		CHECK(got == 0);
		return result;
	case 2:
		result = vo_io_put(object, bytes, 1, bytes, sizeof(bytes), &got, &context);
		CHECK(got == 0);
		return result;
	case 3:
		result = vo_interact0(object, AC0_, &context);
		return result;
	case 4:
		result = vo_interact1(object, AC1_, bytes, sizeof(bytes), &got, &context);
		CHECK(got == 0);
		return result;
	case 5:
		result = vo_interact2(object, AC2_, bytes, 1, &context);
		return result;
	case 6:
		result = vo_interact3(object, AC3_, bytes, 1, bytes, sizeof(bytes), &got,
				      &context);
		CHECK(got == 0);
		return result;
	default:
		CHECK(0);
		return R_N;
	}
}

int main(void)
{
	const int results[] = { R_N, R_P, RO_SBOOIX, RO__SBOOIX, RO_VAINLY };
	struct vr slots[3] = { 0 };
	struct object a = { .callers = slots + 1, .caller_count = 1 };
	struct object b = { .callers = slots + 2, .caller_count = 1 };
	struct object *bases[] = { &a, &b };
	struct object parent = { .callers = slots, .caller_count = 1,
		       .direct_bases = bases, .bases = bases, .base_count = 2
	};

	a.input = b.input = leaf_input;
	parent.input = relay_input;
	a.output = b.output = leaf_output;
	parent.output = relay_output;
	a.io_put = b.io_put = leaf_io_put;
	parent.io_put = relay_io_put;
	a.interact0 = b.interact0 = leaf_interact0;
	parent.interact0 = relay_interact0;
	a.interact1 = b.interact1 = leaf_interact1;
	parent.interact1 = relay_interact1;
	a.interact2 = b.interact2 = leaf_interact2;
	parent.interact2 = relay_interact2;
	a.interact3 = b.interact3 = leaf_interact3;
	parent.interact3 = relay_interact3;
	for (unsigned int interface = 0; interface < 7; interface++) {
		for (unsigned int count = 0; count <= 2; count++) {
			parent.direct_base_count = count;
			for (unsigned int r = 0; r < sizeof(results) / sizeof(*results); r++) {
				leaf_result = results[r];
				calls = 0;
				int expected = count == 1 ? leaf_result : RO__SBOOIX;

				CHECK(invoke(&parent, interface) == expected);
				CHECK(calls == count);
				for (unsigned int i = 0; i < 3; i++)
					CHECK(slots[i].cr == 0 && slots[i].mr == 0);
			}
		}
		/* Saturated parent rejects entry and preserves the other visitor. */
		slots[0] = (struct vr){ 9, 9 };
		calls = 0;
		CHECK(invoke(&parent, interface) == R_N);
		CHECK(calls == 0 && slots[0].cr == 9 && slots[0].mr == 9);
		slots[0] = (struct vr){ 0 };
		/* A duplicate base is visited once in the same context. */
		bases[1] = &a;
		CHECK(invoke(&parent, interface) == RO__SBOOIX);
		CHECK(calls == 1);
		bases[1] = &b;
	}
	puts("seven relay interfaces: return values, access cleanup and saturation passed");
	return 0;
}
