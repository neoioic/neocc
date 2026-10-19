/* SPDX-License-Identifier: MIT
 * Checked layout shared by the bootstrap host and generated constructors.
 * The caller owns external storage; queue buffers always belong to the object.
 */
#ifndef OPENOICC_CONSTRUCT_H
#define OPENOICC_CONSTRUCT_H

#include <limits.h>
#include <string.h>
#include "neoioic.h"

struct openoicc_layout {
	unsigned short model_id;
	void (*bind)(struct object *);
	size_t member_size;
	size_t member_alignment;
	size_t seats;
	const unsigned short *direct;
	size_t direct_count;
	const unsigned short *bases;
	size_t base_count;
};

/* Reserve aligned blocks without overflowing either padding or multiplication. */
static inline int openoicc_reserve(size_t *size, size_t count, size_t unit,
			       size_t alignment, size_t *offset)
{
	size_t padding;
	if (!alignment)
		return 0;
	padding = (*size % alignment) ? alignment - *size % alignment : 0;
	if (padding > SIZE_MAX - *size)
		return 0;
	*size += padding;
	*offset = *size;
	if (count && unit > (SIZE_MAX - *size) / count)
		return 0;
	*size += count * unit;
	return 1;
}

static inline int openoicc_construct(const struct openoicc_layout *layout, size_t count,
		uint64_t *object_id, unsigned int input_capacity,
		unsigned int output_capacity, int caller_capacity,
		struct object **object_out, unsigned char *exotic_memory)
{
	size_t size = 0, offset, i, j;
	unsigned char *memory;
	struct object *nodes;
	struct em_neoioic *root = NULL;
	uint64_t next;

	if (object_out)
		*object_out = NULL;
	if (!layout || !object_id || !*object_id || !count || count > USHRT_MAX ||
	    caller_capacity < 1 || (!!object_out == !!exotic_memory))
		return RN_INVALIDARG;
	next = *object_id;
	/* Do not let the low node field wrap into the host field. */
	if (count > UINT16_MAX - (next & UINT16_MAX))
		return RN_LACKOID;
	if (!openoicc_reserve(&size, count, sizeof(struct object),
			  _Alignof(struct object), &offset))
		return RN_INVALIDARG;
	for (i = 0; i < count; i++) {
		const struct openoicc_layout *item = layout + i;
		if (!item->model_id || !item->bind || !item->seats ||
		    item->seats > USHRT_MAX / (size_t)caller_capacity ||
		    item->direct_count > USHRT_MAX || item->base_count > USHRT_MAX ||
		    !item->member_alignment ||
		    (item->member_alignment & (item->member_alignment - 1)) ||
		    item->member_alignment > _Alignof(struct object))
			return RN_INVALIDARG;
		if ((item->direct_count && !item->direct) ||
		    (item->base_count && !item->bases) ||
		    (item->model_id == MSN_NEOIOIC &&
		     (item->member_size < sizeof(struct em_neoioic) ||
		      item->direct_count || item->base_count)))
			return RN_INVALIDARG;
		for (j = 0; j < i; j++)
			if (layout[j].model_id == item->model_id)
				return RN_INVALIDARG;
		for (j = 0; j < item->direct_count; j++)
			if (item->direct[j] >= count || item->direct[j] == i)
				return RN_INVALIDARG;
		for (j = 0; j < item->base_count; j++)
			if (item->bases[j] >= count || item->bases[j] == i)
				return RN_INVALIDARG;
		if (!openoicc_reserve(&size, item->direct_count, sizeof(struct object *),
				  _Alignof(struct object *), &offset) ||
		    !openoicc_reserve(&size, item->base_count, sizeof(struct object *),
				  _Alignof(struct object *), &offset) ||
		    !openoicc_reserve(&size, item->seats * (size_t)caller_capacity,
				  sizeof(struct vr), _Alignof(struct vr), &offset) ||
		    !openoicc_reserve(&size, 1, item->member_size,
				  _Alignof(struct object), &offset))
			return RN_INVALIDARG;
	}
	memory = exotic_memory ? exotic_memory : calloc(1, size);
	if (!memory)
		return R_N;
	if (exotic_memory)
		memset(memory, 0, size);
	nodes = (struct object *)memory;
	size = count * sizeof(struct object);
	for (i = 0; i < count; i++) {
		const struct openoicc_layout *item = layout + i;
		struct object *node = nodes + i;
		item->bind(node);
		node->model_id = item->model_id;
		node->oid = next++;
		node->nodes = nodes;
		node->node_count = (unsigned short)count;
		openoicc_reserve(&size, item->direct_count, sizeof(struct object *),
			     _Alignof(struct object *), &offset);
		node->direct_base_count = (unsigned short)item->direct_count;
		node->direct_bases = item->direct_count ?
			(struct object **)(memory + offset) : NULL;
		for (j = 0; j < item->direct_count; j++)
			node->direct_bases[j] = nodes + item->direct[j];
		openoicc_reserve(&size, item->base_count, sizeof(struct object *),
			     _Alignof(struct object *), &offset);
		node->base_count = (unsigned short)item->base_count;
		node->bases = item->base_count ?
			(struct object **)(memory + offset) : NULL;
		for (j = 0; j < item->base_count; j++)
			node->bases[j] = nodes + item->bases[j];
		node->caller_count =
			(unsigned short)(item->seats * (size_t)caller_capacity);
		openoicc_reserve(&size, node->caller_count, sizeof(struct vr),
			     _Alignof(struct vr), &offset);
		node->callers = (struct vr *)(memory + offset);
		openoicc_reserve(&size, 1, item->member_size, _Alignof(struct object), &offset);
		node->members = item->member_size ? memory + offset : NULL;
		if (item->model_id == MSN_NEOIOIC)
			root = node->members;
	}
	if (!root)
		goto fail;
	if (input_capacity) {
		root->iq.data = malloc(input_capacity);
		if (!root->iq.data)
			goto fail;
	}
	if (output_capacity) {
		root->oq.data = malloc(output_capacity);
		if (!root->oq.data)
			goto fail;
	}
	root->iq.capacity = input_capacity;
	root->oq.capacity = output_capacity;
	*object_id = next;
	if (object_out)
		*object_out = nodes;
	return R_P;
fail:
	if (root) {
		free(root->iq.data);
		free(root->oq.data);
	}
	if (!exotic_memory)
		free(memory);
	else
		memset(memory, 0, size);
	return R_N;
}

#endif
