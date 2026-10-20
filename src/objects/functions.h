/* Temporary-slot layout migrated from TinyCC tccgen.c, LGPL-2.0-or-later.
 * Copyright (c) 2001-2004 Fabrice Bellard.
 * NeoCC changes: Copyright (C) 2026 CLimber-Rong, LGPL-2.1-or-later.
 */
#ifndef NEOCC_FUNCTIONS_OBJECT_H
#define NEOCC_FUNCTIONS_OBJECT_H

#include "neocc/functions.h"

struct neocc_function_temp {
	int location;
	short size, alignment;
};

struct em_functions {
	struct neocc_function state;
	struct neocc_function_temp temps[NEOCC_FUNCTION_TEMP_CAPACITY];
	int temp_count;
};

#endif
