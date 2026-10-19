/*
 * object.h 文件。
 * 创 建 者：赵平智   <bruce.zhao.zpz@gmail.com>
 * 版权(C)2005-2011 赵平智。保留所有权利。
 */

#ifndef NEOIOIC_OBJECT_H
#define NEOIOIC_OBJECT_H

/* #define  NDEBUG */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/*
 * 内联VO函数的开关。0 - 不内联，1 - 内联，其它 - 保留不用。
 * 备注：定义为1时，能否真正内联VO函数还取决于下面的“NEOIOIC_INLINE”宏定义是否有效。
 */
#define INLINE_VOFUNCTION 0

/*
 * 宏定义内联指示符，因为编译器的内联指示符不统一。
 * 备注：一定要定义为有效的内联指示符，否则即使编译通过，内联也是无效的，编译
 *       器会自动把用其修饰的函数转为非内联编译。
 */
#define NEOIOIC_INLINE inline

/*
 * OS、HO、SO的mask。
 * 备注：目前oid是用64个比特位表示的（见下面uint64_t数据类型定义）。其
 *       中，高16位是OS标识域，中32位是HO标识域，低16位是SO标识域。
 */
#define MSKOS 0xFFFF000000000000L
#define MSKHO 0x0000FFFFFFFF0000L
#define MSKSO 0x000000000000FFFFL

/* 各OS的oid。 */
#define OID_OICOS 0x0001000000000000L	/* Oicos的oid。 */
#define OID_UNIX 0x0002000000000000L	/* Unix的oid。 */
#define OID_LINUX 0x0003000000000000L	/* Linux的oid。 */
#define OID_WINDOWS 0x0004000000000000L /* Windows的oid。 */

/* OS的oid。 */
#if defined(_WIN32)
#define OID_OS OID_WINDOWS
#elif defined(__linux__)
#define OID_OS OID_LINUX
#else
#define OID_OS OID_UNIX
#endif

/* Byte大小（比特位数） */
#define BYTESIZE 8

/*
 * model_id
 * 备注：要求model_id从1开始顺序编号，定义模板如下，
 *       #define   MSN_XXX1    (MSN_ + 0)  /-* xxx1的model_id。HO *-/
 *       #define   MSN_XXX2    (MSN_ + 1)  /-* xxx2的model_id。AO *-/
 *       #define   MSN_XXX3    (MSN_ + 2)  /-* xxx3的model_id。.. *-/
 *       #define   MSN_XXX4    (MSN_ + 3)  /-* xxx4的model_id。GO *-/
 *       #define   MSN_XXX5    (MSN_ + 4)  /-* xxx5的model_id。.. *-/
 *       #define   MSN_XXXn    (MSN_ + n-1)  /-* xxxn的model_id。... *-/
 */
#define MSN_NEOIOIC 1	     /* NEOioic的model_id。 */
#define MSN_ (MSN_NEOIOIC + 1) /* 其它model_id的起点。 */

/*
 * 通用模式定义。
 * 备注：可定义 sizeof(unsigned int)*BYTESIZE 个通用模式。即如下，
 *       #define   MOD_                     1
 *       #define   MOD_XXX0             ( MOD_ << 0 )     /-* BIT0。1：xxx；0：xxx。*-/
 *       #define   MOD_XXX1             ( MOD_ << 1 )     /-* BIT1。1：xxx；0：xxx。*-/
 *       #define   MOD_XXX2             ( MOD_ << 2 )     /-* BIT2。1：xxx；0：xxx。*-/
 *       #define   MOD_XXXn            ( MOD_ << (sizeof(unsigned int)*BYTESIZE-1) )   /-* BITn。 1：xxx； 0：xxx。*-/
 */
#define MOD_ 1
#define MOD_SHARED (MOD_ << 0) /* BIT0。0：共享；1：非共享。 */
#define MOD_BLOCKI \
	(MOD_      \
	 << 1) /* BIT1。0：阻断输入；1：非阻断输入。只适用于input和io_put接口。 */
#define MOD_BLOCKO \
	(MOD_      \
	 << 2) /* BIT2。0：阻断输出；1：非阻断输出。只适用于output和io_put接口。 */

/*
 * 对象交互行为分类定义。
 * 备注：1. 按是否须要输入Byte流和有没有输出Byte流,可将交互行为分为四种：
 *          AC0 - 不需输入Byte流也没有输出Byte流；
 *          AC1 - 不需要输入Byte流但有输出Byte流；
 *          AC2 - 需要输入Byte流但没有输出Byte流；
 *          AC3 - 即需要输入Byte流也有输出Byte流。
 *       2. TSI和TSO
 *          TSI - the Type of Stream Inputted，输入流类型。
 *          TSO - the Type of Stream Outputted，输出流类型。
 */
#define AC0 0
#define AC1 (UINT32_C(1) << 30)
#define AC2 (UINT32_C(2) << 30)
#define AC3 (UINT32_C(3) << 30)

/* 判断交互行为种类的mask */
#define MSKAC AC3

/*
 * 行为定义。
 * 备注：=== 定义模板 ===
 *       /-** AC0 行为：**-/
 *       #define     XXX_YYYYY            (AC0_ + 0)
 *       #define     XXX_YYYYY            (AC0_ + 1)
 *       /-** AC1 行为：**-/
 *       #define     XXX_YYYYY            (AC1_ + 0)
 *       #define     XXX_YYYYY            (AC1_ + 1)
 *       /-** AC2 行为：**-/
 *       #define     XXX_YYYYY            (AC2_ + 0)
 *       #define     XXX_YYYYY            (AC2_ + 1)
 *       /-** AC3 行为：**-/
 *       #define     XXX_YYYYY            (AC3_ + 0)
 *       #define     XXX_YYYYY            (AC3_ + 1)
 */

/* AC0 行为：* */
#define ASK_SHARED (AC0 + 0)   /* 问对象共享么。 */
#define SET_SHARED (AC0 + 1)   /* 设置对象共享。 */
#define SET_UNSHARED (AC0 + 2) /* 设置对象非共享。 */
#define SET_BLOCKI (AC0 + 3)   /* 设置对象阻断输入。 */
#define SET_UNBLOCKI (AC0 + 4) /* 设置对象非阻断输入。 */
#define SET_BLOCKO (AC0 + 5)   /* 设置对象阻断输出。 */
#define SET_UNBLOCKO (AC0 + 6) /* 设置对象非阻断输出。 */
#define CMD_RESET (AC0 + 7)    /* 重置。 */
#define CMD_PREPARE (AC0 + 8)  /* 准备。 */
#define AC0_ (AC0 + 9)	       /* <其它AC0行为的起点> */

