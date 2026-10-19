/*
 * object.c 文件。
 * 创 建 者：赵平智   <bruce.zhao.zpz@gmail.com>
 * 版权(C)2005-2011 赵平智。保留所有权利。
 */

#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "neoioic.h"

/* init_direct_bases & init_base_nodes */

/* 注：init_direct_bases 和 init_base_nodes只由CRT函数调用。 */

/*
 * 初始化ND的AIB。
 * 参数：node --- [IN] ND指针；
 *       base_msns --- [IN] IB的model_id数组；
 *       nib --- [IN] base_msns所指数组的元素个数，即IB的数量。
 * 备注：1. 只由CRT函数调用。
 *       2. 注意，base_msns所指数组元素的排序决定了IB的访问顺序。
 */
void init_direct_bases(struct object *node, unsigned short *base_msns,
		       unsigned short nib)
{
	unsigned short i;
	unsigned short j;

	node->direct_base_count = nib;

	while (nib) {
		j = node->direct_base_count - nib;
		for (i = 1; i < node->node_count; i++) {
			if (*(base_msns + j) == (node->nodes + i)->model_id) {
				*(node->direct_bases + j) = node->nodes + i;
				break;
			}
		}
		nib--;
	}
}

/*
 * 初始化ND的ABN。
 * 参数：node --- [IN] ND指针；
 *       base_msns --- [IN] BN的model_id数组；
 *       nbn --- [IN] base_msns所指数组的元素个数，即BN的数量。
 * 备注：只由CRT函数调用。
 */
void init_base_nodes(struct object *node, unsigned short *base_msns,
		     unsigned short nbn)
{
	unsigned short i;
	unsigned short j;

	node->base_count = nbn;

	while (nbn) {
		j = node->base_count - nbn;
		for (i = 1; i < node->node_count; i++) {
			if (*(base_msns + j) == (node->nodes + i)->model_id) {
				*(node->bases + j) = node->nodes + i;
				break;
			}
		}
		nbn--;
	}
}

/* register_caller & unregister_caller */

/* 注：不允许外面使用 register_caller 和 unregister_caller。 */

/*
 * 来访者来访，登记来访记录。
 * 参数：node --- [IN] ND地址；
 *       caller_context --- [IN] 来访者。
 * 备注：不允许外面使用。
 */
int register_caller(struct object *node, const struct vr *caller_context)
{
	unsigned short i;
	struct vr *visitor;

	/* 先检查有没有这个struct vr。 */
	for (i = 0; i < node->caller_count; i++) {
		visitor = node->callers + i;
		if ((visitor->cr == caller_context->cr) &&
		    (visitor->mr == caller_context->mr))
			return RN_HASVR; /* 已经有这个struct vr了。 */
	}

	/* 加上这个struct vr。 */
	for (i = 0; i < node->caller_count; i++) {
		visitor = node->callers + i;
		if (0 == visitor->cr) {
			*visitor = *caller_context;
			return R_P;
		}
	}

	return R_N;
}

/*
 * 来访者离开，清除来访记录。
 * 参数：node --- [IN] ND地址；
 *       caller_context --- [IN] 来访者。
 * 备注：不允许外面使用。
 */
int unregister_caller(struct object *node, const struct vr *caller_context)
{
	unsigned short i;
	unsigned short j;
	struct vr *visitor;

	/* 首先ND的BN */
	for (i = 0; i < node->base_count; i++) {
		for (j = 0; j < (*(node->bases + i))->caller_count; j++) {
			visitor = (*(node->bases + i))->callers + j;
			if ((visitor->cr == caller_context->cr) &&
			    (visitor->mr == caller_context->mr))
				visitor->cr = visitor->mr = 0;
		}
	}

	/* 然后ND自己 */
	for (j = 0; j < node->caller_count; j++) {
		visitor = node->callers + j;
		if ((visitor->cr == caller_context->cr) &&
		    (visitor->mr == caller_context->mr))
			visitor->cr = visitor->mr = 0;
	}

	return R_P;
}

/* enqueue & dequeue */

/*
 * 向指定的队列入列指定数量的Byte。
 * 参数：input_data --- [IN] 入列数据；
 *       quantity --- [IN] 入列的Byte数量。
 */
