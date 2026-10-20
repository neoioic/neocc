#ifndef NEOCC_EXPRESSIONS_OBJECT_H
#define NEOCC_EXPRESSIONS_OBJECT_H

#include "neocc/expressions.h"

struct em_expressions {
	/* Independently allocated for the host long-double alignment. */
	struct neocc_expressions *state;
};

#endif