/* AC1 行为：* */
#define GET_IQCTY (AC1 + 0) /* 获取iq容量。 */
#define GET_OQCTY (AC1 + 1) /* 获取oq容量。 */
#define AC1_ (AC1 + 2)	    /* <其它AC1行为的起点> */

/* AC2 行为：* */
#define SET_IQCTY (AC2 + 0)	 /* 设置iq容量。 */
#define SET_OQCTY (AC2 + 1)	 /* 设置oq容量。 */
#define CMD_INITIALIZE (AC2 + 2) /* 初始化 */
#define AC2_ (AC2 + 3)		 /* <其它AC2行为的起点> */

/* AC3 行为：* */

#define AC3_ (AC3 + 0) /* <其它AC3行为的起点> */

/*
 * 结果定义。
 * 备注：=== 定义模板 ===
 *       #define       RN_XXXXX            (RN_ - 1)
 *       #define       RN_XXXXX            (RN_ - 0)
 *       /-** N 结果（上）：**-/
 *       #define       RO_XXXXX           (RON_ - 1)
 *       #define       RO_XXXXX           (RON_ - 0)
 *       /-**
 *       **-/
 *       #define       RO_XXXXX           (ROP_ + 0)
 *       #define       RO_XXXXX           (ROP_ + 1)
 *       /-** P 结果（下）：**-/
 *       #define       RP_XXXXX            (RP_ + 0)
 *       #define       RP_XXXXX            (RP_ + 1)
 */

#define RN_ (R_N - 8)	     /* <其它N结果的起点> */
#define RN_LACKOID (R_N - 6) /* host_create_object，没有足够的oid供分配。 */
#define RN_CLOSEPARTIAL (R_N - 7) /* 部分基节点已关闭；仅可重试关闭。 */
#define RN_HASVR (R_N - 5) /* register_caller，已经有这个struct vr了。 */
#define RN_LOST (R_N - 4)  /* enqueue/dequeue，已丢失了Byte。 */
#define RN_UNSHARED (R_N - 3) /* open，对象是非共享的。 */
#define RN_FULL (R_N - 2) /* open，对象引用已满，不能再打开。 */
#define RN_INVALIDARG (R_N - 1) /* 无效参数 */
#define R_N NEOIOIC_RON		/* N结果（否定性结果）的起点。 */

#define NEOIOIC_RON (RON_ - 1000) /* O结果域的N界。 */

#define RON_ (R_O - 3)	    /* <其它负O结果的起点> */
#define RO_NOPEN (R_O - 2)  /* close，对象还没有被打开。 */
#define RO_VAINLY (R_O - 1) /* 空操作 */
#define R_O 0		    /* O结果（中立性结果）域的中点。 */
#define RO_OBSOIX (R_O + 1) /* OBS顺序非接力访问模式专用执行结果。 */
#define RO_SBOOIX (R_O + 2) /* SBO顺序非接力访问模式专用执行结果。 */
#define RO__SBOOIX (R_O + 3) /* SBO顺序接力访问模式专用执行结果。 */
#define ROP_ (R_O + 4)	     /* <其它正O结果的起点> */

#define NEOIOIC_ROP (ROP_ + 1000) /* O结果域的P界。 */

#define R_P NEOIOIC_ROP	    /* P结果（肯定性结果）的起点。 */
#define RP_RCZERO (R_P + 1) /* close，对象引用计数变为0。 */
#define RP_RCONE (R_P + 2)  /* open，对象引用计数为1。 */
#define RP_ (R_P + 3)	    /* <其它P结果的起点> */

/*
 * 结果的否定性/中立性/肯定性检测定义。
 * 备注：RN --- Negative Result；
 *       RO --- O  Result；
 *       RP --- Positive Result。
 */
#define RN(Result) ((int)(Result) <= NEOIOIC_RON)
#define RO(Result) (((int)(Result) > NEOIOIC_RON) && ((int)(Result) < NEOIOIC_ROP))
#define RP(Result) ((int)(Result) >= NEOIOIC_ROP)

/* 定义布尔值和空指针值。 */
#undef TRUE
#undef FALSE
#undef NULL

#define TRUE 1	/* 布尔类型数据值：真 */
#define FALSE 0 /* 布尔类型数据值：假 */
#define NULL 0	/* 指针类型数据值：空 */

/* NEOioic */

/*
 * 队列结构体
 * 备注：q_，pq_。
 */
struct queue {
	unsigned char *data;   /* Data Room，数据空间。 */
	unsigned int capacity; /* data的容量。 */
	unsigned char *front;  /* 当前第一个Byte数据的地址；空队列为NULL。 */
	unsigned char *rear;   /* 当前最后一个Byte数据的地址；空队列为NULL。 */
	unsigned int quantity; /* 当前data内存有的Byte数量。 */
	unsigned int lost;     /* 因data已满而丢弃的Byte数量。 */
};

/*
 * struct vr结构体。
 * 备注：vr_，pvr_。
 */
struct vr {
	uint64_t cr; /* CR的ID。 */
	uint64_t mr; /* MR的ID。 */
};

/* 节点通过 members 指向自己的 struct em_xxx 状态。 */

/*
 * NEOioic对象模型结构体。
 * 备注：原名“OIOIC”是5个通用接口（open，input，output，Interact，close）的第一个字符的合写。
 */
struct object {
	/* ref_count，对象引用计数（Reference Count）。 */
	unsigned int ref_count;

	/* model_id，模的SN。 */
	unsigned short model_id;

	/* oid，对象的ID。 */
	uint64_t oid;

	/*
	 * nodes：指向AND。
	 * node_count：the Number of ND of IC，IC的ND数量，也就是AND的元素数量。
	 */
	struct object *nodes;
	unsigned short node_count;

	/*
	 * direct_bases：指向AIB数组。
	 * direct_base_count：the Number of IB，IB数量，即AIB的元素数量。
	 */
	struct object **direct_bases;
	unsigned short direct_base_count;

	/*
	 * bases：指向ABN数组。
	 * base_count：the Number of BN，BN数量，即ABN的元素数量。
	 */
	struct object **bases;
	unsigned short base_count;

