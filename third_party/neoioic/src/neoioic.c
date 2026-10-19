/*
 * NEOioic 节点的 .c 文件。
 * 创 建 者：赵平智   <bruce.zhao.zpz@gmail.com>
 * 版权(C)2005-2011 赵平智。保留所有权利。
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "neoioic.h"

/* Interface */

/*
 * open接口。
 * 参数：caller_context --- [IN] 来访者。
 */
static int neoioic_open(struct object *self, const struct vr *caller_context)
{
	if (self->ref_count == UINT_MAX)
		return RN_FULL;
	self->ref_count++;
	return R_P;
}

/*
 * input接口。
 * 参数：input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
static int neoioic_input(struct object *self, unsigned char *input_stream,
		       unsigned int quantity, const struct vr *caller_context)
{
	return RO_SBOOIX;
}

/*
 * output接口。
 * 参数：output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
static int neoioic_output(struct object *self, unsigned char *output_stream,
			unsigned int capacity, unsigned int *output_quantity,
			const struct vr *caller_context)
{
	return RO_SBOOIX;
}

/*
 * io_put接口。
 * 参数：input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
static int neoioic_io_put(struct object *self, unsigned char *input_stream,
			unsigned int quantity, unsigned char *output_stream,
			unsigned int capacity, unsigned int *output_quantity,
			const struct vr *caller_context)
{
	return RO_SBOOIX;
}

/*
 * interact0接口。
 * 参数：action --- [IN] 交互行为；
 *       caller_context --- [IN] 来访者。
 */
static int neoioic_interact0(struct object *self, unsigned int action,
			   const struct vr *caller_context)
{
	struct em_neoioic *members = (struct em_neoioic *)self->members;

	switch (action) {
	case ASK_SHARED: /* 问共享么。 */
		if (members->mds & MOD_SHARED)
			return R_N;
		return R_P;
	case SET_SHARED: /* 设置共享。 */
		members->mds &= ~MOD_SHARED;
		return R_P;
	case SET_UNSHARED: /* 设置非共享。 */
		members->mds |= MOD_SHARED;
		return R_P;
	case SET_BLOCKI: /* 设置阻断输入。 */
		members->mds &= ~MOD_BLOCKI;
		return R_P;
	case SET_UNBLOCKI: /* 设置非阻断输入。 */
		members->mds |= MOD_BLOCKI;
		return R_P;
	case SET_BLOCKO: /* 设置阻断输出。 */
		members->mds &= ~MOD_BLOCKO;
		return R_P;
	case SET_UNBLOCKO: /* 设置非阻断输出。 */
		members->mds |= MOD_BLOCKO;
		return R_P;
	case CMD_RESET: /* 重置。 */
		members->iq.front = members->iq.rear = NULL;
		members->iq.quantity = members->iq.lost = 0;
		members->oq.front = members->oq.rear = NULL;
		members->oq.quantity = members->oq.lost = 0;
		return R_P;
	default:
		break;
	}

	return RO_SBOOIX;
}

/*
 * interact1接口。
 * 参数：action --- [IN] 交互行为；
 *       output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
static int neoioic_interact1(struct object *self, unsigned int action,
			   unsigned char *output_stream, unsigned int capacity,
			   unsigned int *output_quantity,
			   const struct vr *caller_context)
{
	struct em_neoioic *members = (struct em_neoioic *)(self->members);

	switch (action) {
	case GET_IQCTY: /* 获取iq容量。 */
		if (output_quantity != NULL)
			*output_quantity = 0;
		if (output_quantity == NULL || output_stream == NULL ||
		    capacity < sizeof(unsigned int))
			return RN_INVALIDARG;
		memcpy(output_stream, &members->iq.capacity, sizeof(unsigned int));
		*output_quantity = sizeof(unsigned int);
		return R_P;
	case GET_OQCTY: /* 获取oq容量。 */
		if (output_quantity != NULL)
			*output_quantity = 0;
		if (output_quantity == NULL || output_stream == NULL ||
		    capacity < sizeof(unsigned int))
			return RN_INVALIDARG;
		memcpy(output_stream, &members->oq.capacity, sizeof(unsigned int));
		*output_quantity = sizeof(unsigned int);
		return R_P;
	default:
		break;
	}

	return RO_SBOOIX;
}

/*
 * interact2接口。
 * 参数：action --- [IN] 交互行为；
 *       input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
static int neoioic_interact2(struct object *self, unsigned int action,
			   unsigned char *input_stream, unsigned int quantity,
			   const struct vr *caller_context)
{
	struct em_neoioic *members = (struct em_neoioic *)(self->members);

	switch (action) {
	case SET_IQCTY: /* 设置iq容量。 */
		if (input_stream == NULL || quantity != sizeof(unsigned int))
			return RN_INVALIDARG;
		{
			unsigned int new_capacity;
			unsigned char *new_data;
			memcpy(&new_capacity, input_stream, sizeof(new_capacity));
			new_data = new_capacity ? calloc(new_capacity, 1) : NULL;
			if (new_capacity && new_data == NULL)
				return R_N;
			free(members->iq.data);
			members->iq.data = new_data;
			members->iq.front = members->iq.rear = NULL;
			members->iq.capacity = new_capacity;
			members->iq.quantity = members->iq.lost = 0;
			return R_P;
		}
	case SET_OQCTY: /* 设置oq容量。 */
		if (input_stream == NULL || quantity != sizeof(unsigned int))
			return RN_INVALIDARG;
		{
			unsigned int new_capacity;
			unsigned char *new_data;
			memcpy(&new_capacity, input_stream, sizeof(new_capacity));
			new_data = new_capacity ? calloc(new_capacity, 1) : NULL;
			if (new_capacity && new_data == NULL)
				return R_N;
			free(members->oq.data);
			members->oq.data = new_data;
			members->oq.front = members->oq.rear = NULL;
			members->oq.capacity = new_capacity;
			members->oq.quantity = members->oq.lost = 0;
			return R_P;
		}
	default:
		break;
	}

	return RO_SBOOIX;
}

/*
 * interact3接口。
 * 参数：action --- [IN] 交互行为；
 *       input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
static int neoioic_interact3(struct object *self, unsigned int action,
			   unsigned char *input_stream, unsigned int quantity,
			   unsigned char *output_stream, unsigned int capacity,
			   unsigned int *output_quantity,
			   const struct vr *caller_context)
{
	return RO_SBOOIX;
}

/*
 * close接口。
 * 参数：caller_context --- [IN] 来访者。
 */
static int neoioic_close(struct object *self, const struct vr *caller_context)
{
	if (self->ref_count == 0)
		return RO_NOPEN;
	self->ref_count--;
	return R_P;
}

/* TOG */

void tog_neoioic(struct object *object)
{
	object->open = neoioic_open;
	object->input = neoioic_input;
	object->output = neoioic_output;
	object->io_put = neoioic_io_put;
	object->interact0 = neoioic_interact0;
	object->interact1 = neoioic_interact1;
	object->interact2 = neoioic_interact2;
	object->interact3 = neoioic_interact3;
	object->close = neoioic_close;
}

/* IRF(s) */

/* Function(s) */
