#ifndef NEOCC_HOST_H
#define NEOCC_HOST_H

#include "object.h"

#define NEOCC_MSN_SOURCE MSN_
/* OpenOICC's generated constructor model-name convention. */
#define MSN_SOURCE NEOCC_MSN_SOURCE
#define NEOCC_MSN_TOKENS (MSN_ + 1)
#define MSN_TOKENS NEOCC_MSN_TOKENS
#define NEOCC_OID_HOST (OID_OS | UINT64_C(0x004e0000))

extern const struct vr neocc_host_caller;

#endif
