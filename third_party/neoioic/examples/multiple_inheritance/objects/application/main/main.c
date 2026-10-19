

#include <stdio.h>
#include <stdlib.h>
#include "main.h"

extern struct object *g_p_car;
extern struct object *g_p_ship;
extern struct object *g_p_amphicar;

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
	int ir;
	int fr;
	float sr32_val;
	unsigned int qty;
	struct vr caller = { OID_MAIN, OID_FACTORY };

	/* 动态创建car对象。 */
	fr = create_object(MSN_CAR, 0, 0, 1, &g_p_car);
	if (RN(fr)) {
		printf("Can't create car object.\n");
		return -1;
	}

	/* 动态创建ship对象。 */
	fr = create_object(MSN_SHIP, 0, 0, 1, &g_p_ship);
	if (RN(fr)) {
		printf("Can't create ship object.\n");
		destroy_object(g_p_car);
		return -1;
	}

	/* 动态创建amphicar对象。 */
	fr = create_object(MSN_AMPHICAR, 0, 0, 1, &g_p_amphicar);
	if (RN(fr)) {
		printf("Can't create amphicar object.\n");
		destroy_object(g_p_car);
		destroy_object(g_p_ship);
		return -1;
	}

	/* 打开car对象。 */
	ir = vo_open(g_p_car, &caller);
	if (RN(ir)) {
		printf("Can't open car object.\n");
		destroy_object(g_p_car);
		destroy_object(g_p_ship);
		destroy_object(g_p_amphicar);
		return -1;
	}

	/* 打开ship对象。 */
	ir = vo_open(g_p_ship, &caller);
	if (RN(ir)) {
		printf("Can't open ship object.\n");
		vo_close(g_p_car, &caller);
		destroy_object(g_p_car);
		destroy_object(g_p_ship);
		return -1;
	}

	/* 打开amphicar对象。 */
	ir = vo_open(g_p_amphicar, &caller);
	if (RN(ir)) {
		printf("Can't open amphicar object.\n");
		vo_close(g_p_car, &caller);
		destroy_object(g_p_car);
		vo_close(g_p_ship, &caller);
		destroy_object(g_p_ship);
		destroy_object(g_p_amphicar);
		return -1;
	}

	do {
		/* 输出car的重量。 */
		ir = vo_interact1(g_p_car, GET_WEIGHT,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("car's weight is %f ton.\n", sr32_val);
		} else {
			printf("Can't get car's weight.\n");
			break;
		}

		/* 输出car的最大载重量。 */
		ir = vo_interact1(g_p_car, GET_MAXLOAD,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("car's max-load is %f ton.\n", sr32_val);
		} else {
			printf("Can't get car's max-load.\n");
			break;
		}

		/* 输出car的最大速度。 */
		ir = vo_interact1(g_p_car, GET_MAXVELOCITY,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("car's max-velocity is %f km/h.\n", sr32_val);
		} else {
			printf("Can't get car's max-velocity.\n");
			break;
		}

		putchar('\n');

		/* 输出ship的重量。 */
		ir = vo_interact1(g_p_ship, GET_WEIGHT,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("ship's weight is %f ton.\n", sr32_val);
		} else {
			printf("Can't get ship's weight.\n");
			break;
		}

		/* 输出ship的最大载重量。 */
		ir = vo_interact1(g_p_ship, GET_MAXLOAD,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("ship's max-load is %f ton.\n", sr32_val);
		} else {
			printf("Can't get ship's max-load.\n");
			break;
		}

		/* 输出ship的最大速度。 */
		ir = vo_interact1(g_p_ship, GET_MAXVELOCITY,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("ship's max-velocity is %f km/h.\n", sr32_val);
		} else {
			printf("Can't get ship's max-velocity.\n");
			break;
		}

		putchar('\n');

		/* 输出amphicar的重量。 */
		ir = vo_interact1(g_p_amphicar, GET_WEIGHT,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("amphicar's weight is %f ton.\n", sr32_val);
		} else {
			printf("Can't get amphicar's weight.\n");
			break;
		}

		/* 输出amphicar的陆上最大载重量。 */
		ir = vo_interact1(g_p_amphicar, GET_TMAXLOAD,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("amphicar's terrestrial max-load is %f ton.\n",
			       sr32_val);
		} else {
			printf("Can't get amphicar's terrestrial max-load.\n");
			break;
		}

		/* 输出amphicar的水上最大载重量。 */
		ir = vo_interact1(g_p_amphicar, GET_AMAXLOAD,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("amphicar's aquatic max-load is %f ton.\n",
			       sr32_val);
		} else {
			printf("Can't get amphicar's aquatic max-load.\n");
			break;
		}

		/* 输出amphicar的陆上最大速度。 */
		ir = vo_interact1(g_p_amphicar, GET_TMAXVELOCITY,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("amphicar's terrestrial max-velocity is %f km/h.\n",
			       sr32_val);
		} else {
			printf("Can't get amphicar's terrestrial max-velocity.\n");
			break;
		}

		/* 输出amphicar的水上最大速度。 */
		ir = vo_interact1(g_p_amphicar, GET_AMAXVELOCITY,
				  (unsigned char *)&sr32_val, sizeof(sr32_val),
				  &qty, &caller);
		if (RP(ir)) {
			printf("amphicar's aquatic max-velocity is %f km/h.\n",
			       sr32_val);
		} else {
			printf("Can't get amphicar's aquatic max-velocity.\n");
			break;
		}
	} while (0);

	ir = vo_close(g_p_car, &caller);
	if (RP_RCZERO == ir) {
		destroy_object(g_p_car); /* 销毁动态创建的car对象。 */
	} else {
		/* 因为仅一次打开car对象，如果引用计数不是0，就是错误的。 */
		printf("Reference count is not zero, after close car.\n");
		return -1;
	}

	ir = vo_close(g_p_ship, &caller);
	if (RP_RCZERO == ir) {
		destroy_object(g_p_ship); /* 销毁动态创建的ship对象。 */
	} else {
		printf("Reference count is not zero, after close ship.\n");
		return -1;
	}

	ir = vo_close(g_p_amphicar, &caller);
	if (RP_RCZERO == ir) {
		destroy_object(g_p_amphicar); /* 销毁动态创建的amphicar对象。 */
	} else {
		printf("Reference count is not zero, after close amphicar.\n");
		return -1;
	}

	return 0;
}

/* IRF(s) */

/* Function(s) */
