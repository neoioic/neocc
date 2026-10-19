

#include <stdlib.h>
#include <string.h>
#include "host.h"

/* Definition */

/*
 * 记录已销毁对象的oid。
 * 备注：struct doid，Destroyed Object's ID。
 */
struct doid {
	unsigned short model_id;
	uint64_t oid;
};

/* struct doid数组结构体。 */
struct arrdoid {
	int neo; /* 存在的对象个数，即没有销毁的对象个数。eo - existent object */
	struct doid *
		pad; /* 指向元素类型为struct doid的数组，这个数组动态分配。ad - the array of struct doid */
	int lad; /* pad所指数组的空间长度，以元素为单位。lad - the length of array of struct doid */
	int nwe; /* pad所指数组中已填写过的元素个数。nwe - the number of written element */
	uint64_t oid; /* 传给CRT函数的oid。 */
};

/*
 * 宏，struct doid数组空间增长步长，以数组元素为单位。
 * 备注：STP - step，LAD - the length of array of struct doid，GLAD - the growth of LAD。
 */
#define STP_GLAD 10

/* Variable(s) */

static struct arrdoid gs_tad = { 0, NULL, 0, 0, OID_ };

/* host_create_object */

/*
 * 创建对象。
 * 参数：model_id --- [IN] 模序号，指定创建哪个模的对象；
 *       input_capacity --- [IN] 输入队列缓冲；
 *       output_capacity --- [IN] 输出队列缓冲；
 *       caller_capacity --- [IN] 来访者数量；
 *       object_out --- [OUT] 输出创建的对象；
 *       exotic_memory --- [IN] 在外面为IC分配的空间，适用于创建静态对象（如函数内的局部对象）。
 * 返回：成功返回P结果，失败返回N结果。
 * 备注：1. caller_capacity必须大于0；
 *       2. object_out和exotic_memory不能都为NULL或非NULL；
 *       3. 如果exotic_memory不是NULL，表示IC空间已在外面分配好了。
 */
int host_create_object(unsigned short model_id, unsigned int input_capacity,
		       unsigned int output_capacity, int caller_capacity,
		       struct object **object_out, unsigned char *exotic_memory)
{
	/* extern int   CRT_xxx(uint64_t* object_id, unsigned int input_capacity, unsigned int output_capacity, int caller_capacity, struct object** object_out, unsigned char* exotic_memory); */
	extern int crt_car(uint64_t * object_id, unsigned int input_capacity,
			   unsigned int output_capacity, int caller_capacity,
			   struct object **object_out,
			   unsigned char *exotic_memory);
	extern int crt_ship(uint64_t * object_id, unsigned int input_capacity,
			    unsigned int output_capacity, int caller_capacity,
			    struct object **object_out,
			    unsigned char *exotic_memory);
	extern int crt_amphicar(
		uint64_t * object_id, unsigned int input_capacity,
		unsigned int output_capacity, int caller_capacity,
		struct object **object_out, unsigned char *exotic_memory);

	int fr;
	uint64_t oidbak;

	struct em_neoioic *members;
	int idd; /* struct doid的指标。idd - the index of struct doid */

	if ((caller_capacity < 1) ||
	    (NULL == object_out && NULL == exotic_memory) ||
	    (object_out != NULL && exotic_memory != NULL))
		return RN_INVALIDARG;

	oidbak = gs_tad.oid;

	for (idd = 0; idd < gs_tad.nwe; idd++) {
		if ((model_id == (gs_tad.pad + idd)->model_id) &&
		    ((gs_tad.pad + idd)->oid != 0)) {
			gs_tad.oid = (gs_tad.pad + idd)->oid;
			break;
		}
	}

	switch (model_id) {
	/*
	 * fr = CRT_xxx(&gs_tad.oid, input_capacity, output_capacity, caller_capacity, object_out, exotic_memory);
	 * break;
	 */
	case MSN_CAR:
		fr = crt_car(&gs_tad.oid, input_capacity, output_capacity,
			     caller_capacity, object_out, exotic_memory);
		break;
	case MSN_SHIP:
		fr = crt_ship(&gs_tad.oid, input_capacity, output_capacity,
			      caller_capacity, object_out, exotic_memory);
		break;
	case MSN_AMPHICAR:
		fr = crt_amphicar(&gs_tad.oid, input_capacity, output_capacity,
				  caller_capacity, object_out, exotic_memory);
		break;
	default:
		fr = R_N;
		break;
	}

