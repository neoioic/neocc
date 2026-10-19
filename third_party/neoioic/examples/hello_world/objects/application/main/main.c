

#include <stdio.h>
#include "main.h"

/* OICC label */

/* <oicc> */
/* <ibn> neoioic </ibn> */
/* <crt> */
/* </crt> */
/* </oicc> */

/* Interface */

static int main_open(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_OPEN_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

static int main_input(struct object *self, unsigned char *input_stream,
		      unsigned int quantity, const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INPUT;
}

static int main_output(struct object *self, unsigned char *output_stream,
		       unsigned int capacity, unsigned int *output_quantity,
		       const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_OUTPUT;
}

static int main_io_put(struct object *self, unsigned char *input_stream,
		       unsigned int quantity, unsigned char *output_stream,
		       unsigned int capacity, unsigned int *output_quantity,
		       const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_IOPUT;
}

static int main_interact0(struct object *self, unsigned int action,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT0;
}

static int main_interact1(struct object *self, unsigned int action,
			  unsigned char *output_stream, unsigned int capacity,
			  unsigned int *output_quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT1;
}

static int main_interact2(struct object *self, unsigned int action,
			  unsigned char *input_stream, unsigned int quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT2;
}

static int main_interact3(struct object *self, unsigned int action,
			  unsigned char *input_stream, unsigned int quantity,
			  unsigned char *output_stream, unsigned int capacity,
			  unsigned int *output_quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT3;
}

static int main_close(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_CLOSE_;

	/* 在下面完成接口自己的任务。 */

	return R_P;
}

/* TOG */

void tog_main(struct object *object)
{
	object->open = main_open;
	object->input = main_input;
	object->output = main_output;
	object->io_put = main_io_put;
	object->interact0 = main_interact0;
	object->interact1 = main_interact1;
	object->interact2 = main_interact2;
	object->interact3 = main_interact3;
	object->close = main_close;
}

/* main */

/* 主体函数。 */
int main(int argc, char **argv)
{
	printf("Hello world!\n");
	return 0;
}

/* IRF(s) */

/* Function(s) */
