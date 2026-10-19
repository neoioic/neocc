/*
 * NEOioic 节点的 .h 文件。
 * 创 建 者：赵平智   <bruce.zhao.zpz@gmail.com>
 * 版权(C)2005-2011 赵平智。保留所有权利。
 */

#ifndef NEOIOIC_NEOIOIC_H
#define NEOIOIC_NEOIOIC_H

#include "object.h"

/* INIT */

/* Extended Member */

struct em_neoioic {
	struct queue iq;  /* 输入队列。 */
	struct queue oq;  /* 输出队列。 */
	unsigned int mds; /* 模式集。 */
};

#endif /* #ifndef NEOIOIC_NEOIOIC_H */
