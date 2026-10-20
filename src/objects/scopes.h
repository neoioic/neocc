#ifndef NEOCC_SCOPES_OBJECT_H
#define NEOCC_SCOPES_OBJECT_H

#include "neocc/scopes.h"

struct neocc_scope_frame {
	struct neocc_scope_frame *next;
	struct neocc_scope scope;
};

struct em_scopes {
	struct neocc_scopes state;
	struct neocc_scope_frame *active, *available;
};

#endif