	/*
	 *  不可重入（Not Reentrant）访问规则，即同一MR的同一CR对每个BN只能访问一次。
	 * callers：指向ACS。
	 * caller_count：the Number of CS，CS的数量，即ACS的元素数量。
	 */
	struct vr *callers;
	unsigned short caller_count;

	/* 对象接口：open，input，output，io_put，interact0~3，close。* */

	/*
	 * 打开。
	 *  参数：caller_context --- [IN] 来访者。
	 */
	int (*open)(struct object *self, const struct vr *caller_context);
	/*
	 * 输入。
	 *  参数：input_stream --- [IN] 输入流；
	 *        quantity --- [IN] 输入的Byte数量；
	 *        caller_context --- [IN] 来访者。
	 */
	int (*input)(struct object *self, unsigned char *input_stream,
		     unsigned int quantity, const struct vr *caller_context);
	/*
	 * 输出。
	 *  参数：output_stream --- [OUT] 输出流的流向；
	 *        capacity --- [IN] output_stream所指存储空间的容量；
	 *        output_quantity --- [OUT] 指向实际输出的Byte数量；
	 *        caller_context --- [IN] 来访者。
	 */
	int (*output)(struct object *self, unsigned char *output_stream,
		      unsigned int capacity, unsigned int *output_quantity,
		      const struct vr *caller_context);
	/*
	 * 输入输出。
	 *  参数：input_stream --- [IN] 输入流；
	 *        quantity --- [IN] 输入的Byte数量；
	 *        output_stream --- [OUT] 输出流的流向；
	 *        capacity --- [IN] output_stream所指存储空间的容量；
	 *        output_quantity --- [OUT] 指向实际输出的Byte数量；
	 *        caller_context --- [IN] 来访者。
	 */
	int (*io_put)(struct object *self, unsigned char *input_stream,
		      unsigned int quantity, unsigned char *output_stream,
		      unsigned int capacity, unsigned int *output_quantity,
		      const struct vr *caller_context);
	/*
	 * 交互，接收AC0类型的行为。
	 *  参数：action --- [IN] 交互行为；
	 *        caller_context --- [IN] 来访者。
	 */
	int (*interact0)(struct object *self, unsigned int action,
			 const struct vr *caller_context);
	/*
	 * 交互，接收AC1类型的行为。
	 *  参数：action --- [IN] 交互行为；
	 *        output_stream --- [OUT] 输出流的流向；
	 *        capacity --- [IN] output_stream所指存储空间的容量；
	 *        output_quantity --- [OUT] 指向实际输出的Byte数量；
	 *        caller_context --- [IN] 来访者。
	 */
	int (*interact1)(struct object *self, unsigned int action,
			 unsigned char *output_stream, unsigned int capacity,
			 unsigned int *output_quantity,
			 const struct vr *caller_context);
	/*
	 * 交互，接收AC2类型的行为。
	 *  参数：action --- [IN] 交互行为；
	 *        input_stream --- [IN] 输入流；
	 *        quantity --- [IN] 输入的Byte数量；
	 *        caller_context --- [IN] 来访者。
	 */
	int (*interact2)(struct object *self, unsigned int action,
			 unsigned char *input_stream, unsigned int quantity,
			 const struct vr *caller_context);
	/*
	 * 交互，接收AC3类型的行为。
	 *  参数：action --- [IN] 交互行为；
	 *        input_stream --- [IN] 输入流；
	 *        quantity --- [IN] 输入的Byte数量；
	 *        output_stream --- [OUT] 输出流的流向；
	 *        capacity --- [IN] output_stream所指存储空间的容量；
	 *        output_quantity --- [OUT] 指向实际输出的Byte数量；
	 *        caller_context --- [IN] 来访者。
	 */
	int (*interact3)(struct object *self, unsigned int action,
			 unsigned char *input_stream, unsigned int quantity,
			 unsigned char *output_stream, unsigned int capacity,
			 unsigned int *output_quantity,
			 const struct vr *caller_context);
	/*
	 * 关闭。
	 *  参数：caller_context --- [IN] 来访者。
	 */
	int (*close)(struct object *self, const struct vr *caller_context);

	/* members，指向对象的void（扩展成员）。 */
	void *members;
};

/* Keep generated extended-member blocks aligned for subsequent nodes. */
#define NEOIOIC_ALIGNED_SIZE(Type)                        \
	((sizeof(Type) + _Alignof(struct object) - 1) / \
	 _Alignof(struct object) * _Alignof(struct object))

/* 宏封装单个Byte入列和出列 */

/*
 *  1. PLACE --- 放入模式，向有空闲空间的队列放入一个Byte；
 *  2. PUSH --- 推入模式，向已满（没有空闲空间）的队列推入一个Byte，把第一个Byte挤出扔掉。
 */

/*
 * 宏，向有空闲空间的队列放入一个Byte。
 * 参数：pQ --- [IN] struct queue类型指针，指向队列，这个队列必须是有空闲空间的；
 *       byte --- [IN] 入列的Byte。
 * 备注：只有在队列有空闲空间时才能使用。如下，
 * 	 if(pQ->quantity < pQ->capacity)
 * 	 {
 * 	  	ENQ_PLACE_BYTE(pQ, byte);
 * 	 }else
 * 	 {
 * 	 	pQ->lost++;
 *  		/-* ... *-/
 * 	 }
 */
#define ENQ_PLACE_BYTE(pQ, byte)                                   \
	do {                                                       \
		if ((pQ)->quantity == 0)                           \
			(pQ)->front = (pQ)->rear = (pQ)->data;     \
		else if ((pQ)->rear == (pQ)->data + (pQ)->capacity - 1) \
			(pQ)->rear = (pQ)->data;                   \
		else                                               \
			(pQ)->rear++;                              \
		*((pQ)->rear) = (byte);                            \
		(pQ)->quantity++;                                  \
	} while (0)

/*
 * 宏，向已满（没有空闲空间）的队列推入一个Byte，把第一个Byte挤出扔掉。
 * 参数：pQ --- [IN] struct queue类型指针，指向队列；
 *       byte --- [IN] 入列的Byte。
 * 备注：只有在队列已满时才能使用。如下，
 *   	 if(pQ->quantity < pQ->capacity)
 * 	 {
 * 		ENQ_PLACE_BYTE(pQ, byte);
 * 	 }else
 * 	 {
 * 	 	ENQ_PUSH_BYTE(pQ, byte);
 * 	 	pQ->lost++;
 * 		/-* ... *-/
 * 	 }
 */
