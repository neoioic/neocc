

#ifndef NEOIOIC_HO_HELLOWORLD_H
#define NEOIOIC_HO_HELLOWORLD_H

#include "neoioic.h"

/* oid(s) and OID_ */

/*
 *  注意：在目前操作系统下，尽管大多情况不需要创建HO和AO，但它
 *  们的oid是必需的！不需要创建的HO或AO就在这里为它们定义oid。
 */

#define OID_HO (0x0000000000010000 | OID_OS) /* HO的oid。 */
/* #define   OID_XXX1      (OID_HO + 0) /-* xxx1的oid。HO *-/ */
/* #define   OID_XXX2      (OID_HO + 1) /-* xxx2的oid。AO *-/ */
/* #define   OID_XXX3      (OID_HO + 2) /-* xxx3的oid。.. *-/ */

#define OID_ (OID_HO + 0) /* 其它oid的起点。 */

/* model_id */

/* #define   MSN_XXX1      (MSN_ + 0)  /-* xxx1的model_id。HO *-/ */
/* #define   MSN_XXX2      (MSN_ + 1)  /-* xxx2的model_id。AO *-/ */
/* #define   MSN_XXX3      (MSN_ + 2)  /-* xxx3的model_id。.. *-/ */
/* #define   MSN_XXX4      (MSN_ + 3)  /-* xxx4的model_id。GO *-/ */
/* #define   MSN_XXX5      (MSN_ + 4)  /-* xxx5的model_id。.. *-/ */

/* Action */

/* 非一个对象模专用的交互行为：* */
/* #define     XXX_YYY        (ACx | (ACT_PVT_  + 0)) */

/* #define    ACT_PVT_XXX_         (ACT_PVT_ + x)  /-* X1的私有交互行为起点。*-/ */

/* X1对象模的私有交互行为：* */
/* #define     XXX_YYY        (ACx | (ACT_PVT_XXX_  + 0)) */

/* #define    ACT_PVT_YYY_         (ACT_PVT_XXX_ + x)  /-* X2的私有交互行为起点。*-/ */

#endif /* #ifndef NEOIOIC_HO_HELLOWORLD_H */