int enqueue(struct queue *pq, unsigned char *input_data, unsigned int quantity)
{
	unsigned int cty_free, qty_enq, rear_offset, first;

	assert(pq != NULL);
	assert(input_data != NULL);

	cty_free = pq->capacity - pq->quantity;
	qty_enq = (cty_free > quantity) ? quantity : cty_free;

	if (0 == qty_enq) {
		if (quantity > 0) {
			pq->lost = quantity; /* 丢失了 quantity 个Byte ！ */
			return RN_LOST;
		}

		return RO_VAINLY;
	}

	rear_offset = pq->quantity == 0 ? 0 :
		(unsigned int)(pq->rear - pq->data + 1);
	if (rear_offset == pq->capacity)
		rear_offset = 0;
	first = pq->capacity - rear_offset;
	if (first > qty_enq)
		first = qty_enq;
	memcpy(pq->data + rear_offset, input_data, first);
	memcpy(pq->data, input_data + first, qty_enq - first);
	pq->rear = pq->data + (qty_enq > pq->capacity - rear_offset ?
		qty_enq - (pq->capacity - rear_offset) - 1 :
		rear_offset + (qty_enq - 1));
	if (pq->quantity == 0)
		pq->front = pq->data + rear_offset;
	pq->quantity += qty_enq;

	if (cty_free < quantity) {
		pq->lost = quantity -
			   cty_free; /* 丢失了 quantity - cty_free 个Byte ！ */
		return RN_LOST;
	}

	return R_P;
}

/*
 * 从指定队列出列Byte。
 * 参数：output_data --- [OUT] 出列数据的存储位置；
 *       capacity --- [IN] output_data所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际出列的Byte数量。
 */
int dequeue(struct queue *pq, unsigned char *output_data, unsigned int capacity,
	    unsigned int *output_quantity)
{
	unsigned int qty_deq, front_offset, first;

	assert(pq != NULL);
	assert(output_data != NULL);
	assert(capacity != 0);
	assert(output_quantity != NULL);

	*output_quantity = 0; /* 先将出列数置0。 */

	if (pq->quantity != 0) {
		if (pq->capacity == 0)
			return RN_INVALIDARG;
		qty_deq = (capacity > pq->quantity) ? pq->quantity : capacity;
		front_offset = (unsigned int)(pq->front - pq->data);
		first = pq->capacity - front_offset;
		if (first > qty_deq)
			first = qty_deq;
		memcpy(output_data, pq->data + front_offset, first);
		memcpy(output_data + first, pq->data, qty_deq - first);

		pq->quantity -= qty_deq;

		if (0 == pq->quantity)
			pq->front = pq->rear = NULL;
		else {
			unsigned int tail = pq->capacity - front_offset;
			unsigned int next = qty_deq >= tail ?
				qty_deq - tail : front_offset + qty_deq;
			pq->front = pq->data + next;
		}

	} else {
		return RO_VAINLY;
	}

	*output_quantity = qty_deq;

	if (pq->lost > 0) {
		pq->lost = 0;
		return RN_LOST;
	}

	return R_P;
}

/* get_base_node & get_base_members */

/*
 * 获取指定BN的地址。
 * 参数：node --- [IN] ND指针；
 *       model_id --- [IN] BN的M。
 * 返回：BN的指针。
 */
struct object *get_base_node(struct object *node, unsigned short model_id)
{
	unsigned short i;

	assert(node != NULL);

	for (i = 0; i < node->base_count; i++) {
		if (model_id == (*(node->bases + i))->model_id)
			return *(node->bases + i);
	}

	return NULL; /* 没有这个BN。 */
}

/*
 * 获取指定BN的void地址。
 * 参数：node --- [IN] ND指针；
 *       model_id --- [IN] BN的M。
 * 返回：void的指针。
 */
void *get_base_members(struct object *node, unsigned short model_id)
{
	unsigned short i;

	assert(node != NULL);

	for (i = 0; i < node->base_count; i++) {
		if (model_id == (*(node->bases + i))->model_id)
			return (*(node->bases + i))->members;
	}

	return NULL;
}

/* get_iq & get_oq */

/*
 * 获取节点的iq指针。
 * 参数：node --- [IN] 节点指针。
 * 返回：iq的指针。
 */
struct queue *get_iq(struct object *node)
{
	unsigned short i;

	assert(node != NULL);

	for (i = 0; i < node->base_count; i++) {
		if (MSN_NEOIOIC == (*(node->bases + i))->model_id)
			return &((struct em_neoioic *)((*(node->bases + i))
							     ->members))
					->iq;
	}

	return NULL;
}

/*
 * 获取节点的oq指针。
 * 参数：node --- [IN] 节点指针。
 * 返回：oq的指针。
 */
struct queue *get_oq(struct object *node)
{
	unsigned short i;

	assert(node != NULL);

	for (i = 0; i < node->base_count; i++) {
		if (MSN_NEOIOIC == (*(node->bases + i))->model_id)
			return &((struct em_neoioic *)((*(node->bases + i))
							     ->members))
					->oq;
	}

	return NULL;
}