#define ENQ_PUSH_BYTE(pQ, byte)                                       \
	do {                                                          \
		(pQ)->rear = (pQ)->front;                             \
		*((pQ)->rear) = (byte);                               \
		if ((pQ)->front == ((pQ)->data + (pQ)->capacity - 1)) \
			(pQ)->front = (pQ)->data;                     \
		else                                                  \
			(pQ)->front++;                                \
	} while (0)

/*
 * 宏，从非空的队列出列一个Byte。
 * 参数：pQ --- [IN] struct queue类型指针，指向队列；
 *       pByte --- [OUT] Byte地址，输出出列的Byte。
 * 备注：只有在队列非空时才能使用。如下，
 *   	 if(pQ->quantity > 0)
 * 	 {
 * 		DEQ_BYTE(pQ, pByte);
 * 	 }
 */
#define DEQ_BYTE(pQ, pByte)                                                \
	do {                                                               \
		*(pByte) = *((pQ)->front);                                 \
		(pQ)->quantity--;                                          \
		if (0 == (pQ)->quantity)                                   \
			(pQ)->front = (pQ)->rear = NULL;                   \
		else if ((pQ)->front == ((pQ)->data + (pQ)->capacity - 1)) \
			(pQ)->front = (pQ)->data;                          \
		else                                                       \
			(pQ)->front++;                                     \
	} while (0)

/* 内联VO函数 */

/* 注意：内联VO函数和非内联VO函数的功能是完全相同的。 */

#if INLINE_VOFUNCTION == 1

/*
 * 接口open的VO函数。
 * 参数：object --- [IN] 指向对象；
 *       caller_context --- [IN] 来访者。
 */
