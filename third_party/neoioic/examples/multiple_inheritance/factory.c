

#include "factory.h"

/* OICC label */

/* <oicc> */
/* <ibn> neoioic </ibn> */
/* <crt> */
/* </crt> */
/* </oicc> */

/* Interface */

static int factory_open(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_OPEN_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

static int factory_input(struct object *self, unsigned char *input_stream,
			 unsigned int quantity, const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INPUT;
}

static int factory_output(struct object *self, unsigned char *output_stream,
			  unsigned int capacity, unsigned int *output_quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_OUTPUT;
}

static int factory_io_put(struct object *self, unsigned char *input_stream,
			  unsigned int quantity, unsigned char *output_stream,
			  unsigned int capacity, unsigned int *output_quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_IOPUT;
}

static int factory_interact0(struct object *self, unsigned int action,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT0;
}

static int factory_interact1(struct object *self, unsigned int action,
			     unsigned char *output_stream,
			     unsigned int capacity,
			     unsigned int *output_quantity,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT1;
}

static int factory_interact2(struct object *self, unsigned int action,
			     unsigned char *input_stream, unsigned int quantity,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT2;
}

static int factory_interact3(struct object *self, unsigned int action,
			     unsigned char *input_stream, unsigned int quantity,
			     unsigned char *output_stream,
			     unsigned int capacity,
			     unsigned int *output_quantity,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT3;
}

static int factory_close(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_CLOSE_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

/* TOG */

void tog_factory(struct object *object)
{
	object->open = factory_open;
	object->input = factory_input;
	object->output = factory_output;
	object->io_put = factory_io_put;
	object->interact0 = factory_interact0;
	object->interact1 = factory_interact1;
	object->interact2 = factory_interact2;
	object->interact3 = factory_interact3;
	object->close = factory_close;
}

/* IRF(s) */

/* Function(s) */
