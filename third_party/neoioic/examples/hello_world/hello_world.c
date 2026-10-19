

#include "hello_world.h"

/* OICC label */

/* <oicc> */
/* <ibn> neoioic </ibn> */
/* <crt> */
/* </crt> */
/* </oicc> */

/* Interface */

static int hello_world_open(struct object *self,
			    const struct vr *caller_context)
{
	OBS_OBJECT_OPEN_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

static int hello_world_input(struct object *self, unsigned char *input_stream,
			     unsigned int quantity,
			     const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INPUT;
}

static int hello_world_output(struct object *self, unsigned char *output_stream,
			      unsigned int capacity,
			      unsigned int *output_quantity,
			      const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_OUTPUT;
}

static int hello_world_io_put(struct object *self, unsigned char *input_stream,
			      unsigned int quantity,
			      unsigned char *output_stream,
			      unsigned int capacity,
			      unsigned int *output_quantity,
			      const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_IOPUT;
}

static int hello_world_interact0(struct object *self, unsigned int action,
				 const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT0;
}

static int hello_world_interact1(struct object *self, unsigned int action,
				 unsigned char *output_stream,
				 unsigned int capacity,
				 unsigned int *output_quantity,
				 const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT1;
}

static int hello_world_interact2(struct object *self, unsigned int action,
				 unsigned char *input_stream,
				 unsigned int quantity,
				 const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT2;
}

static int hello_world_interact3(struct object *self, unsigned int action,
				 unsigned char *input_stream,
				 unsigned int quantity,
				 unsigned char *output_stream,
				 unsigned int capacity,
				 unsigned int *output_quantity,
				 const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT3;
}

static int hello_world_close(struct object *self,
			     const struct vr *caller_context)
{
	OBS_OBJECT_CLOSE_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

/* TOG */

void tog_hello_world(struct object *object)
{
	object->open = hello_world_open;
	object->input = hello_world_input;
	object->output = hello_world_output;
	object->io_put = hello_world_io_put;
	object->interact0 = hello_world_interact0;
	object->interact1 = hello_world_interact1;
	object->interact2 = hello_world_interact2;
	object->interact3 = hello_world_interact3;
	object->close = hello_world_close;
}

/* IRF(s) */

/* Function(s) */