static NEOIOIC_INLINE int vo_open(struct object *object,
				const struct vr *caller_context)
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
static NEOIOIC_INLINE int vo_input(struct object *object,
				 unsigned char *input_stream,
				 unsigned int quantity,
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
static NEOIOIC_INLINE int vo_output(struct object *object,
				  unsigned char *output_stream,
				  unsigned int capacity,
				  unsigned int *output_quantity,
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
static NEOIOIC_INLINE int
vo_io_put(struct object *object, unsigned char *input_stream,
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
static NEOIOIC_INLINE int vo_interact0(struct object *object, unsigned int action,
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
static NEOIOIC_INLINE int vo_interact1(struct object *object, unsigned int action,
				     unsigned char *output_stream,
				     unsigned int capacity,
				     unsigned int *output_quantity,
				     const struct vr *caller_context)
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
static NEOIOIC_INLINE int vo_interact2(struct object *object, unsigned int action,
				     unsigned char *input_stream,
				     unsigned int quantity,
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
static NEOIOIC_INLINE int
vo_interact3(struct object *object, unsigned int action,
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
static NEOIOIC_INLINE int vo_close(struct object *object,
				 const struct vr *caller_context)
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

#endif /* #if INLINE_VOFUNCTION == 1 */

/* 继承链的四种访问模式 */

/*
 * 1. 继承链的两种访问顺序：OBS和SBO。
 * （1）OBS，others before self，先访问节点的IBN后访问节点自身；
 * （2）SBO，self before others，先访问节点自身后访问节点的IBN。
 * 2. 继承链的两种访问方式：接力和非接力访问。
 * （1）接力访问，Relay Visiting，访问上一个节点后，不管是什么执行结果接
 *      着访问下一节点。OBS返回LND的执行结果；SBO的返回结果分两种情况，
 *      单重继承链返回FND的执行结果，多重继承链返回RO__SBOOIX。
 * （2）相反就是非接力访问，Non-Relay Visiting，访问上一个节点后，如果不
 *      是RO_OBSOIX（对于OBS）或RO_SBOOIX（对于SBO）就返回执行结果，否则
 *      就访问下一个节点。
 * 3. 继承链的两种访问顺序和两种访问方式组合为继承链的四种访问模式。
 * （1）OBS_O_IX，OBS_OBJECT_<INTERFACE> 型的宏，按OBS顺序非接力访问；
 * （2）OBS_O_IX_，OBS_OBJECT_<INTERFACE>_ 型的宏，按OBS顺序接力访问；
 * （3）SBO_O_IX，SBO_OBJECT_<INTERFACE> 型的宏，按SBO顺序非接力访问；
 * （4）_SBO_O_IX，_SBO_OBJECT_<INTERFACE> 型的宏，按SBO顺序接力访问。
 * 注：1. open和close接口只能按OBS顺序接力访问，所以open和close接口没
 *        有OBS_O_IX、SBO_O_IX和_SBO_O_IX。
 *     2. 同一接口，四种访问模式只能选择其一，因此同一接口内不能同时存
 *        两种不同的访问模式。
 *     3. 接力访问一般适用于单重继承，对于多重继承意义不大。
 */

/* 宏，open接口的OBS_O_IX_。 */
#define OBS_OBJECT_OPEN_                                                               \
	do {                                                                           \
		int ir;                                                                \
		int fr;                                                                \
		unsigned short i;                                                      \
		struct vr Caller;                                                      \
		struct em_neoioic *members =                                             \
			(struct em_neoioic *)get_base_members(self, MSN_NEOIOIC);          \
		Caller.cr = caller_context->cr;                                        \
		Caller.mr = self->oid;                                                 \
		if (0 == ++self->ref_count) {                                          \
			self->ref_count--;                                             \
			return RN_FULL;                                                \
		}                                                                      \
		if (!(MOD_SHARED & members->mds) || 1 == self->ref_count) {            \
			extern int register_caller(                                    \
				struct object *node,                                   \
				const struct vr *caller_context);                      \
			for (i = 0; i < self->direct_base_count; i++) {                \
				fr = register_caller(                                  \
					*(self->direct_bases + i),                     \
					caller_context);                               \
				if (R_P == fr)                                         \
					ir = (*(self->direct_bases + i))               \
						     ->open(*(self->direct_bases +     \
							      i),                      \
							    caller_context);           \
				else if (RN_HASVR == fr)                               \
					continue;                                      \
				else                                                   \
					ir = R_N;                                      \
				if (RN(ir)) {                                          \
					unsigned short j;                              \
					int close_failed = 0;                          \
					for (j = 0; j < i; j++) {                      \
						if ((*(self->direct_bases +            \
						       j))                             \
							    ->ref_count ==             \
						    self->ref_count) {                 \
							if (RN(vo_close(               \
								*(self->direct_bases + \
								  j),                  \
								&Caller)))             \
								close_failed = 1;      \
						}                                      \
					}                                              \
					if (close_failed || ir == RN_CLOSEPARTIAL)     \
						return RN_CLOSEPARTIAL;                \
					self->ref_count--;                             \
					return ir;                                     \
				}                                                      \
			}                                                              \
		} else {                                                               \
			self->ref_count--;                                             \
			return RN_UNSHARED;                                            \
		}                                                                      \
	} while (0)

/* 宏，input接口的OBS_O_IX。 */
#define OBS_OBJECT_INPUT                                                     \
	do {                                                                 \
		int ir = RO_OBSOIX;                                          \
		int fr;                                                      \
		unsigned short i;                                            \
		for (i = 0; i < self->direct_base_count; i++) {              \
			extern int register_caller(                          \
				struct object *node,                         \
				const struct vr *caller_context);            \
			fr = register_caller(*(self->direct_bases + i),      \
					     caller_context);                \
			if (R_P == fr)                                       \
				ir = (*(self->direct_bases + i))             \
					     ->input(*(self->direct_bases +  \
						       i),                   \
						     input_stream, quantity, \
						     caller_context);        \
			else if (RN_HASVR == fr)                             \
				continue;                                    \
			else                                                 \
				ir = R_N;                                    \
			if (ir != RO_OBSOIX)                                 \
				return ir;                                   \
		}                                                            \
	} while (0)

/* 宏，input接口的OBS_O_IX_。 */
#define OBS_OBJECT_INPUT_                                                     \
	do {                                                                  \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			if (R_P == register_caller(*(self->direct_bases + i), \
						   caller_context))           \
				(*(self->direct_bases + i))                   \
					->input(*(self->direct_bases + i),    \
						input_stream, quantity,       \
						caller_context);              \
		}                                                             \
	} while (0)

/* 宏，input接口的SBO_O_IX。 */
#define SBO_OBJECT_INPUT                                                     \
	do {                                                                 \
		int ir = RO_SBOOIX;                                          \
		int fr;                                                      \
		unsigned short i;                                            \
		for (i = 0; i < self->direct_base_count; i++) {              \
			extern int register_caller(                          \
				struct object *node,                         \
				const struct vr *caller_context);            \
			fr = register_caller(*(self->direct_bases + i),      \
					     caller_context);                \
			if (R_P == fr)                                       \
				ir = (*(self->direct_bases + i))             \
					     ->input(*(self->direct_bases +  \
						       i),                   \
						     input_stream, quantity, \
						     caller_context);        \
			else if (RN_HASVR == fr)                             \
				continue;                                    \
			else                                                 \
				ir = R_N;                                    \
			if (ir != RO_SBOOIX)                                 \
				return ir;                                   \
		}                                                            \
		return ir;                                                   \
	} while (0)

/* 宏，input接口的_SBO_O_IX。 */
#define _SBO_OBJECT_INPUT                                                     \
	do {                                                                  \
		int ir = RO__SBOOIX;                                          \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			if (R_P == register_caller(*(self->direct_bases + i), \
						   caller_context))           \
				ir = (*(self->direct_bases + i))              \
					     ->input(*(self->direct_bases +   \
						       i),                    \
						     input_stream, quantity,  \
						     caller_context);         \
		}                                                             \
		return self->direct_base_count > 1 ? RO__SBOOIX : ir;         \
	} while (0)

/* 宏，output接口的OBS_O_IX。 */
#define OBS_OBJECT_OUTPUT                                                      \
	do {                                                                   \
		int ir = RO_OBSOIX;                                            \
		int fr;                                                        \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			fr = register_caller(*(self->direct_bases + i),        \
					     caller_context);                  \
			if (R_P == fr)                                         \
				ir = (*(self->direct_bases + i))               \
					     ->output(*(self->direct_bases +   \
							i),                    \
						      output_stream, capacity, \
						      output_quantity,         \
						      caller_context);         \
			else if (RN_HASVR == fr)                               \
				continue;                                      \
			else                                                   \
				ir = R_N;                                      \
			if (ir != RO_OBSOIX)                                   \
				return ir;                                     \
		}                                                              \
	} while (0)

/* 宏，output接口的OBS_O_IX_。 */
#define OBS_OBJECT_OUTPUT_                                                    \
	do {                                                                  \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			if (R_P == register_caller(*(self->direct_bases + i), \
						   caller_context))           \
				(*(self->direct_bases + i))                   \
					->output(*(self->direct_bases + i),   \
						 output_stream, capacity,     \
						 output_quantity,             \
						 caller_context);             \
		}                                                             \
	} while (0)

/* 宏，output接口的SBO_O_IX。 */
#define SBO_OBJECT_OUTPUT                                                      \
	do {                                                                   \
		int ir = RO_SBOOIX;                                            \
		int fr;                                                        \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			fr = register_caller(*(self->direct_bases + i),        \
					     caller_context);                  \
			if (R_P == fr)                                         \
				ir = (*(self->direct_bases + i))               \
					     ->output(*(self->direct_bases +   \
							i),                    \
						      output_stream, capacity, \
						      output_quantity,         \
						      caller_context);         \
			else if (RN_HASVR == fr)                               \
				continue;                                      \
			else                                                   \
				ir = R_N;                                      \
			if (ir != RO_SBOOIX)                                   \
				return ir;                                     \
		}                                                              \
		return ir;                                                     \
	} while (0)

/* 宏，output接口的_SBO_O_IX。 */
#define _SBO_OBJECT_OUTPUT                                                     \
	do {                                                                   \
		int ir = RO__SBOOIX;                                           \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			if (R_P == register_caller(*(self->direct_bases + i),  \
						   caller_context))            \
				ir = (*(self->direct_bases + i))               \
					     ->output(*(self->direct_bases +   \
							i),                    \
						      output_stream, capacity, \
						      output_quantity,         \
						      caller_context);         \
		}                                                              \
		return self->direct_base_count > 1 ? RO__SBOOIX : ir;          \
	} while (0)

