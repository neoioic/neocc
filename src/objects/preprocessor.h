#ifndef NEOCC_PREPROCESSOR_OBJECT_H
#define NEOCC_PREPROCESSOR_OBJECT_H

#include "neocc/preprocessor.h"

struct em_preprocessor {
	/* Separate allocation honors the host long-double alignment. */
	struct neocc_preprocessor *state;
	struct neocc_cached_include *includes[NEOCC_INCLUDE_HASH_CAPACITY];
	struct neocc_preprocessor_results results;
};

#endif