	if (!RN(fr)) {
		if (idd < gs_tad.nwe) {
			gs_tad.oid = oidbak;
			(gs_tad.pad + idd)->oid = 0;
		} else if ((MSKSO & gs_tad.oid) < (MSKSO & oidbak)) {
			gs_tad.oid = oidbak;

			members = (struct em_neoioic *)get_base_members(
				exotic_memory != NULL ?
					(struct object *)exotic_memory : *object_out,
				MSN_NEOIOIC);
			if (members->iq.data != NULL)
				free(members->iq.data);
			if (members->oq.data != NULL)
				free(members->oq.data);

			members->iq = (struct queue){ 0 };
			members->oq = (struct queue){ 0 };

			if (NULL == exotic_memory) {
				free(*object_out);
				*object_out = NULL;
			}

			return RN_LACKOID;
		}

		gs_tad.neo++;

	} else {
		gs_tad.oid = oidbak;
	}

	return fr;
}

/* host_destroy_object */

/*
 * 销毁对象。
 * 参数：object --- [IN] 指向对象。
 * 返回：TRUE - 成功，FALSE - 失败。
 * 备注：静态对象（如函数内的局部对象）也须用此函数销毁。
 */
char host_destroy_object(struct object *object)
{
	struct doid *ptr;
	int idd; /* struct doid的指标。idd - the index of struct doid */
	struct em_neoioic *members;

	if (NULL == object)
		return FALSE;

	members = (struct em_neoioic *)get_base_members(object, MSN_NEOIOIC);

	if (NULL == gs_tad.pad) {
		if (gs_tad.neo - 1 > 0) {
			gs_tad.pad = (struct doid *)calloc(STP_GLAD,
							   sizeof(struct doid));
			if (NULL == gs_tad.pad)
				return FALSE;
			gs_tad.pad->model_id = object->model_id;
			gs_tad.pad->oid = object->oid;
			gs_tad.lad = STP_GLAD;
			gs_tad.nwe = 1;
		} else if (0 == gs_tad.neo - 1) {
			gs_tad.oid = OID_;
		} else {
			return FALSE;
		}
	} else {
		for (idd = 0; idd < gs_tad.nwe; idd++) {
			if ((object->model_id ==
			     (gs_tad.pad + idd)->model_id) &&
			    (0 == (gs_tad.pad + idd)->oid)) {
				(gs_tad.pad + idd)->oid = object->oid;
				break;
			}
		}

		if (idd == gs_tad.nwe) {
			if (idd == gs_tad.lad) {
				ptr = (struct doid *)calloc(
					gs_tad.lad + STP_GLAD,
					sizeof(struct doid));
				if (NULL == ptr)
					return FALSE;
				memcpy(ptr, gs_tad.pad,
				       gs_tad.nwe * sizeof(struct doid));
				free(gs_tad.pad);
				gs_tad.pad = ptr;
				gs_tad.lad += STP_GLAD;
			}
			(gs_tad.pad + gs_tad.nwe)->model_id = object->model_id;
			(gs_tad.pad + gs_tad.nwe)->oid = object->oid;
			gs_tad.nwe++;
		}
	}

	if (members->iq.data != NULL)
		free(members->iq.data);
	if (members->oq.data != NULL)
		free(members->oq.data);

	gs_tad.neo--;

	if (gs_tad.pad != NULL && 0 == gs_tad.neo) {
		free(gs_tad.pad);
		gs_tad.pad = NULL;
		gs_tad.lad = 0;
		gs_tad.nwe = 0;
		gs_tad.oid = OID_;
	}

	return TRUE;
}

/* Global object pointer(s) */

struct object *g_p_car = NULL;
struct object *g_p_ship = NULL;
struct object *g_p_amphicar = NULL;