/* 宏，io_put接口的OBS_O_IX。 */
#define OBS_OBJECT_IOPUT                                                       \
	do {                                                                   \
		int ir = RO_OBSOIX;                                            \
		int fr;                                                        \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			fr = register_caller(*(self->direct_bases + i),        \
					     caller_context);                  \
			if (R_P == fr)                                         \
				ir = (*(self->direct_bases + i))               \
					     ->io_put(*(self->direct_bases +   \
							i),                    \
						      input_stream, quantity,  \
						      output_stream, capacity, \
						      output_quantity,         \
						      caller_context);         \
			else if (RN_HASVR == fr)                               \
				continue;                                      \
			else                                                   \
				ir = R_N;                                      \
			if (ir != RO_OBSOIX)                                   \
				return ir;                                     \
		}                                                              \
	} while (0)

/* 宏，io_put接口的OBS_O_IX_。 */
#define OBS_OBJECT_IOPUT_                                                     \
	do {                                                                  \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			if (R_P == register_caller(*(self->direct_bases + i), \
						   caller_context))           \
				(*(self->direct_bases + i))                   \
					->io_put(*(self->direct_bases + i),   \
						 input_stream, quantity,      \
						 output_stream, capacity,     \
						 output_quantity,             \
						 caller_context);             \
		}                                                             \
	} while (0)

/* 宏，io_put接口的SBO_O_IX。 */
#define SBO_OBJECT_IOPUT                                                       \
	do {                                                                   \
		int ir = RO_SBOOIX;                                            \
		int fr;                                                        \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			fr = register_caller(*(self->direct_bases + i),        \
					     caller_context);                  \
			if (R_P == fr)                                         \
				ir = (*(self->direct_bases + i))               \
					     ->io_put(*(self->direct_bases +   \
							i),                    \
						      input_stream, quantity,  \
						      output_stream, capacity, \
						      output_quantity,         \
						      caller_context);         \
			else if (RN_HASVR == fr)                               \
				continue;                                      \
			else                                                   \
				ir = R_N;                                      \
			if (ir != RO_SBOOIX)                                   \
				return ir;                                     \
		}                                                              \
		return ir;                                                     \
	} while (0)

/* 宏，io_put接口的_SBO_O_IX。 */
#define _SBO_OBJECT_IOPUT                                                      \
	do {                                                                   \
		int ir = RO__SBOOIX;                                           \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			if (R_P == register_caller(*(self->direct_bases + i),  \
						   caller_context))            \
				ir = (*(self->direct_bases + i))               \
					     ->io_put(*(self->direct_bases +   \
							i),                    \
						      input_stream, quantity,  \
						      output_stream, capacity, \
						      output_quantity,         \
						      caller_context);         \
		}                                                              \
		return self->direct_base_count > 1 ? RO__SBOOIX : ir;          \
	} while (0)

/* 宏，interact0接口的OBS_O_IX。 */
#define OBS_OBJECT_INTERACT0                                                  \
	do {                                                                  \
		int ir = RO_OBSOIX;                                           \
		int fr;                                                       \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			fr = register_caller(*(self->direct_bases + i),       \
					     caller_context);                 \
			if (R_P == fr)                                        \
				ir = (*(self->direct_bases + i))              \
					     ->interact0(                     \
						     *(self->direct_bases +   \
						       i),                    \
						     action, caller_context); \
			else if (RN_HASVR == fr)                              \
				continue;                                     \
			else                                                  \
				ir = R_N;                                     \
			if (ir != RO_OBSOIX)                                  \
				return ir;                                    \
		}                                                             \
	} while (0)

/* 宏，interact0接口的OBS_O_IX_。 */
#define OBS_OBJECT_INTERACT0_                                                  \
	do {                                                                   \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			if (R_P == register_caller(*(self->direct_bases + i),  \
						   caller_context))            \
				(*(self->direct_bases + i))                    \
					->interact0(*(self->direct_bases + i), \
						    action, caller_context);   \
		}                                                              \
	} while (0)

/* 宏，interact0接口的SBO_O_IX。 */
#define SBO_OBJECT_INTERACT0                                                  \
	do {                                                                  \
		int ir = RO_SBOOIX;                                           \
		int fr;                                                       \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			fr = register_caller(*(self->direct_bases + i),       \
					     caller_context);                 \
			if (R_P == fr)                                        \
				ir = (*(self->direct_bases + i))              \
					     ->interact0(                     \
						     *(self->direct_bases +   \
						       i),                    \
						     action, caller_context); \
			else if (RN_HASVR == fr)                              \
				continue;                                     \
			else                                                  \
				ir = R_N;                                     \
			if (ir != RO_SBOOIX)                                  \
				return ir;                                    \
		}                                                             \
		return ir;                                                    \
	} while (0)

/* 宏，interact0接口的_SBO_O_IX。 */
#define _SBO_OBJECT_INTERACT0                                                  \
	do {                                                                   \
		int ir = RO__SBOOIX;                                           \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			if (R_P == register_caller(*(self->direct_bases + i),  \
						   caller_context))            \
				ir = (*(self->direct_bases + i))               \
					->interact0(*(self->direct_bases + i), \
						    action, caller_context);   \
		}                                                              \
		return self->direct_base_count > 1 ? RO__SBOOIX : ir;          \
	} while (0)

/* 宏，interact1接口的OBS_O_IX。 */
#define OBS_OBJECT_INTERACT1                                                \
	do {                                                                \
		int ir = RO_OBSOIX;                                         \
		int fr;                                                     \
		unsigned short i;                                           \
		for (i = 0; i < self->direct_base_count; i++) {             \
			extern int register_caller(                         \
				struct object *node,                        \
				const struct vr *caller_context);           \
			fr = register_caller(*(self->direct_bases + i),     \
					     caller_context);               \
			if (R_P == fr)                                      \
				ir = (*(self->direct_bases + i))            \
					     ->interact1(                   \
						     *(self->direct_bases + \
						       i),                  \
						     action, output_stream, \
						     capacity,              \
						     output_quantity,       \
						     caller_context);       \
			else if (RN_HASVR == fr)                            \
				continue;                                   \
			else                                                \
				ir = R_N;                                   \
			if (ir != RO_OBSOIX)                                \
				return ir;                                  \
		}                                                           \
	} while (0)

/* 宏，interact1接口的OBS_O_IX_。 */
#define OBS_OBJECT_INTERACT1_                                                  \
	do {                                                                   \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			if (R_P == register_caller(*(self->direct_bases + i),  \
						   caller_context))            \
				(*(self->direct_bases + i))                    \
					->interact1(*(self->direct_bases + i), \
						    action, output_stream,     \
						    capacity, output_quantity, \
						    caller_context);           \
		}                                                              \
	} while (0)

