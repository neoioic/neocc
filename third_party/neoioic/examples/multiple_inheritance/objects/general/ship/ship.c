

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "../vehicle/vehicle.h"
#include "ship.h"

void tog_ship(struct object *object);

/* OICC label */

/* <oicc> */
/* <ibn> vehicle </ibn> */
/* <crt> */
int crt_ship(uint64_t *object_id, unsigned int input_capacity,
	     unsigned int output_capacity, int caller_capacity,
	     struct object **object_out, unsigned char *exotic_memory)
{
	extern void init_direct_bases(struct object * node,
				      unsigned short *base_msns,
				      unsigned short nib);
	extern void init_base_nodes(struct object * node,
				    unsigned short *base_msns,
				    unsigned short nbn);
	extern void tog_vehicle(struct object * object);
	extern void tog_neoioic(struct object * object);

	size_t ux_nd = 3;		    /* ND数量。 */
	size_t ux_bn = 3;		    /* 各ND的BN数量之和。 */
	size_t ux_ib = 2;		    /* 各ND的IBN数量之和。 */
	if (caller_capacity < 1 || (size_t)caller_capacity > USHRT_MAX / 3)
		return RN_INVALIDARG;
	size_t ux_cs = (size_t)caller_capacity * 6; /* 各ND的最少CS数量之和。 */
	size_t ux_em =
		NEOIOIC_ALIGNED_SIZE(struct em_ship) +
		NEOIOIC_ALIGNED_SIZE(struct em_vehicle) +
		NEOIOIC_ALIGNED_SIZE(struct em_neoioic); /* 各ND的void尺寸之和。 */

	size_t sum = ux_nd * sizeof(struct object)     /* for AND. */
		     + ux_bn * sizeof(struct object *) /* for "bases". */
		     + ux_ib * sizeof(struct object *) /* for "direct_bases". */
		     + ux_cs * sizeof(struct vr) +
		     ux_em; /* for "callers" and "members". */

	unsigned char *p_ic = NULL;

	if (NULL == exotic_memory) {
		*object_out = NULL;
		p_ic = (unsigned char *)calloc(sum, 1);
	} else {
		p_ic = exotic_memory;
		memset(p_ic, 0, sum);
	}

	if (p_ic != NULL) {
		struct object *node;
		unsigned short base_msns[2];
		unsigned char *p = p_ic + sizeof(struct object) * ux_nd;

		/* 确定AND各元素  ** */

		/* ship * */
		node = (struct object *)p_ic;
		tog_ship(node);
		node->model_id = MSN_SHIP;
		node->oid = (*object_id)++;
		node->nodes = (struct object *)p_ic;
		node->node_count = ux_nd;

		/* vehicle * */
		tog_vehicle(++node);
		node->model_id = MSN_VEHICLE;
		node->oid = (*object_id)++;
		node->nodes = (struct object *)p_ic;
		node->node_count = ux_nd;

		/* NEOioic * */
		tog_neoioic(++node);
		node->model_id = MSN_NEOIOIC;
		node->oid = (*object_id)++;
		node->nodes = (struct object *)p_ic;
		node->node_count = ux_nd;

		/* 初始化各ND  ** */

		/* ship * */
		node = (struct object *)p_ic;

		/* - AIB - */
		node->direct_bases = (struct object **)p;
		base_msns[0] = MSN_VEHICLE;
		init_direct_bases(node, base_msns, 1);
		p += sizeof(struct object *) * 1;

		/* - ABN - */
		node->bases = (struct object **)p;
		base_msns[0] = MSN_VEHICLE;
		base_msns[1] = MSN_NEOIOIC;
		init_base_nodes(node, base_msns, 2);
		p += sizeof(struct object *) * 2;

		/* - ACS - */
		node->callers = (struct vr *)p;
		node->caller_count = caller_capacity * 1;
		p += sizeof(struct vr) * node->caller_count;

		/* - void - */
		node->members = (void *)p;
		p += NEOIOIC_ALIGNED_SIZE(struct em_ship);

		/* vehicle * */
		++node;

		/* - AIB - */
		node->direct_bases = (struct object **)p;
		base_msns[0] = MSN_NEOIOIC;
		init_direct_bases(node, base_msns, 1);
		p += sizeof(struct object *) * 1;

		/* - ABN - */
		node->bases = (struct object **)p;
		base_msns[0] = MSN_NEOIOIC;
		init_base_nodes(node, base_msns, 1);
		p += sizeof(struct object *) * 1;

		/* - ACS - */
		node->callers = (struct vr *)p;
		node->caller_count = caller_capacity * 2;
		p += sizeof(struct vr) * node->caller_count;

		/* - void - */
		node->members = (void *)p;
		p += NEOIOIC_ALIGNED_SIZE(struct em_vehicle);

		/* NEOioic * */
		++node;

		/* - ACS - */
		node->callers = (struct vr *)p;
		node->caller_count = caller_capacity * 3;
		p += sizeof(struct vr) * node->caller_count;

		/* - void - */
		node->members = (void *)p;
		if (input_capacity > 0) {
			/* - iq - */
			((struct em_neoioic *)(node->members))->iq.data =
				(unsigned char *)calloc(input_capacity, 1);
			if (NULL ==
			    ((struct em_neoioic *)(node->members))->iq.data) {
				/* 分配iq的容量失败！ */
				if (NULL == exotic_memory)
					free(p_ic);
				return R_N;
			}
			((struct em_neoioic *)(node->members))->iq.front =
				((struct em_neoioic *)(node->members))->iq.rear =
					NULL;
			((struct em_neoioic *)(node->members))->iq.capacity =
				input_capacity;
			((struct em_neoioic *)(node->members))->iq.quantity =
				((struct em_neoioic *)(node->members))->iq.lost =
					0;
		}

		if (output_capacity > 0) {
			/* - oq - */
			((struct em_neoioic *)(node->members))->oq.data =
				(unsigned char *)calloc(output_capacity, 1);
			if (NULL ==
			    ((struct em_neoioic *)(node->members))->oq.data) {
				/* 分配oq的容量失败！ */
				if (((struct em_neoioic *)(node->members))
					    ->iq.data != NULL)
					free(((struct em_neoioic
						       *)(node->members))
						     ->iq.data);
				if (NULL == exotic_memory)
					free(p_ic);
				return R_N;
			}
			((struct em_neoioic *)(node->members))->oq.front =
				((struct em_neoioic *)(node->members))->oq.rear =
					NULL;
			((struct em_neoioic *)(node->members))->oq.capacity =
				output_capacity;
			((struct em_neoioic *)(node->members))->oq.quantity =
				((struct em_neoioic *)(node->members))->oq.lost =
					0;
		}
		p += NEOIOIC_ALIGNED_SIZE(struct em_neoioic);
	} else {
		return R_N;
	}