/* 非内联VO函数 */

/* 注意：内联VO函数和非内联VO函数的功能是完全相同的。 */

#if INLINE_VOFUNCTION == 0

/*
 * 接口open的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       caller_context --- [IN] 来访者。
 */
int vo_open(struct object *object, const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	if (R_P == register_caller(object, caller_context)) {
		ir = object->open(object, caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口input的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
int vo_input(struct object *object, unsigned char *input_stream,
	     unsigned int quantity, const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert(input_stream != NULL);
	assert(quantity > 0);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	if (R_P == register_caller(object, caller_context)) {
		ir = object->input(object, input_stream, quantity,
				   caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口output的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
int vo_output(struct object *object, unsigned char *output_stream,
	      unsigned int capacity, unsigned int *output_quantity,
	      const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert(output_stream != NULL);
	assert(capacity > 0);
	assert(output_quantity != NULL);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	*output_quantity = 0;

	if (R_P == register_caller(object, caller_context)) {
		ir = object->output(object, output_stream, capacity,
				    output_quantity, caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口io_put的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
int vo_io_put(struct object *object, unsigned char *input_stream,
	      unsigned int quantity, unsigned char *output_stream,
	      unsigned int capacity, unsigned int *output_quantity,
	      const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert(input_stream != NULL);
	assert(quantity > 0);
	assert(output_stream != NULL);
	assert(capacity > 0);
	assert(output_quantity != NULL);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	*output_quantity = 0;

	if (R_P == register_caller(object, caller_context)) {
		ir = object->io_put(object, input_stream, quantity,
				    output_stream, capacity, output_quantity,
				    caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口interact0的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       action --- [IN] 交互行为；
 *       caller_context --- [IN] 来访者。
 */
int vo_interact0(struct object *object, unsigned int action,
		 const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert((MSKAC & action) == AC0);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	if (R_P == register_caller(object, caller_context)) {
		ir = object->interact0(object, action, caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口interact1的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       action --- [IN] 交互行为；
 *       output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
int vo_interact1(struct object *object, unsigned int action,
		 unsigned char *output_stream, unsigned int capacity,
		 unsigned int *output_quantity, const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert((MSKAC & action) == AC1);
	assert(output_stream != NULL);
	assert(capacity > 0);
	assert(output_quantity != NULL);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	*output_quantity = 0;

	if (R_P == register_caller(object, caller_context)) {
		ir = object->interact1(object, action, output_stream, capacity,
				       output_quantity, caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口interact2的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       action --- [IN] 交互行为；
 *       input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
int vo_interact2(struct object *object, unsigned int action,
		 unsigned char *input_stream, unsigned int quantity,
		 const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert((MSKAC & action) == AC2);
	assert(input_stream != NULL);
	assert(quantity > 0);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	if (R_P == register_caller(object, caller_context)) {
		ir = object->interact2(object, action, input_stream, quantity,
				       caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口interact3的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       action --- [IN] 交互行为；
 *       input_stream --- [IN] 输入流；
 *       quantity --- [IN] 输入的Byte数量；
 *       output_stream --- [OUT] 输出流的流向；
 *       capacity --- [IN] output_stream所指存储空间的容量；
 *       output_quantity --- [OUT] 指向实际输出的Byte数量；
 *       caller_context --- [IN] 来访者。
 */
int vo_interact3(struct object *object, unsigned int action,
		 unsigned char *input_stream, unsigned int quantity,
		 unsigned char *output_stream, unsigned int capacity,
		 unsigned int *output_quantity, const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert((MSKAC & action) == AC3);
	assert(input_stream != NULL);
	assert(quantity > 0);
	assert(output_stream != NULL);
	assert(capacity > 0);
	assert(output_quantity != NULL);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	*output_quantity = 0;

	if (R_P == register_caller(object, caller_context)) {
		ir = object->interact3(object, action, input_stream, quantity,
				       output_stream, capacity, output_quantity,
				       caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

/*
 * 接口close的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       caller_context --- [IN] 来访者。
 */
int vo_close(struct object *object, const struct vr *caller_context)
{
	extern int register_caller(struct object * node,
				   const struct vr *caller_context);
	extern int unregister_caller(struct object * node,
				     const struct vr *caller_context);

	int ir = R_O;

	assert(object != NULL);
	assert(caller_context->cr != 0);
	assert(caller_context->mr != 0);

	if (R_P == register_caller(object, caller_context)) {
		ir = object->close(object, caller_context);
		unregister_caller(object, caller_context);
	} else {
		ir = R_N;
	}

	return ir;
}

#endif /* #if INLINE_VOFUNCTION == 0 */