/* 宏，interact1接口的SBO_O_IX。 */
#define SBO_OBJECT_INTERACT1                                                \
	do {                                                                \
		int ir = RO_SBOOIX;                                         \
		int fr;                                                     \
		unsigned short i;                                           \
		for (i = 0; i < self->direct_base_count; i++) {             \
			extern int register_caller(                         \
				struct object *node,                        \
				const struct vr *caller_context);           \
			fr = register_caller(*(self->direct_bases + i),     \
					     caller_context);               \
			if (R_P == fr)                                      \
				ir = (*(self->direct_bases + i))            \
					     ->interact1(                   \
						     *(self->direct_bases + \
						       i),                  \
						     action, output_stream, \
						     capacity,              \
						     output_quantity,       \
						     caller_context);       \
			else if (RN_HASVR == fr)                            \
				continue;                                   \
			else                                                \
				ir = R_N;                                   \
			if (ir != RO_SBOOIX)                                \
				return ir;                                  \
		}                                                           \
		return ir;                                                  \
	} while (0)

/* 宏，interact1接口的_SBO_O_IX。 */
#define _SBO_OBJECT_INTERACT1                                                 \
	do {                                                                  \
		int ir = RO__SBOOIX;                                          \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			if (R_P == register_caller(*(self->direct_bases + i), \
						   caller_context))           \
				ir = (*(self->direct_bases + i))              \
					     ->interact1(                     \
						     *(self->direct_bases +   \
						       i),                    \
						     action, output_stream,   \
						     capacity,                \
						     output_quantity,         \
						     caller_context);         \
		}                                                             \
		return self->direct_base_count > 1 ? RO__SBOOIX : ir;         \
	} while (0)

/* 宏，interact2接口的OBS_O_IX。 */
#define OBS_OBJECT_INTERACT2                                                \
	do {                                                                \
		int ir = RO_OBSOIX;                                         \
		int fr;                                                     \
		unsigned short i;                                           \
		for (i = 0; i < self->direct_base_count; i++) {             \
			extern int register_caller(                         \
				struct object *node,                        \
				const struct vr *caller_context);           \
			fr = register_caller(*(self->direct_bases + i),     \
					     caller_context);               \
			if (R_P == fr)                                      \
				ir = (*(self->direct_bases + i))            \
					     ->interact2(                   \
						     *(self->direct_bases + \
						       i),                  \
						     action, input_stream,  \
						     quantity,              \
						     caller_context);       \
			else if (RN_HASVR == fr)                            \
				continue;                                   \
			else                                                \
				ir = R_N;                                   \
			if (ir != RO_OBSOIX)                                \
				return ir;                                  \
		}                                                           \
	} while (0)

/* 宏，interact2接口的OBS_O_IX_。 */
#define OBS_OBJECT_INTERACT2_                                                  \
	do {                                                                   \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			if (R_P == register_caller(*(self->direct_bases + i),  \
						   caller_context))            \
				(*(self->direct_bases + i))                    \
					->interact2(*(self->direct_bases + i), \
						    action, input_stream,      \
						    quantity, caller_context); \
		}                                                              \
	} while (0)

/* 宏，interact2接口的SBO_O_IX。 */
#define SBO_OBJECT_INTERACT2                                                \
	do {                                                                \
		int ir = RO_SBOOIX;                                         \
		int fr;                                                     \
		unsigned short i;                                           \
		for (i = 0; i < self->direct_base_count; i++) {             \
			extern int register_caller(                         \
				struct object *node,                        \
				const struct vr *caller_context);           \
			fr = register_caller(*(self->direct_bases + i),     \
					     caller_context);               \
			if (R_P == fr)                                      \
				ir = (*(self->direct_bases + i))            \
					     ->interact2(                   \
						     *(self->direct_bases + \
						       i),                  \
						     action, input_stream,  \
						     quantity,              \
						     caller_context);       \
			else if (RN_HASVR == fr)                            \
				continue;                                   \
			else                                                \
				ir = R_N;                                   \
			if (ir != RO_SBOOIX)                                \
				return ir;                                  \
		}                                                           \
		return ir;                                                  \
	} while (0)

/* 宏，interact2接口的_SBO_O_IX。 */
#define _SBO_OBJECT_INTERACT2                                                 \
	do {                                                                  \
		int ir = RO__SBOOIX;                                          \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			if (R_P == register_caller(*(self->direct_bases + i), \
						   caller_context))           \
				ir = (*(self->direct_bases + i))              \
					     ->interact2(                     \
						     *(self->direct_bases +   \
						       i),                    \
						     action, input_stream,    \
						     quantity,                \
						     caller_context);         \
		}                                                             \
		return self->direct_base_count > 1 ? RO__SBOOIX : ir;         \
	} while (0)

/* 宏，interact3接口的OBS_O_IX。 */
#define OBS_OBJECT_INTERACT3                                                  \
	do {                                                                  \
		int ir = RO_OBSOIX;                                           \
		int fr;                                                       \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			fr = register_caller(*(self->direct_bases + i),       \
					     caller_context);                 \
			if (R_P == fr)                                        \
				ir = (*(self->direct_bases + i))              \
					     ->interact3(                     \
						     *(self->direct_bases +   \
						       i),                    \
						     action, input_stream,    \
						     quantity, output_stream, \
						     capacity,                \
						     output_quantity,         \
						     caller_context);         \
			else if (RN_HASVR == fr)                              \
				continue;                                     \
			else                                                  \
				ir = R_N;                                     \
			if (ir != RO_OBSOIX)                                  \
				return ir;                                    \
		}                                                             \
	} while (0)

/* 宏，interact3接口的OBS_O_IX_。 */
#define OBS_OBJECT_INTERACT3_                                                  \
	do {                                                                   \
		unsigned short i;                                              \
		for (i = 0; i < self->direct_base_count; i++) {                \
			extern int register_caller(                            \
				struct object *node,                           \
				const struct vr *caller_context);              \
			if (R_P == register_caller(*(self->direct_bases + i),  \
						   caller_context))            \
				(*(self->direct_bases + i))                    \
					->interact3(*(self->direct_bases + i), \
						    action, input_stream,      \
						    quantity, output_stream,   \
						    capacity, output_quantity, \
						    caller_context);           \
		}                                                              \
	} while (0)

