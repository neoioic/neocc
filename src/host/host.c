#include "host.h"
#include "neoioic.h"

/* Single-threaded host allocation; released IDs can be reused. */
static unsigned char ids[UINT16_MAX + 1];
const struct vr neocc_host_caller = { NEOCC_OID_HOST, NEOCC_OID_HOST };

int host_create_object(unsigned short model_id, unsigned int input_capacity,
	unsigned int output_capacity, int caller_capacity,
	struct object **object_out, unsigned char *exotic_memory)
{
	extern int crt_source(uint64_t *, unsigned int, unsigned int, int,
			      struct object **, unsigned char *);
	extern int crt_tokens(uint64_t *, unsigned int, unsigned int, int,
			      struct object **, unsigned char *);
	extern int crt_preprocessor(uint64_t *, unsigned int, unsigned int, int,
				    struct object **, unsigned char *);
	extern int crt_symbols(uint64_t *, unsigned int, unsigned int, int,
			       struct object **, unsigned char *);
	extern int crt_functions(uint64_t *, unsigned int, unsigned int, int,
				 struct object **, unsigned char *);
	extern int crt_scopes(uint64_t *, unsigned int, unsigned int, int,
			      struct object **, unsigned char *);
	extern int crt_expressions(uint64_t *, unsigned int, unsigned int, int,
				   struct object **, unsigned char *);
	extern int crt_switches(uint64_t *, unsigned int, unsigned int, int,
				struct object **, unsigned char *);
	extern int crt_initializers(uint64_t *, unsigned int, unsigned int, int,
				    struct object **, unsigned char *);
	extern int crt_temporaries(uint64_t *, unsigned int, unsigned int, int,
				   struct object **, unsigned char *);
	extern int crt_translation(uint64_t *, unsigned int, unsigned int, int,
				   struct object **, unsigned char *);
	size_t i;
	uint64_t oid;
	int result;

	if (object_out)
		*object_out = NULL;
	if ((model_id != NEOCC_MSN_SOURCE && model_id != NEOCC_MSN_TOKENS &&
	     model_id != NEOCC_MSN_PREPROCESSOR && model_id != NEOCC_MSN_SYMBOLS &&
	     model_id != NEOCC_MSN_EXPRESSIONS && model_id != NEOCC_MSN_SCOPES &&
	     model_id != NEOCC_MSN_FUNCTIONS && model_id != NEOCC_MSN_SWITCHES &&
	     model_id != NEOCC_MSN_INITIALIZERS &&
	     model_id != NEOCC_MSN_TEMPORARIES &&
	     model_id != NEOCC_MSN_TRANSLATION) ||
	    caller_capacity < 1 ||
	    (!!object_out == !!exotic_memory))
		return RN_INVALIDARG;
	for (i = 1; i < UINT16_MAX - 1; i++)
		if (!ids[i] && !ids[i + 1])
			break;
	if (i == UINT16_MAX - 1)
		return RN_LACKOID;
	oid = NEOCC_OID_HOST + i;
	if (model_id == NEOCC_MSN_SOURCE)
		result = crt_source(&oid, input_capacity, output_capacity,
				    caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_TOKENS)
		result = crt_tokens(&oid, input_capacity, output_capacity,
				    caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_PREPROCESSOR)
		result = crt_preprocessor(&oid, input_capacity, output_capacity,
					  caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_EXPRESSIONS)
		result = crt_expressions(&oid, input_capacity, output_capacity,
					 caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_FUNCTIONS)
		result = crt_functions(&oid, input_capacity, output_capacity,
				       caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_SWITCHES)
		result = crt_switches(&oid, input_capacity, output_capacity,
				     caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_INITIALIZERS)
		result = crt_initializers(&oid, input_capacity, output_capacity,
					 caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_TEMPORARIES)
		result = crt_temporaries(&oid, input_capacity, output_capacity,
					caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_TRANSLATION)
		result = crt_translation(&oid, input_capacity, output_capacity,
					caller_capacity, object_out, exotic_memory);
	else if (model_id == NEOCC_MSN_SCOPES)
		result = crt_scopes(&oid, input_capacity, output_capacity,
				    caller_capacity, object_out, exotic_memory);
	else
		result = crt_symbols(&oid, input_capacity, output_capacity,
				     caller_capacity, object_out, exotic_memory);
	if (RP(result))
		ids[i] = ids[i + 1] = 1;
	return result;
}

char host_destroy_object(struct object *object)
{
	struct em_neoioic *root;
	size_t i;

	if (!object || object != object->nodes ||
	    (object->model_id != NEOCC_MSN_SOURCE &&
	     object->model_id != NEOCC_MSN_TOKENS &&
	     object->model_id != NEOCC_MSN_PREPROCESSOR &&
	     object->model_id != NEOCC_MSN_SYMBOLS &&
	     object->model_id != NEOCC_MSN_EXPRESSIONS &&
	     object->model_id != NEOCC_MSN_SCOPES &&
	     object->model_id != NEOCC_MSN_FUNCTIONS &&
	     object->model_id != NEOCC_MSN_SWITCHES &&
	     object->model_id != NEOCC_MSN_INITIALIZERS &&
	     object->model_id != NEOCC_MSN_TEMPORARIES &&
	     object->model_id != NEOCC_MSN_TRANSLATION) || !object->oid)
		return FALSE;
	for (i = 0; i < object->node_count; i++) {
		struct object *node = object->nodes + i;
		if (node->ref_count)
			return FALSE;
	}
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
