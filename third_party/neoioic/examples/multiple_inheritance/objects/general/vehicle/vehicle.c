

#include "vehicle.h"

/* OICC label */

/* <oicc> */
/* <ibn> neoioic </ibn> */
/* <crt> */
/* </crt> */
/* </oicc> */

/* Interface */

static int vehicle_open(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_OPEN_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

static int vehicle_input(struct object *self, unsigned char *input_stream,
			 unsigned int quantity, const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INPUT;
}

static int vehicle_output(struct object *self, unsigned char *output_stream,
			  unsigned int capacity, unsigned int *output_quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_OUTPUT;
}

static int vehicle_io_put(struct object *self, unsigned char *input_stream,
			  unsigned int quantity, unsigned char *output_stream,
			  unsigned int capacity, unsigned int *output_quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_IOPUT;
}

static int vehicle_interact0(struct object *self, unsigned int action,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT0;
}

static int vehicle_interact1(struct object *self, unsigned int action,
			     unsigned char *output_stream,
			     unsigned int capacity,
			     unsigned int *output_quantity,
			     const struct vr *caller_context)
{
	struct em_vehicle *members = (struct em_vehicle *)self->members;

	switch (action) {
	case GET_WEIGHT:
		assert(capacity >= sizeof(float)); /* 判断输出流容量够不够。 */
		*(float *)output_stream = members->weight; /* 输出重量值。 */
		*output_quantity = sizeof(float); /* 输出的字节数。 */
		return R_P;
	default:
		break;
	}

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT1;
}

static int vehicle_interact2(struct object *self, unsigned int action,
			     unsigned char *input_stream, unsigned int quantity,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT2;
}

static int vehicle_interact3(struct object *self, unsigned int action,
			     unsigned char *input_stream, unsigned int quantity,
			     unsigned char *output_stream,
			     unsigned int capacity,
			     unsigned int *output_quantity,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT3;
}

static int vehicle_close(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_CLOSE_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

/* TOG */

void tog_vehicle(struct object *object)
{
	object->open = vehicle_open;
	object->input = vehicle_input;
	object->output = vehicle_output;
	object->io_put = vehicle_io_put;
	object->interact0 = vehicle_interact0;
	object->interact1 = vehicle_interact1;
	object->interact2 = vehicle_interact2;
	object->interact3 = vehicle_interact3;
	object->close = vehicle_close;
}

/* IRF(s) */

/* Function(s) */
