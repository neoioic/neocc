/* SPDX-License-Identifier: MIT */
#include "host.h"
#include "../objects/project.h"
#include "neoioic.h"

/* Single-threaded reusable node IDs; no allocation needed during destruction. */
static unsigned char ids[UINT16_MAX + 1];

int host_create_object(unsigned short model_id, unsigned int input_capacity,
	unsigned int output_capacity, int caller_capacity,
	struct object **object_out, unsigned char *exotic_memory)
{
	extern int crt_project(uint64_t *, unsigned int, unsigned int, int,
			       struct object **, unsigned char *);
	size_t i;
	uint64_t oid;
	int result;
	if (model_id != MSN_PROJECT)
		return RN_INVALIDARG;
	for (i = 1; i < UINT16_MAX - 1; i++)
		if (!ids[i] && !ids[i + 1])
			break;
	if (i == UINT16_MAX - 1)
		return RN_LACKOID;
	oid = OID_OPENOICC + i;
	result = crt_project(&oid, input_capacity, output_capacity,
			     caller_capacity, object_out, exotic_memory);
	if (RP(result))
		ids[i] = ids[i + 1] = 1;
	return result;
}

char host_destroy_object(struct object *object)
{
	size_t i;
	struct em_neoioic *root;
	if (!object || object->ref_count)
		return FALSE;
	for (i = 0; i < object->node_count; i++)
		if (object->nodes[i].ref_count)
			return FALSE;
	project_clear(object->members);
	root = get_base_members(object, MSN_NEOIOIC);
	free(root->iq.data);
	free(root->oq.data);
	root->iq = (struct queue){ 0 };
	root->oq = (struct queue){ 0 };
	for (i = 0; i < object->node_count; i++) {
		ids[object->nodes[i].oid & UINT16_MAX] = 0;
		object->nodes[i].oid = 0;
	}
	return TRUE;
}