	if (NULL == exotic_memory)
		*object_out = (struct object *)p_ic;

	return R_P;
}
/* </crt> */
/* </oicc> */

/* Interface */

static int ship_open(struct object *self, const struct vr *caller_context)
{
	struct em_vehicle *pvem = get_base_members(self, MSN_VEHICLE);
	struct em_ship *members = (struct em_ship *)self->members;

	OBS_OBJECT_OPEN_;

	/* 在下面完成接口自己的任务。 */

	pvem->weight = 2.33f;
	members->max_load = 5.87f;
	members->max_velocity = 105.35f;

	return R_P;
}

static int ship_input(struct object *self, unsigned char *input_stream,
		      unsigned int quantity, const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INPUT;
}

static int ship_output(struct object *self, unsigned char *output_stream,
		       unsigned int capacity, unsigned int *output_quantity,
		       const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_OUTPUT;
}

static int ship_io_put(struct object *self, unsigned char *input_stream,
		       unsigned int quantity, unsigned char *output_stream,
		       unsigned int capacity, unsigned int *output_quantity,
		       const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_IOPUT;
}

static int ship_interact0(struct object *self, unsigned int action,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT0;
}

static int ship_interact1(struct object *self, unsigned int action,
			  unsigned char *output_stream, unsigned int capacity,
			  unsigned int *output_quantity,
			  const struct vr *caller_context)
{
	struct em_ship *members = (struct em_ship *)self->members;

	switch (action) {
	case GET_MAXLOAD:
		assert(capacity >= sizeof(float));
		*(float *)output_stream = members->max_load;
		*output_quantity = sizeof(float);
		return R_P;
	case GET_MAXVELOCITY:
		assert(capacity >= sizeof(float));
		*(float *)output_stream = members->max_velocity;
		*output_quantity = sizeof(float);
		return R_P;
	default:
		break;
	}

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT1;
}

static int ship_interact2(struct object *self, unsigned int action,
			  unsigned char *input_stream, unsigned int quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT2;
}

static int ship_interact3(struct object *self, unsigned int action,
			  unsigned char *input_stream, unsigned int quantity,
			  unsigned char *output_stream, unsigned int capacity,
			  unsigned int *output_quantity,
			  const struct vr *caller_context)
{

	/* 在上面完成接口自己的任务。 */

	SBO_OBJECT_INTERACT3;
}

static int ship_close(struct object *self, const struct vr *caller_context)
{
	OBS_OBJECT_CLOSE_;

	/* 在下面完成接口自己的任务。 */

	if (0 == self->ref_count)
		return RP_RCZERO;

	return R_P;
}

/* TOG */

void tog_ship(struct object *object)
{
	object->open = ship_open;
	object->input = ship_input;
	object->output = ship_output;
	object->io_put = ship_io_put;
	object->interact0 = ship_interact0;
	object->interact1 = ship_interact1;
	object->interact2 = ship_interact2;
	object->interact3 = ship_interact3;
	object->close = ship_close;
}

/* IRF(s) */

/* Function(s) */
