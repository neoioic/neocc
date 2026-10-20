#ifndef NEOCC_HOST_H
#define NEOCC_HOST_H

#include "object.h"

#define NEOCC_MSN_SOURCE MSN_
/* OpenOICC's generated constructor model-name convention. */
#define MSN_SOURCE NEOCC_MSN_SOURCE
#define NEOCC_MSN_TOKENS (MSN_ + 1)
#define MSN_TOKENS NEOCC_MSN_TOKENS
#define NEOCC_MSN_PREPROCESSOR (MSN_ + 2)
#define MSN_PREPROCESSOR NEOCC_MSN_PREPROCESSOR
#define NEOCC_MSN_SYMBOLS (MSN_ + 3)
#define MSN_SYMBOLS NEOCC_MSN_SYMBOLS
#define NEOCC_MSN_EXPRESSIONS (MSN_ + 4)
#define MSN_EXPRESSIONS NEOCC_MSN_EXPRESSIONS
#define NEOCC_OID_HOST (OID_OS | UINT64_C(0x004e0000))

extern const struct vr neocc_host_caller;

#endif