/* 宏，interact3接口的SBO_O_IX。 */
#define SBO_OBJECT_INTERACT3                                                  \
	do {                                                                  \
		int ir = RO_SBOOIX;                                           \
		int fr;                                                       \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			fr = register_caller(*(self->direct_bases + i),       \
					     caller_context);                 \
			if (R_P == fr)                                        \
				ir = (*(self->direct_bases + i))              \
					     ->interact3(                     \
						     *(self->direct_bases +   \
						       i),                    \
						     action, input_stream,    \
						     quantity, output_stream, \
						     capacity,                \
						     output_quantity,         \
						     caller_context);         \
			else if (RN_HASVR == fr)                              \
				continue;                                     \
			else                                                  \
				ir = R_N;                                     \
			if (ir != RO_SBOOIX)                                  \
				return ir;                                    \
		}                                                             \
		return ir;                                                    \
	} while (0)

/* 宏，interact3接口的_SBO_O_IX。 */
#define _SBO_OBJECT_INTERACT3                                                 \
	do {                                                                  \
		int ir = RO__SBOOIX;                                          \
		unsigned short i;                                             \
		for (i = 0; i < self->direct_base_count; i++) {               \
			extern int register_caller(                           \
				struct object *node,                          \
				const struct vr *caller_context);             \
			if (R_P == register_caller(*(self->direct_bases + i), \
						   caller_context))           \
				ir = (*(self->direct_bases + i))              \
					     ->interact3(                     \
						     *(self->direct_bases +   \
						       i),                    \
						     action, input_stream,    \
						     quantity, output_stream, \
						     capacity,                \
						     output_quantity,         \
						     caller_context);         \
		}                                                             \
		return self->direct_base_count > 1 ? RO__SBOOIX : ir;         \
	} while (0)

/*
 * 宏，close接口的OBS_O_IX_。
 * RN_CLOSEPARTIAL表示基节点已部分关闭，保留自身引用供重试清理，非状态回滚。
 * 返回此结果后不得继续业务访问或打开，只能排除失败原因后重试vo_close。
 */
#define OBS_OBJECT_CLOSE_                                                           \
	do {                                                                        \
		int ir;                                                             \
		int fr;                                                             \
		int closed = 0;                                                     \
		unsigned short i;                                                   \
		if (0 == self->ref_count)                                           \
			return RO_NOPEN;                                            \
		self->ref_count--;                                                  \
		for (i = 0; i < self->direct_base_count; i++) {                     \
			extern int register_caller(                                 \
				struct object *node,                                \
				const struct vr *caller_context);                   \
			if ((*(self->direct_bases + i))->ref_count <                \
			    self->ref_count + 1)                                    \
				closed = 1;                                         \
			if ((*(self->direct_bases + i))->ref_count ==               \
			    self->ref_count + 1) {                                  \
				fr = register_caller(                               \
					*(self->direct_bases + i),                  \
					caller_context);                            \
				if (R_P == fr)                                      \
					ir = (*(self->direct_bases + i))            \
						     ->close(*(self->direct_bases + \
							       i),                  \
							     caller_context);       \
				else if (RN_HASVR == fr)                            \
					continue;                                   \
				else                                                \
					ir = R_N;                                   \
				if (RN(ir)) {                                       \
					self->ref_count++;                          \
					return closed || ir == RN_CLOSEPARTIAL ?    \
						RN_CLOSEPARTIAL : ir;               \
				}                                                   \
				closed = 1;                                         \
			}                                                           \
		}                                                                   \
	} while (0)

/* create_object & destroy_object */

/*
 * 宏，动态创建对象。
 * 参数：model_id --- [IN] 模序号，指定创建哪个模的对象。
 *       input_capacity --- [IN] 输入队列的容量。
 *       output_capacity --- [IN] 输出队列的容量。
 *       caller_capacity --- [IN] 来访者数量。
 *       object_out --- [OUT] 输出创建的对象。
 */
extern int host_create_object(unsigned short model_id,
			      unsigned int input_capacity,
			      unsigned int output_capacity, int caller_capacity,
			      struct object **object_out,
			      unsigned char *exotic_memory);
#define create_object(model_id, input_capacity, output_capacity,      \
		      caller_capacity, object_out)                    \
	host_create_object(model_id, input_capacity, output_capacity, \
			   caller_capacity, object_out, NULL)

/*
 * 宏，销毁动态创建的对象，即销毁由create_object宏创建的对象。
 * 参数：object --- [IN] 指向对象。
 */
extern char host_destroy_object(struct object *object);
#define destroy_object(object)                             \
	do {                                               \
		if (TRUE == host_destroy_object(object)) { \
			free(object);                      \
			(object) = NULL;                   \
		}                                          \
	} while (0)

/* 向外声明外面直接调用的函数 */

extern int enqueue(struct queue *pq, unsigned char *input_data,
		   unsigned int quantity);
extern int dequeue(struct queue *pq, unsigned char *output_data,
		   unsigned int capacity, unsigned int *output_quantity);
extern struct object *get_base_node(struct object *node,
				    unsigned short model_id);
extern void *get_base_members(struct object *node, unsigned short model_id);
extern struct queue *get_iq(struct object *node);
extern struct queue *get_oq(struct object *node);

#if INLINE_VOFUNCTION == 0

extern int vo_open(struct object *object, const struct vr *caller_context);
extern int vo_input(struct object *object, unsigned char *input_stream,
		    unsigned int quantity, const struct vr *caller_context);
extern int vo_output(struct object *object, unsigned char *output_stream,
		     unsigned int capacity, unsigned int *output_quantity,
		     const struct vr *caller_context);
extern int vo_io_put(struct object *object, unsigned char *input_stream,
		     unsigned int quantity, unsigned char *output_stream,
		     unsigned int capacity, unsigned int *output_quantity,
		     const struct vr *caller_context);
extern int vo_interact0(struct object *object, unsigned int action,
			const struct vr *caller_context);
extern int vo_interact1(struct object *object, unsigned int action,
			unsigned char *output_stream, unsigned int capacity,
			unsigned int *output_quantity,
			const struct vr *caller_context);
extern int vo_interact2(struct object *object, unsigned int action,
			unsigned char *input_stream, unsigned int quantity,
			const struct vr *caller_context);
extern int vo_interact3(struct object *object, unsigned int action,
			unsigned char *input_stream, unsigned int quantity,
			unsigned char *output_stream, unsigned int capacity,
			unsigned int *output_quantity,
			const struct vr *caller_context);
extern int vo_close(struct object *object, const struct vr *caller_context);

#endif

#endif /* #ifndef NEOIOIC_OBJECT_H */
